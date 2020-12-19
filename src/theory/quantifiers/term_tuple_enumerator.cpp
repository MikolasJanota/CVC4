/*
 * File:  term_tuple_enumerator.cpp
 * Author:  mikolas
 * Created on:  Fri Dec 18 14:26:58 CET 2020
 * Copyright (C) 2020, Mikolas Janota
 */
#include <iterator>
#include <algorithm>
#include "theory/quantifiers/term_tuple_enumerator.h"

namespace CVC4 {

  //TODO: kosher?
  template <typename T>
  CVC4ostream& operator<< (CVC4ostream& out, const std::vector<T>& v) {
    out << "[ ";
    std::copy(v.begin(), v.end(), std::ostream_iterator<T>(out, " "));
    return out << "]";
  }

namespace theory {
namespace quantifiers {
class TermTupleEnumeratorBase : public TermTupleEnumeratorInterface {
  public:
    TermTupleEnumeratorBase(QuantifiersEngine *qe, Node quantifier, bool fullEffort)
      : d_quantEngine( qe ), s_quantifier(quantifier), s_fullEffort(fullEffort) {}
    virtual ~TermTupleEnumeratorBase() = default;
    virtual bool next() override;
  protected:
    QuantifiersEngine* const d_quantEngine;
    const Node               s_quantifier;
    const bool               s_fullEffort;
    std::vector<unsigned>    d_terms_sizes;
    virtual unsigned prepareTerms(unsigned child_ix) = 0;
    virtual Node getTerm(unsigned child_ix, unsigned term_index) CVC4_WARN_UNUSED_RESULT = 0;
    bool tryStage(unsigned stage, /*out*/ std::vector<unsigned>& termIndex);
    bool nextCombination(unsigned stage, /*out*/ std::vector<unsigned>& termIndex);
};

class TermTupleEnumeratorBasic : public TermTupleEnumeratorBase {
  public:
    TermTupleEnumeratorBasic(QuantifiersEngine *qe, Node quantifier, bool fullEffort)
      : TermTupleEnumeratorBase(qe, quantifier, fullEffort) {}
    virtual ~TermTupleEnumeratorBasic() = default;
  protected:
    std::map<TypeNode, std::vector<Node> > d_term_db_list;
    virtual unsigned prepareTerms(unsigned child_ix) override;
    virtual Node getTerm(unsigned child_ix, unsigned term_index) override;
};

class TermTupleEnumeratorRD : public TermTupleEnumeratorBase {
  public:
    TermTupleEnumeratorRD(QuantifiersEngine *qe, Node quantifier, bool fullEffort, RelevantDomain* rd)
      : TermTupleEnumeratorBase(qe, quantifier, fullEffort), d_rd(rd) {}
    virtual ~TermTupleEnumeratorRD() = default;
  protected:
    RelevantDomain* const d_rd;
    virtual unsigned prepareTerms(unsigned child_ix) override
    {
      return d_rd->getRDomain(s_quantifier, child_ix)->d_terms.size();
    }
    virtual Node getTerm(unsigned child_ix, unsigned term_index) override
    {
      return d_rd->getRDomain(s_quantifier, child_ix)->d_terms[term_index];
    }
};

TermTupleEnumeratorInterface * mkTermTupleEnumerator(QuantifiersEngine *qe,
    Node quantifier, bool fullEffort, bool isRd, RelevantDomain* rd) 
{
  return isRd ?
    static_cast<TermTupleEnumeratorInterface*>(new TermTupleEnumeratorRD(qe, quantifier, fullEffort, rd))
    : static_cast<TermTupleEnumeratorInterface*>(new TermTupleEnumeratorBasic(qe, quantifier, fullEffort));
}

bool TermTupleEnumeratorBase::next()
{
  // ignore if constant true (rare case of non-standard quantifier whose body is
  // rewritten to true)
  if (s_quantifier[1].isConst() && s_quantifier[1].getConst<bool>())
  {
    return false;
  }
  unsigned max_stage = 0;
  const unsigned variable_count = s_quantifier[0].getNumChildren();

  // prepare a sequence of terms for each quantified variable
  for (unsigned i = 0; i < variable_count; i++)
  {
    const unsigned terms_size = prepareTerms(i);
    Trace("inst-alg-rd") << "Variable " << i << " has " << terms_size
                         << " in relevant domain." << std::endl;
    if (terms_size == 0 && !s_fullEffort)
    {
      return false;  // give up on an empty domain
    }
    d_terms_sizes.push_back(terms_size);
    max_stage = std::max(max_stage, terms_size);
  }

  // go through stages
  Trace("inst-alg-rd") << "Will do " << max_stage << " stages of instantiation." << std::endl;
  bool stage_successful = false;
  std::vector<unsigned> termIndex(variable_count, 0);
  for (unsigned stage = 0; stage <= max_stage && !stage_successful; stage++)
  {
    stage_successful = tryStage(stage, termIndex);
  }
  return stage_successful;
}

bool TermTupleEnumeratorBase::nextCombination(unsigned stage, /*out*/ std::vector<unsigned>& termIndex)
{
  for (unsigned digit = termIndex.size(); digit--;)
  {
    const unsigned new_value = termIndex[digit] + 1;
    if (new_value < d_terms_sizes[digit] && new_value <= stage)
    {
      termIndex[digit] = new_value;
      std::fill(termIndex.begin() + digit + 1, termIndex.end(), 0);
      return true;
    }
  }
  return false;
}

bool TermTupleEnumeratorBase::tryStage(unsigned stage, /*out*/ std::vector<unsigned>& termIndex)
{
  Trace("inst-alg-rd") << "Try stage " << stage << "..." << std::endl;
  const unsigned var_count = s_quantifier[0].getNumChildren();
  Assert(termIndex.size() == var_count);
  // skipping  some elements that have already been definitely seen, TODO: should we skip all?
  if (var_count > 0) 
  {
    std::fill(termIndex.begin(), termIndex.end(), 0);
    const unsigned top_value = d_terms_sizes[var_count - 1] ? d_terms_sizes[var_count - 1] - 1 : 0;
    termIndex[var_count - 1] = std::min(stage, top_value);
  }

  // try instantiation
  std::vector<Node> terms;
  terms.reserve(var_count);
  do
  {
    Trace("inst-alg-rd") << "Try instantiation: " << termIndex << std::endl;
    terms.clear();
    for (unsigned child_ix = 0; child_ix < var_count; child_ix++)
    {
      const Node t = d_terms_sizes[child_ix] == 0 ? Node::null() : getTerm(child_ix, termIndex[child_ix]);
      terms.push_back(t);
      Trace("inst-alg-rd") << "  " << t << std::endl;
      Assert(terms[child_ix].isNull()
          || terms[child_ix].getType().isComparableTo(s_quantifier[0][child_ix].getType()));
    }

    Instantiate* const ie = d_quantEngine->getInstantiate();
    if (ie->addInstantiation(s_quantifier, terms))
    {
      Trace("inst-alg-rd") << "Success!" << std::endl;
      ++(d_quantEngine->d_statistics.d_instantiations_guess);
      return true;
    }

    if (d_quantEngine->inConflict())
    {
      // could be conflicting for an internal reason (such as term
      // indices computed in above calls)
      return false;
    }
  } while(nextCombination(stage, termIndex));
  return false;
}

unsigned TermTupleEnumeratorBasic::prepareTerms(unsigned child_ix)
{
  TermDb* const tdb = d_quantEngine->getTermDatabase();
  EqualityQuery* const qy = d_quantEngine->getEqualityQuery();
  const TypeNode type_node = s_quantifier[0][child_ix].getType();

  std::map<TypeNode, std::vector<Node> >::iterator ittd = d_term_db_list.find(type_node);
  if (ittd == d_term_db_list.end())
  {
    const unsigned ground_terms_count = tdb->getNumTypeGroundTerms(type_node);
    std::map<Node, Node> reps_found;
    for (unsigned j = 0; j < ground_terms_count; j++)
    {
      Node gt = tdb->getTypeGroundTerm(type_node, j);
      // TODO: this loop doesn't do anything if this condition does not hold???!!!
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
  Trace("inst-alg-rd") << "Instantiation Terms for child " << child_ix
    << ": " << d_term_db_list[type_node] << std::endl;
  return d_term_db_list[type_node].size();
}

Node TermTupleEnumeratorBasic::getTerm(unsigned child_ix, unsigned term_index)
{
  const TypeNode type_node = s_quantifier[0][child_ix].getType(); // TODO: should we worry about efficiency here, old version used to store this?
  Assert(term_index < d_term_db_list[type_node].size());
  return d_term_db_list[type_node][term_index];
}


} /* CVC4::theory::quantifiers namespace */
} /* CVC4::theory namespace */
} /* CVC4 namespace */
