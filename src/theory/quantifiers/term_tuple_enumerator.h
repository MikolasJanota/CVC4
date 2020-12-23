/*
 * File:  term_tuple_enumerator.h
 * Author:  mikolas
 * Created on:  Fri Dec 18 14:26:54 CET 2020
 * Copyright (C) 2020, Mikolas Janota
 */
#ifndef TERM_TUPLE_ENUMERATOR_H_7640
#define TERM_TUPLE_ENUMERATOR_H_7640
#include "theory/quantifiers_engine.h"
#include "theory/quantifiers/relevant_domain.h"
namespace CVC4 {
namespace theory {
namespace quantifiers {
class TermTupleEnumeratorInterface {
  public:
    virtual void init() = 0;
    virtual bool hasNext() = 0;
    virtual bool next() = 0;
    virtual ~TermTupleEnumeratorInterface() = default;
};

TermTupleEnumeratorInterface * mkTermTupleEnumerator(QuantifiersEngine *qe,
    Node quantifier, bool fullEffort, bool isRd, RelevantDomain* rd);

} /* CVC4::theory::quantifiers namespace */
} /* CVC4::theory namespace */
} /* CVC4 namespace */
#endif /* TERM_TUPLE_ENUMERATOR_H_7640 */
