/*********************                                                        */
/*! \file  term_tuple_enumerator.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Mikolas Janota
 ** This file is part of the CVC4 project.
 ** Copyright (c) 2009-2020 by the authors listed in the file AUTHORS
 ** in the top-level source directory and their institutional affiliations.
 ** All rights reserved.  See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief Implementation of an enumeration of tuples of terms for the purpose
 *of quantifier instantiation.
 **/
#include "theory/quantifiers/term_tuple_enumerator.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <numeric>
#include <vector>

#include "base/map_util.h"
#include "base/output.h"
#include "smt/smt_statistics_registry.h"
#include "theory/quantifiers/index_trie.h"
#include "theory/quantifiers/quant_module.h"
#include "theory/quantifiers/term_util.h"
#include "theory/quantifiers_engine.h"
#include "util/statistics_registry.h"
#include "theory/quantifiers/relevant_domain.h"

namespace CVC4 {

template <typename T>
static CVC4ostream& operator<<(CVC4ostream& out, const std::vector<T>& v)
{
  out << "[ ";
  std::copy(v.begin(), v.end(), std::ostream_iterator<T>(out, " "));
  return out << "]";
}

/** Tracing purposes, printing a masked vector of indices. */
static void traceMaskedVector(const char* trace,
                              const char* name,
                              const std::vector<bool>& mask,
                              const std::vector<size_t>& values)
{
  Assert(mask.size() == values.size());
  Trace(trace) << name << " [ ";
  for (size_t variableIx = 0; variableIx < mask.size(); variableIx++)
  {
    if (mask[variableIx])
    {
      Trace(trace) << values[variableIx] << " ";
    }
    else
    {
      Trace(trace) << "_ ";
    }
  }
  Trace(trace) << "]" << std::endl;
}

namespace theory {
namespace quantifiers {
/**
 * Base class for enumerators of tuples of terms for the purpose of
 * quantification instantiation. The tuples are represented as tuples of
 * indices of  terms, where the tuple has as many elements as there are
 * quantified variables in the considered quantifier.
 *
 * Like so, we see a tuple as a number, where the digits may have different
 * ranges. The most significant digits are stored first.
 *
 * Tuples are enumerated  in a lexicographic order in stages. There are 2
 * possible strategies, either  all tuples in a given stage have the same sum of
 * digits, or, the maximum  over these digits is the same.
 * */
class TermTupleEnumeratorBase : public TermTupleEnumeratorInterface
{
 public:
  /** Initialize the class with the quantifier to be instantiated. */
  TermTupleEnumeratorBase(Node quantifier,
                          bool fullEffort,
                          bool increaseSum,
                          TermTupleEnumeratorContext* context)
      : d_quantifier(quantifier),
        d_fullEffort(fullEffort),
        d_increaseSum(increaseSum),
        d_variableCount(d_quantifier[0].getNumChildren()),
        d_context(context),
        d_stepCounter(0),
        d_disabledCombinations(
            true)  // do not record combinations with no blanks
  {
    d_changePrefix = d_variableCount;
  }

  virtual ~TermTupleEnumeratorBase() = default;

  // implementation of the TermTupleEnumeratorInterface
  virtual void init() override;
  virtual bool hasNext() override;
  virtual void next(/*out*/ std::vector<Node>& terms) override;
  virtual void failureReason(const std::vector<bool>& mask) override;
  // end of implementation of the TermTupleEnumeratorInterface

 protected:
  /** the quantifier whose variables are being instantiated */
  const Node d_quantifier;
  /** number of variables in the quantifier */
  const size_t d_variableCount;
  /** context of structures with a longer lifespan */
  const TermTupleEnumeratorContext* const d_context;
  /** type for each variable */
  std::vector<TypeNode> d_typeCache;
  /** number of candidate terms for each variable */
  std::vector<size_t> d_termsSizes;
  /** tuple of indices of the current terms */
  std::vector<size_t> d_termIndex;
  /** total number of steps of the enumerator */
  uint32_t d_stepCounter;

  /** a data structure storing disabled combinations of terms */
  IndexTrie d_disabledCombinations;

  /** current sum/max  of digits, depending on the strategy */
  size_t d_currentStage;
  /**total number of stages*/
  size_t d_stageCount;
  /**becomes false once the enumerator runs out of options*/
  bool d_hasNext;
  std::vector<std::vector<size_t> > d_termPermutations;
  /* Allow larger indices from now on */
  /** the length of the prefix that has to be changed in the next
  combination, i.e.  the number of the most significant digits that need to be
  changed in order to escape a  useless instantiation */
  size_t d_changePrefix;
  /** Move onto the next stage */
  bool increaseStage();
  /** Move onto the next stage, sum strategy. */
  bool increaseStageSum();
  /** Move onto the next stage, max strategy. */
  bool increaseStageMax();
  /** Move on in the current stage */
  bool nextCombination();
  /** Move onto the next combination. */
  bool nextCombinationInternal();
  /** Find the next lexicographically smallest combination of terms that change
   * on the change prefix, each digit is within the current state,  and there is
   * at least one digit not in the previous stage. */
  bool nextCombinationSum();
  /** Find the next lexicographically smallest combination of terms that change
   * on the change prefix and their sum is equal to d_currentStage. */
  bool nextCombinationMax();
  /** Set up terms for given variable.  */
  virtual size_t prepareTerms(size_t variableIx) = 0;
  /**
   *  Get a given term for a given variable.
   */
  virtual Node getTermCore(size_t variableIx,
                           size_t term_index) CVC4_WARN_UNUSED_RESULT = 0;

  Node getTerm(size_t variableIx, size_t term_index) CVC4_WARN_UNUSED_RESULT
  {
    return getTermCore(variableIx, d_termPermutations[variableIx][term_index]);
  }

  void runLearning(size_t variableIx);
  virtual Node getTerm(size_t variableIx,
                       size_t term_index) CVC4_WARN_UNUSED_RESULT = 0;
};  // namespace quantifiers

/**
 * Enumerate ground terms as they come from the term database.
 */
class TermTupleEnumeratorBasic : public TermTupleEnumeratorBase
{
 public:
  TermTupleEnumeratorBasic(Node quantifier,
                           bool fullEffort,
                           bool increaseSum,
                           TermTupleEnumeratorContext* context)
      : TermTupleEnumeratorBase(quantifier, fullEffort, increaseSum, context)
  {
  }

  virtual ~TermTupleEnumeratorBasic() = default;

 protected:
  /**  a list of terms for each type */
  std::map<TypeNode, std::vector<Node> > d_termDbList;
  virtual size_t prepareTerms(size_t variableIx) override;
  virtual Node getTermCore(size_t variableIx, size_t term_index) override;
};

/**
 * Enumerate ground terms according to the relevant domain class.
 */
class TermTupleEnumeratorRD : public TermTupleEnumeratorBase
{
 public:
  TermTupleEnumeratorRD(Node quantifier,
                        bool fullEffort,
                        bool increaseSum,
                        TermTupleEnumeratorContext* context)
      : TermTupleEnumeratorBase(quantifier, fullEffort, increaseSum, context)
  {
  }
  virtual ~TermTupleEnumeratorRD() = default;

 protected:
  virtual size_t prepareTerms(size_t variableIx) override
  {
    return d_context->d_rd->getRDomain(d_quantifier, variableIx)
        ->d_terms.size();
  }
  virtual Node getTermCore(size_t variableIx, size_t term_index) override
  {
    return d_context->d_rd->getRDomain(d_quantifier, variableIx)
        ->d_terms[term_index];
  }
};

TermTupleEnumeratorInterface* mkTermTupleEnumerator(
    Node quantifier, const TermTupleEnumeratorContext* context)
{
  return isRd ? static_cast<TermTupleEnumeratorInterface*>(
             new TermTupleEnumeratorRD(
                 quantifier, fullEffort, increaseSum, context))
              : static_cast<TermTupleEnumeratorInterface*>(
                  new TermTupleEnumeratorBasic(
                      quantifier, fullEffort, increaseSum, context));
}

bool TermTupleEnumeratorContext::addTerm(Node quantifier,
                                         Node instantiationTerm,
                                         size_t phase)
{
  if (ContainsKey(d_qinfos[quantifier].d_termInfos, instantiationTerm))
  {
    return false;
  }

  const auto age = d_qinfos[quantifier].d_termInfos.size();
  d_qinfos[quantifier].d_termInfos[instantiationTerm] =
      TermInfo::mk(age, phase);
  return true;
}

size_t TermTupleEnumeratorContext::getCurrentPhase(Node quantifier) const
{
  const auto i = d_qinfos.find(quantifier);
  return i != d_qinfos.end() ? i->second.d_currentPhase : 0;
}

size_t TermTupleEnumeratorContext::increasePhase(Node quantifier)
{
  const auto i = d_qinfos.find(quantifier);
  return i != d_qinfos.end() ? i->second.d_currentPhase++
                             : d_qinfos[quantifier].d_currentPhase = 1;
}

void TermTupleEnumeratorBase::init()
{
  Trace("inst-alg-rd") << "Initializing enumeration " << d_quantifier
                       << std::endl;
  d_currentStage = 0;
  d_hasNext = true;
  d_stageCount = 1;  // in the case of full effort we do at least one stage
  d_termPermutations.resize(d_variableCount);

  if (d_variableCount == 0)
  {
    d_hasNext = false;
    return;
  }

  bool anyTerms = false;
  const auto currentPhase = d_context->getCurrentPhase(d_quantifier);
  // prepare a sequence of terms for each quantified variable
  // additionally initialize the cache for variable types
  for (size_t variableIx = 0; variableIx < d_variableCount; variableIx++)
  {
    d_typeCache.push_back(d_quantifier[0][variableIx].getType());
    const size_t termsSize = prepareTerms(variableIx);
    Trace("inst-alg-rd") << "Variable " << variableIx << " has " << termsSize
                         << " in relevant domain." << std::endl;
    if (termsSize == 0 && !d_fullEffort)
    {
      d_hasNext = false;
      return;  // give up on an empty domBoomain
    }
    for (size_t termIx = 0; termIx < termsSize; termIx++)
    {
      const auto term = getTermCore(variableIx, termIx);
      anyTerms =
          d_context->addTerm(d_quantifier, term, currentPhase) || anyTerms;
    }
    d_termsSizes.push_back(termsSize);
    d_stageCount = std::max(d_stageCount, termsSize);
    runLearning(variableIx);
  }

  Trace("inst-alg-rd") << "Will do " << d_stageCount
                       << " stages of instantiation." << std::endl;
  d_termIndex.resize(d_variableCount, 0);
  if (anyTerms)
  {
    d_context->increasePhase(d_quantifier);
  }
}

bool TermTupleEnumeratorBase::hasNext()
{
  if (!d_hasNext)
  {
    return false;
  }

  if (d_stepCounter++ == 0)
  {  // TODO:any (nice)  way of avoiding this special if?
    Assert(d_currentStage == 0);
    Trace("inst-alg-rd") << "Try stage " << d_currentStage << "..."
                         << std::endl;
    return true;
  }

  // try to find the next combination
  return d_hasNext = nextCombination();
}

void TermTupleEnumeratorBase::failureReason(const std::vector<bool>& mask)
{
  if (Trace.isOn("inst-alg"))
  {
    traceMaskedVector("inst-alg", "failureReason", mask, d_termIndex);
  }
  d_disabledCombinations.add(mask, d_termIndex);  // record failure
  // update change prefix accordingly
  for (d_changePrefix = mask.size();
       d_changePrefix && !mask[d_changePrefix - 1];
       d_changePrefix--)
    ;
}

void TermTupleEnumeratorBase::next(/*out*/ std::vector<Node>& terms)
{
  Trace("inst-alg-rd") << "Try instantiation: " << d_termIndex << std::endl;
  terms.resize(d_variableCount);
  for (size_t variableIx = 0; variableIx < d_variableCount; variableIx++)
  {
    const Node t = d_termsSizes[variableIx] == 0
                       ? Node::null()
                       : getTerm(variableIx, d_termIndex[variableIx]);
    terms[variableIx] = t;
    Trace("inst-alg-rd") << t << "  ";
    Assert(terms[variableIx].isNull()
           || terms[variableIx].getType().isComparableTo(
               d_quantifier[0][variableIx].getType()));
  }
  Trace("inst-alg-rd") << std::endl;
}

bool TermTupleEnumeratorBase::increaseStageSum()
{
  const size_t lowerBound = d_currentSum + 1;
  Trace("inst-alg-rd") << "Try sum " << lowerBound << "..." << std::endl;
  d_currentStage = 0;
  for (size_t digit = d_termIndex.size();
       d_currentStage < lowerBound && digit--;)
  {
    const size_t missing = lowerBound - d_currentSum;
    const size_t maxValue = d_termsSizes[digit] ? d_termsSizes[digit] - 1 : 0;
    d_termIndex[digit] = std::min(missing, maxValue);
    d_currentSum += d_termIndex[digit];
  }
  return d_currentSum >= lowerBound;
}

bool TermTupleEnumeratorBase::increaseStage()
{
  d_changePrefix = d_variableCount;  // simply reset upon increase stage
  return d_context->d_increaseSum ? increaseStageSum() : increaseStageMax();
}

bool TermTupleEnumeratorBase::increaseStageMax()
{
  d_stage++;
  if (d_stage >= d_stageCount)
  {
    return false;
  }
  Trace("inst-alg-rd") << "Try stage " << d_currentStage << "..." << std::endl;
  // skipping some elements that have already been definitely seen
  // find the least significant digit that can be set to the current stage
  // TODO: should we skip all?
  std::fill(d_termIndex.begin(), d_termIndex.end(), 0);
  bool found = false;
  for (size_t digit = d_termIndex.size(); !found && digit--;)
  {
    if (d_termsSizes[digit] > d_stage)
    {
      found = true;
      d_termIndex[digit] = d_stage;
    }
  }
  Assert(found);
  return found;
}

bool TermTupleEnumeratorBase::nextCombination()
{
  while (true)
  {
    Trace("inst-alg-rd") << "changePrefix " << d_changePrefix << std::endl;
    if (!nextCombinationInternal() && !increaseStage())
    {
      return false;  // ran out of combinations
    }
    if (!d_disabledCombinations.find(d_termIndex, d_changePrefix))
    {
      return true;  // current combination vetted by disabled combinations
    }
  }
}

/** Move onto the next combination, depending on the strategy. */
bool TermTupleEnumeratorBase::nextCombinationInternal()
{
  return d_context->d_increaseSum ? nextCombinationSum() : nextCombinationMax();
}

/** Find the next lexicographically smallest combination of terms that change
 * on the change prefix and their sum is equal to d_currentStage. */
bool TermTupleEnumeratorBase::nextCombinationMax()
{
  // look for the least significant digit, within change prefix,
  // that can be increased
  bool found = false;
  size_t increaseDigit = d_changePrefix;
  while (!found && increaseDigit--)
  {
    const size_t new_value = d_termIndex[increaseDigit] + 1;
    if (new_value < d_termsSizes[increaseDigit] && new_value <= d_currentStage)
    {
      d_termIndex[increaseDigit] = new_value;
      // send everything after the increased digit to 0
      std::fill(d_termIndex.begin() + increaseDigit + 1, d_termIndex.end(), 0);
      found = true;
    }
  }
  if (!found)
  {
    return false;  // nothing to increase
  }
  // check if the combination has at least one digit in the current stage,
  // since at least one digit was increased, no need for this in stage 1
  bool inStage = d_currentStage <= 1;
  for (size_t i = increaseDigit + 1; !inStage && i--;)
  {
    inStage = d_termIndex[i] >= d_currentStage;
  }
  if (!inStage)  // look for a digit that can increase to current stage
  {
    for (increaseDigit = d_variableCount, found = false;
         !found && increaseDigit--;)
    {
      found = d_termsSizes[increaseDigit] > d_currentStage;
    }
    if (!found)
    {
      return false;  // nothing to increase to the current stage
    }
    Assert(d_termsSizes[increaseDigit] > d_currentStage
           && d_termIndex[increaseDigit] < d_currentStage);
    d_termIndex[increaseDigit] = d_currentStage;
    // send everything after the increased digit to 0
    std::fill(d_termIndex.begin() + increaseDigit + 1, d_termIndex.end(), 0);
  }
  return true;
}

/** Find the next lexicographically smallest combination of terms that change
 * on the change prefix, each digit is within the current state,  and there is
 * at least one digit not in the previous stage. */
bool TermTupleEnumeratorBase::nextCombinationSum()
{
  size_t suffixSum = 0;
  bool found = false;
  size_t increaseDigit = d_termIndex.size();
  while (increaseDigit--)
  {
    const size_t newValue = d_termIndex[increaseDigit] + 1;
    found = suffixSum > 0 && newValue < d_termsSizes[increaseDigit]
            && increaseDigit < d_changePrefix;
    if (found)
    {
      // digit can be increased and suffix can be decreased
      d_termIndex[increaseDigit] = newValue;
      break;
    }
    suffixSum += d_termIndex[increaseDigit];
    d_termIndex[increaseDigit] = 0;
  }
  if (!found)
  {
    return false;
  }
  Assert(suffixSum > 0);
  // increaseDigit went up by one, hence, distribute (suffixSum - 1) in the
  // least significant digits
  suffixSum--;
  for (size_t digit = d_termIndex.size(); suffixSum > 0 && digit--;)
  {
    const size_t maxValue = d_termsSizes[digit] ? d_termsSizes[digit] - 1 : 0;
    d_termIndex[digit] = std::min(suffixSum, maxValue);
    suffixSum -= d_termIndex[digit];
  }
  Assert(suffixSum == 0);  // everything should have been distributed
  return true;
}


void TermTupleEnumeratorBase::runLearning(size_t variableIx)
{
  TimerStat::CodeTimer codeTimer(d_context->d_learningTimer);
  const auto termCount = d_termsSizes[variableIx];
  auto& permutation = d_termPermutations[variableIx];
  permutation.resize(termCount, 0);
  std::iota(permutation.begin(), permutation.end(), 0);

  if (d_context->d_ml == nullptr || termCount == 0)
  {
    return;
  }
  ++d_context->d_learningCounter;

  const auto& relevantTermVector =
      d_context->d_rd->getRDomain(d_quantifier, variableIx)->d_terms;
  std::set<Node> relevant(relevantTermVector.begin(), relevantTermVector.end());

  const auto& qinfo = d_context->d_qinfos.at(d_quantifier);
  const auto& tsinfo = qinfo.d_termInfos;
  std::vector<double> scores(termCount);

  //[age, phase, relevant, depth]
  const size_t featureCount = 4;
  float features[featureCount];

  Trace("inst-alg-rd") << "Predicting terms for var" << variableIx
                       << " on [age, phase, relevant, depth]" << std::endl;
  AlwaysAssert(d_context->d_ml->numberOfFeatures() == 4);
  for (size_t termIx = 0; termIx < termCount; termIx++)
  {
    const auto term = getTermCore(variableIx, termIx);
    const auto termInfo = tsinfo.at(term);
    features[0] = termInfo.d_age;
    features[1] = termInfo.d_phase;
    features[2] = relevant.find(term) != relevant.end() ? 1 : 0;
    features[3] = TermUtil::getTermDepth(term);

    {
      TimerStat::CodeTimer predictTimer(d_context->d_mlTimer);
      scores[termIx] = d_context->d_ml->predict(features);
    }

    Trace("inst-alg-rd") << "Prediction " << term << " : [";
    for (size_t i = 0; i < featureCount; i++)
      Trace("inst-alg-rd") << " " << features[i];
    Trace("inst-alg-rd") << " ] : " << scores[termIx] << std::endl;
  }

  std::stable_sort(
      permutation.begin(), permutation.end(), [scores](size_t a, size_t b) {
        return scores[a] > scores[b];
      });
  Trace("inst-alg-rd") << "Learned order : [";
  for (size_t i = 0; i < permutation.size(); i++)
  {
    Trace("inst-alg-rd") << (i ? ", " : "")
                         // << getTermCore(variableIx, permutation[i])
                         //<< " : "
                         << getTerm(variableIx, i);
  }
  Trace("inst-alg-rd") << "]" << std::endl;
}

size_t TermTupleEnumeratorBasic::prepareTerms(size_t variableIx)
{
  TermDb* const tdb = d_context->d_quantEngine->getTermDatabase();
  QuantifiersState& qs = d_context->d_quantEngine->getState();
  const TypeNode type_node = d_typeCache[variableIx];

  if (!ContainsKey(d_termDbList, type_node))
  {
    const size_t ground_terms_count = tdb->getNumTypeGroundTerms(type_node);
    std::map<Node, Node> repsFound;
    for (size_t j = 0; j < ground_terms_count; j++)
    {
      Node gt = tdb->getTypeGroundTerm(type_node, j);
      if (!options::cegqi() || !quantifiers::TermUtil::hasInstConstAttr(gt))
      {
        Node rep = qs.getRepresentative(gt);
        if (repsFound.find(rep) == repsFound.end())
        {
          repsFound[rep] = gt;
          d_termDbList[type_node].push_back(gt);
        }
      }
    }
  }

  Trace("inst-alg-rd") << "Instantiation Terms for child " << variableIx << ": "
                       << d_termDbList[type_node] << std::endl;
  return d_termDbList[type_node].size();
}

Node TermTupleEnumeratorBasic::getTermCore(size_t variableIx, size_t term_index)
{
  const TypeNode type_node = d_typeCache[variableIx];
  Assert(term_index < d_termDbList[type_node].size());
  return d_termDbList[type_node][term_index];
}

}  // namespace quantifiers
}  // namespace theory
}  // namespace CVC4
