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
  /** Initialize the enumerator. */
  virtual void init() = 0;
  /** Test if there are any more combinations. */
  virtual bool hasNext() = 0;
  /** Obtain the next combination, meaningful only if hasNext Returns true. */
  virtual void next(/*out*/ std::vector<Node>& terms) = 0;
  /** Record which of the terms obtained by the last call of next should not be
   * explored again. */
  virtual void failureReason(const std::vector<bool>& mask) = 0;
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
  LearningInterface* d_ml;
  std::map<Node, QuantifierInfo> d_qinfos;
  TimerStat d_learningTimer, d_mlTimer;
  IntStat d_learningCounter;
  size_t increasePhase(Node quantifier);
  size_t getCurrentPhase(Node quantifier) const;
  bool addTerm(Node quantifier, Node instantiationTerm, size_t phase);
  TermTupleEnumeratorContext()
      : d_learningTimer("theory::quantifiers::fs::timers::learningTimer"),
        d_mlTimer("theory::quantifiers::fs::timers::mlTimer"),
        d_learningCounter("theory::quantifiers::fs::mlCounter", 0)
  {
    smtStatisticsRegistry()->registerStat(&d_learningTimer);
    smtStatisticsRegistry()->registerStat(&d_mlTimer);
    smtStatisticsRegistry()->registerStat(&d_learningCounter);
  }
  ~TermTupleEnumeratorContext()
  {
    smtStatisticsRegistry()->unregisterStat(&d_learningTimer);
    smtStatisticsRegistry()->unregisterStat(&d_mlTimer);
    smtStatisticsRegistry()->unregisterStat(&d_learningCounter);
  }
};

TermTupleEnumeratorInterface* mkTermTupleEnumerator(
    Node quantifier, const TermTupleEnumeratorContext* context);

}  // namespace quantifiers
}  // namespace theory
}  // namespace CVC4
#endif /* TERM_TUPLE_ENUMERATOR_H_7640 */
