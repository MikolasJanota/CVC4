/*
 * File:  term_tuple_enumerator.cpp
 * Author:  mikolas
 * Created on:  Fri Dec 18 14:26:58 CET 2020
 * Copyright (C) 2020, Mikolas Janota
 */
#include "theory/quantifiers/term_tuple_enumerator.h"

#include <algorithm>
#include <iterator>

#include "base/map_util.h"
#include "theory/quantifiers/quantifier_logger.h"

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
  TermTupleEnumeratorBase(QuantifiersEngine* qe,
                          Node quantifier,
                          bool fullEffort)
      : d_quantEngine(qe),
        d_quantifier(quantifier),
        d_fullEffort(fullEffort),
        d_variableCount(d_quantifier[0].getNumChildren()),
        d_stepCounter(0)
  {
  }
  virtual ~TermTupleEnumeratorBase() = default;
  virtual void init() override;
  virtual bool hasNext() override;
  virtual void next(/*out*/ std::vector<Node>& terms) override;

 protected:
  QuantifiersEngine* const d_quantEngine;
  const Node d_quantifier;
  const bool d_fullEffort;
  const size_t d_variableCount;
  std::vector<TypeNode> d_typeCache;
  std::vector<size_t> d_termsSizes;
  std::vector<size_t> d_termIndex;
  uint32_t d_stepCounter;
  size_t d_stage;
  size_t d_stageCount;
  bool d_hasNext;
  /* Allow larger indices from now on */
  bool increaseStage();
  /* Move on in the current stage */
  bool nextCombination();
  /**
   *  Set up terms for  given variable.
   */
  virtual size_t prepareTerms(size_t child_ix) = 0;
  /**
   *  Get a given term for a given variable.
   */
  virtual Node getTerm(size_t child_ix,
                       size_t term_index) CVC4_WARN_UNUSED_RESULT = 0;
};

class TermTupleEnumeratorBasic : public TermTupleEnumeratorBase
{
 public:
  TermTupleEnumeratorBasic(QuantifiersEngine* qe,
                           Node quantifier,
                           bool fullEffort)
      : TermTupleEnumeratorBase(qe, quantifier, fullEffort)
  {
  }
  virtual ~TermTupleEnumeratorBasic() = default;

 protected:
  std::map<TypeNode, std::vector<Node> > d_term_db_list;
  virtual size_t prepareTerms(size_t child_ix) override;
  virtual Node getTerm(size_t child_ix, size_t term_index) override;
};

class TermTupleEnumeratorRD : public TermTupleEnumeratorBase
{
 public:
  TermTupleEnumeratorRD(QuantifiersEngine* qe,
                        Node quantifier,
                        bool fullEffort,
                        RelevantDomain* rd)
      : TermTupleEnumeratorBase(qe, quantifier, fullEffort), d_rd(rd)
  {
  }
  virtual ~TermTupleEnumeratorRD() = default;

 protected:
  RelevantDomain* const d_rd;
  virtual size_t prepareTerms(size_t child_ix) override
  {
    return d_rd->getRDomain(d_quantifier, child_ix)->d_terms.size();
  }
  virtual Node getTerm(size_t child_ix, size_t term_index) override
  {
    return d_rd->getRDomain(d_quantifier, child_ix)->d_terms[term_index];
  }
};

TermTupleEnumeratorInterface* mkTermTupleEnumerator(QuantifiersEngine* qe,
                                                    Node quantifier,
                                                    bool fullEffort,
                                                    bool isRd,
                                                    RelevantDomain* rd)
{
  return isRd ? static_cast<TermTupleEnumeratorInterface*>(
             new TermTupleEnumeratorRD(qe, quantifier, fullEffort, rd))
              : static_cast<TermTupleEnumeratorInterface*>(
                  new TermTupleEnumeratorBasic(qe, quantifier, fullEffort));
}

void TermTupleEnumeratorBase::init()
{
  d_stage = 0;
  d_hasNext = true;
  d_stageCount = 1;  // in the case of full effort we do at least one stage

  // ignore if constant true (rare case of non-standard quantifier whose body is
  // rewritten to true)
  if (d_quantifier[1].isConst() && d_quantifier[1].getConst<bool>())
  {
    d_hasNext = false;
    return;
  }

  // prepare a sequence of terms for each quantified variable
  // additionally initialized the cache for variable types
  for (size_t child_ix = 0; child_ix < d_variableCount; child_ix++)
  {
    d_typeCache.push_back(d_quantifier[0][child_ix].getType());
    const size_t terms_size = prepareTerms(child_ix);
    for (size_t term_ix = 0; term_ix < terms_size; term_ix++)
    {
      QuantifierLogger::s_logger.registerCandidate(d_quantifier,
      child_ix, getTerm(child_ix, term_ix));

    }
    Trace("inst-alg-rd") << "Variable " << child_ix << " has " << terms_size
                         << " in relevant domain." << std::endl;
    if (terms_size == 0 && !d_fullEffort)
    {
      d_hasNext = false;
      return;  // give up on an empty domain
    }
    d_termsSizes.push_back(terms_size);
    d_stageCount = std::max(d_stageCount, terms_size);
  }
  Trace("inst-alg-rd") << "Will do " << d_stageCount
                       << " stages of instantiation." << std::endl;
  d_termIndex.resize(d_variableCount, 0);
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
  for (size_t child_ix = 0; child_ix < d_variableCount; child_ix++)
  {
    const Node t = d_termsSizes[child_ix] == 0
                       ? Node::null()
                       : getTerm(child_ix, d_termIndex[child_ix]);
    terms[child_ix] = t;
    Trace("inst-alg-rd") << "  " << t << std::endl;
    Assert(terms[child_ix].isNull()
           || terms[child_ix].getType().isComparableTo(
               d_quantifier[0][child_ix].getType()));
  }
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

size_t TermTupleEnumeratorBasic::prepareTerms(size_t child_ix)
{
  TermDb* const tdb = d_quantEngine->getTermDatabase();
  EqualityQuery* const qy = d_quantEngine->getEqualityQuery();
  const TypeNode type_node = d_typeCache[child_ix];

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
        if (reps_found.find(rep) == reps_found.end())
        {
          reps_found[rep] = gt;
          d_term_db_list[type_node].push_back(gt);
        }
      }
    }
  }
  Trace("inst-alg-rd") << "Instantiation Terms for child " << child_ix << ": "
                       << d_term_db_list[type_node] << std::endl;
  return d_term_db_list[type_node].size();
}

Node TermTupleEnumeratorBasic::getTerm(size_t child_ix, size_t term_index)
{
  const TypeNode type_node = d_typeCache[child_ix];
  Assert(term_index < d_term_db_list[type_node].size());
  return d_term_db_list[type_node][term_index];
}

}  // namespace quantifiers
}  // namespace theory
}  // namespace CVC4
