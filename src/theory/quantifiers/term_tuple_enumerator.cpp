/*
 * File:  term_tuple_enumerator.cpp
 * Author:  mikolas
 * Created on:  Fri Dec 18 14:26:58 CET 2020
 * Copyright (C) 2020, Mikolas Janota
 */
#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <numeric>
#include <vector>

#include "base/map_util.h"
#include "base/output.h"
#include "theory/quantifiers/term_tuple_enumerator.h"
#include "theory/quantifiers/term_util.h"

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
                          TermTupleEnumeratorContext* context)
      : d_quantifier(quantifier),
        d_fullEffort(fullEffort),
        d_variableCount(d_quantifier[0].getNumChildren()),
        d_context(context),
        d_stepCounter(0)
  {
  }
  virtual ~TermTupleEnumeratorBase() = default;
  virtual void init() override;
  virtual bool hasNext() override;
  virtual void next(/*out*/ std::vector<Node>& terms) override;

 protected:
  const Node d_quantifier;
  const bool d_fullEffort;
  const size_t d_variableCount;
  TermTupleEnumeratorContext* const d_context;
  std::vector<TypeNode> d_typeCache;
  std::vector<size_t> d_termsSizes;
  std::vector<size_t> d_termIndex;
  uint32_t d_stepCounter;
  size_t d_stage;
  size_t d_stageCount;
  bool d_hasNext;
  std::vector<std::vector<size_t> > d_termPermutations;
  /* Allow larger indices from now on */
  bool increaseStage();
  /* Move on in the current stage */
  bool nextCombination();
  /**
   *  Set up terms for  given variable.
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
};

class TermTupleEnumeratorBasic : public TermTupleEnumeratorBase
{
 public:
  TermTupleEnumeratorBasic(Node quantifier,
                           bool fullEffort,
                           TermTupleEnumeratorContext* context)
      : TermTupleEnumeratorBase(quantifier, fullEffort, context)
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
                        TermTupleEnumeratorContext* context)
      : TermTupleEnumeratorBase(quantifier, fullEffort, context)
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
    Node quantifier,
    bool fullEffort,
    bool isRd,
    TermTupleEnumeratorContext* context)
{
  return isRd
             ? static_cast<TermTupleEnumeratorInterface*>(
                 new TermTupleEnumeratorRD(quantifier, fullEffort, context))
             : static_cast<TermTupleEnumeratorInterface*>(
                 new TermTupleEnumeratorBasic(quantifier, fullEffort, context));
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
      return;  // give up on an empty domain
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
  {  // TODO: (nice) any way of avoiding this special if?
    Assert(d_stage == 0);
    Trace("inst-alg-rd") << "Try stage " << d_stage << "..." << std::endl;
    return true;
  }

  // try to find the next combination in the current state or increase stage
  if (nextCombination() || increaseStage())
  {
    return true;
  }

  // we ran out of stages
  return d_hasNext = false;
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

bool TermTupleEnumeratorBase::increaseStage()
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
  Assert(d_termIndex.size() == d_variableCount);
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

void TermTupleEnumeratorBase::runLearning(size_t variableIx)
{
  const auto termCount = d_termsSizes[variableIx];
  auto& permutation = d_termPermutations[variableIx];
  permutation.resize(termCount, 0);
  std::iota(permutation.begin(), permutation.end(), 0);

  if (d_context->d_ml == nullptr)
  {
    return;
  }

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
  for (size_t termIx = 0; termIx < termCount; termIx++)
  {
    const auto term = getTermCore(variableIx, termIx);
    const auto termInfo = tsinfo.at(term);
    features[0] = termInfo.d_age;
    features[1] = termInfo.d_phase;
    features[2] = relevant.find(term) != relevant.end() ? 1 : 0;
    features[3] = TermUtil::getTermDepth(term);
    scores[termIx] = d_context->d_ml->predict(features);

    Trace("inst-alg-rd") << "Prediction " << term << " : [";
    for (size_t i = 0; i < featureCount; i++)
      Trace("inst-alg-rd") << " " << features[i];
    Trace("inst-alg-rd") << " ] : " << scores[termIx] << std::endl;
  }

  std::sort(permutation.begin(),
            permutation.end(),
            [scores](size_t a, size_t b) { return scores[a] > scores[b]; });
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
  EqualityQuery* const qy = d_context->d_quantEngine->getEqualityQuery();
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
        Node rep = qy->getRepresentative(gt);
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
