/*
 * File:  term_tuple_enumerator.h
 * Author:  mikolas
 * Created on:  Fri Dec 18 14:26:54 CET 2020
 * Copyright (C) 2020, Mikolas Janota
 */
#ifndef TERM_TUPLE_ENUMERATOR_H_7640
#define TERM_TUPLE_ENUMERATOR_H_7640
#include <cstddef>
#include <vector>

#include "cvc4_public.h"
#include "smt/smt_statistics_registry.h"
#include "theory/quantifiers/ml.h"
#include "theory/quantifiers/relevant_domain.h"
#include "theory/quantifiers_engine.h"

namespace CVC4 {
namespace theory {
namespace quantifiers {
class TermTupleEnumeratorInterface
{
 public:
  virtual void init() = 0;
  virtual bool hasNext() = 0;
  virtual void next(/*out*/ std::vector<Node>& terms) = 0;
  virtual ~TermTupleEnumeratorInterface() = default;
};

struct TermInfo
{
  size_t d_age, d_phase;
  static TermInfo mk(size_t age, size_t phase)
  {
    TermInfo rv;
    rv.d_age = age;
    rv.d_phase = phase;
    return rv;
  }
};

struct QuantifierInfo
{
  size_t d_currentPhase = 0;
  std::map<Node, TermInfo> d_termInfos;
};

struct TermTupleEnumeratorContext
{
  QuantifiersEngine* d_quantEngine;
  RelevantDomain* d_rd;
  LightGBMWrapper* d_ml;
  std::map<Node, QuantifierInfo> d_qinfos;
  TimerStat d_learningTimer;
  size_t increasePhase(Node quantifier);
  size_t getCurrentPhase(Node quantifier) const;
  bool addTerm(Node quantifier, Node instantiationTerm, size_t phase);
  TermTupleEnumeratorContext()
      : d_learningTimer("theory::quantifiers::fs::timers::learningTimer")
  {
    smtStatisticsRegistry()->registerStat(&d_learningTimer);
  }
  ~TermTupleEnumeratorContext()
  {
    smtStatisticsRegistry()->unregisterStat(&d_learningTimer);
  }
};

TermTupleEnumeratorInterface* mkTermTupleEnumerator(
    Node quantifier,
    bool fullEffort,
    bool isRd,
    TermTupleEnumeratorContext* context);

}  // namespace quantifiers
}  // namespace theory
}  // namespace CVC4
#endif /* TERM_TUPLE_ENUMERATOR_H_7640 */
