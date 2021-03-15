/*********************                                                        */
/*! \file  term_tuple_enumerator.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Mikolas Janota
 ** This file is part of the CVC4 project.
 ** Copyright (c) 2009-2021 by the authors listed in the file AUTHORS
 ** in the top-level source directory and their institutional affiliations.
 ** All rights reserved.  See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief Implementation of an enumeration of tuples of terms for the purpose
 *of quantifier instantiation.
 **/
#ifndef CVC4__THEORY__QUANTIFIERS__TERM_TUPLE_ENUMERATOR_H
#define CVC4__THEORY__QUANTIFIERS__TERM_TUPLE_ENUMERATOR_H
#include <random>
#include <vector>

#include "expr/node.h"

namespace CVC4 {
namespace theory {

class QuantifiersEngine;

namespace quantifiers {

class RelevantDomain;

/**  Interface for enumeration of tuples of terms.
 *
 * The interface should be used as follows. Firstly, init is called, then,
 * repeatedly,  verify if there are any combinations left by calling hasNext
 * and obtaining the next combination by calling next.
 *
 *  Optionally, if the  most recent combination is determined to be undesirable
 * (for whatever reason), the method failureReason is used to indicate which
 *  positions of the tuple are responsible for the said failure.
 */
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

/** A struct bundling up parameters for term tuple enumerator.*/
struct TermTupleEnumeratorContext
{
  QuantifiersEngine* d_quantEngine;
  RelevantDomain* d_rd;
  std::mt19937* d_mt;
  bool d_fullEffort;
  bool d_increaseSum;
  bool d_isRd;
};

/**  A function to construct a tuple enumerator.
 *
 * Currently we support the enumerators based on the following idea.
 * The tuples are represented as tuples of
 * indices of  terms, where the tuple has as many elements as there are
 * quantified variables in the considered quantifier.
 *
 * Like so, we see a tuple as a number, where the digits may have different
 * ranges. The most significant digits are stored first.
 *
 * Tuples are enumerated  in a lexicographic order in stages. There are 2
 * possible strategies, either  all tuples in a given stage have the same sum of
 * digits, or, the maximum  over these digits is the same (controlled by
 * d_increaseSum).
 *
 * Further, an enumerator  either draws ground terms from the term database or
 * using the relevant domain class  (controlled by d_isRd).
 */
TermTupleEnumeratorInterface* mkTermTupleEnumerator(
    Node quantifier, const TermTupleEnumeratorContext* context);

}  // namespace quantifiers
}  // namespace theory
}  // namespace CVC4
#endif /* TERM_TUPLE_ENUMERATOR_H_7640 */
