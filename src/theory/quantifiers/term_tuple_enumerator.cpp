/*
 * File:  term_tuple_enumerator.cpp
 * Author:  mikolas
 * Created on:  Fri Dec 18 14:26:58 CET 2020
 * Copyright (C) 2020, Mikolas Janota
 */
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
#include "util/statistics_registry.h"

namespace CVC4 {

// TODO: kosher?
template <typename T>
CVC4ostream& operator<<(CVC4ostream& out, const std::vector<T>& v)
{
  out << "[ ";
  std::copy(v.begin(), v.end(), std::ostream_iterator<T>(out, " "));
  return out << "]";
}

namespace theory {
namespace quantifiers {
class TermTupleEnumeratorBase : public TermTupleEnumeratorInterface
{
 public:
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
        d_disabledCombinations(true) // do not record combinations with no blanks
  {
  }
  virtual ~TermTupleEnumeratorBase() = default;

  // implementation of the TermTupleEnumeratorInterface
  virtual void init() override;
  virtual bool hasNext() override;
  virtual void next(/*out*/ std::vector<Node>& terms) override;
  virtual void failureReason(const std::vector<bool>& mask) override;
  // end of implementation of the TermTupleEnumeratorInterface

 protected:
  const Node d_quantifier;
  const bool d_fullEffort;
  const bool d_increaseSum;
  const size_t d_variableCount;
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

  size_t d_currentSum = 0;
  size_t d_stage;
  size_t d_stageCount;
  bool d_hasNext;
  std::vector<std::vector<size_t> > d_termPermutations;
  /* Allow larger indices from now on */
  /** Move onto the next stage */
  bool increaseStage();
  bool increaseStageSum();
  bool increaseStageMax();
  /** Move on in the current stage */
  bool nextCombination();
  bool nextCombinationInternal();
  bool nextCombinationSum();
  bool nextCombinationMax();
  /**
   *  Set up terms for given variable.
   */
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
};

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
  std::map<TypeNode, std::vector<Node> > d_term_db_list;
  virtual size_t prepareTerms(size_t variableIx) override;
  virtual Node getTermCore(size_t variableIx, size_t term_index) override;
};

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
  Trace("inst-alg") << "Initializing enumeration " << d_quantifier << std::endl;
  d_stage = 0;
  d_hasNext = true;
  d_stageCount = 1;  // in the case of full effort we do at least one stage
  d_termPermutations.resize(d_variableCount);

  // ignore if constant true (rare case of non-standard quantifier whose body is
  // rewritten to true)
  if (d_quantifier[1].isConst() && d_quantifier[1].getConst<bool>())
  {
    d_hasNext = false;
    return;
  }

  bool anyTerms = false;
  const auto currentPhase = d_context->getCurrentPhase(d_quantifier);
  // prepare a sequence of terms for each quantified variable
  // additionally initialized the cache for variable types
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
  if (nextCombination())
  {
    return true;
  }

  // we ran out of stages
  return d_hasNext = false;
}

void TermTupleEnumeratorBase::failureReason(const std::vector<bool>& mask)
{
  if (Trace.isOn("inst-alg"))
  {
    Trace("inst-alg") << "failureReason [ ";
    for (size_t variableIx = 0; variableIx < d_variableCount; variableIx++)
    {
      if (mask[variableIx])
      {
        Trace("inst-alg") << d_termIndex[variableIx] << " ";
      }
      else
      {
        Trace("inst-alg") << "_ ";
      }
    }
    Trace("inst-alg") << "]" << std::endl;
  }
  d_disabledCombinations.add(mask, d_termIndex);
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
  return options::fullSaturateSum() ? increaseStageSum() : increaseStageMax();
}

bool TermTupleEnumeratorBase::increaseStageMax()
{
  d_stage++;
  if (d_stage >= d_stageCount)
  {
    return false;
  }
  Trace("inst-alg-rd") << "Try stage " << d_stage << "..." << std::endl;
  // skipping  some elements that have already been definitely seen
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
    if (!nextCombinationInternal() && !increaseStage())
    {
      return false;
    }
    if (!d_disabledCombinations.find(d_termIndex))
    {
      return true;
    }
  }
}

bool TermTupleEnumeratorBase::nextCombinationInternal()
{
  return d_increaseSum ? nextCombinationSum() : nextCombinationMax();
}

bool TermTupleEnumeratorBase::nextCombinationMax()
{
  for (size_t digit = d_termIndex.size(); digit--;)
  {
    const size_t new_value = d_termIndex[digit] + 1;
    if (new_value < d_termsSizes[digit] && new_value <= d_stage)
    {
      d_termIndex[digit] = new_value;
      std::fill(d_termIndex.begin() + digit + 1, d_termIndex.end(), 0);
      return true;
    }
  }
  return false;
}

bool TermTupleEnumeratorBase::nextCombinationSum()
{
  size_t suffixSum = 0;
  bool found = false;
  size_t increaseDigit = d_termIndex.size();
  while (increaseDigit--)
  {
    const size_t newValue = d_termIndex[increaseDigit] + 1;
    found = suffixSum > 0 && newValue < d_termsSizes[increaseDigit];
    if (found)
    {
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
  size_t missing = suffixSum - 1;  // increaseDigit went up by one
  for (size_t digit = d_termIndex.size(); missing > 0 && digit--;)
  {
    const size_t maxValue = d_termsSizes[digit] ? d_termsSizes[digit] - 1 : 0;
    d_termIndex[digit] = std::min(missing, maxValue);
    missing -= d_termIndex[digit];
  }
  Assert(missing == 0);
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
  /* EqualityQuery* const qy = d_context->d_quantEngine->getEqualityQuery(); */
  QuantifiersState& qs = d_context->d_quantEngine->getState();
  const TypeNode type_node = d_typeCache[variableIx];

  if (!ContainsKey(d_term_db_list, type_node))
  {
    const size_t ground_terms_count = tdb->getNumTypeGroundTerms(type_node);
    std::map<Node, Node> reps_found;
    for (size_t j = 0; j < ground_terms_count; j++)
    {
      Node gt = tdb->getTypeGroundTerm(type_node, j);
      if (!options::cegqi() || !quantifiers::TermUtil::hasInstConstAttr(gt))
      {
        /* Node rep = qy->getRepresentative(gt); */
        Node rep = qs.getRepresentative(gt);
        if (reps_found.find(rep) != reps_found.end())
        {
          continue;
        }
        reps_found[rep] = gt;
        d_term_db_list[type_node].push_back(gt);
      }
    }
  }

  Trace("inst-alg-rd") << "Instantiation Terms for child " << variableIx << ": "
                       << d_term_db_list[type_node] << std::endl;
  return d_term_db_list[type_node].size();
}

Node TermTupleEnumeratorBasic::getTermCore(size_t variableIx, size_t term_index)
{
  const TypeNode type_node = d_typeCache[variableIx];
  Assert(term_index < d_term_db_list[type_node].size());
  return d_term_db_list[type_node][term_index];
}

}  // namespace quantifiers
}  // namespace theory
}  // namespace CVC4
