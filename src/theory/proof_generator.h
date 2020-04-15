/*********************                                                        */
/*! \file proof_generator.h
 ** \verbatim
 ** Top contributors (to current version):
 **   Andrew Reynolds
 ** This file is part of the CVC4 project.
 ** Copyright (c) 2009-2019 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved.  See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief The abstract proof generator class
 **/

#include "cvc4_private.h"

#ifndef CVC4__THEORY__PROOF_GENERATOR_H
#define CVC4__THEORY__PROOF_GENERATOR_H

#include "context/cdhashmap.h"
#include "expr/node.h"
#include "expr/proof_node.h"
#include "theory/output_channel.h"

namespace CVC4 {
namespace theory {

/**
 * An abstract proof generator class, to be used in combination with
 * ProofOutputChannel (see theory/proof_output_channel.h).
 *
 * A proof generator is intended to be used as a utility in theory
 * solvers for constructing and storing proofs internally. A Theory may have
 * multiple instances of ProofGenerator objects, e.g. if it has more than one
 * way of justifying lemmas or conflicts.
 */
class ProofGenerator
{
 public:
  ProofGenerator();
  virtual ~ProofGenerator() {}
  /** Get the proof for conflict conf */
  virtual std::shared_ptr<ProofNode> getProofForConflict(Node conf) = 0;
  /** Get the proof for lemma lem */
  virtual std::shared_ptr<ProofNode> getProofForLemma(Node lem) = 0;
};

/**
 * An eager proof generator, with explicit proof caching.
 *
 * The intended use of this class is to store proofs for lemmas and conflicts
 * at the time they are sent out on the ProofOutputChannel. This means that the
 * getProofForConflict and getProofForLemma methods are lookups in a
 * (user-context depedent) map, the field d_proofs below.
 *
 * In detail, the method setProofForConflict(conf, pf) should be called prior to
 * calling ProofOutputChannel(conf, X), where X is this generator. Similarly for
 * setProofForLemma.
 *
 * A clean usage of this class in combination with ProofOutputChannel is the
 * following:
 * //-----------------------------------------------------------
 *   class MyEagerProofGenerator : public EagerProofGenerator
 *   {
 *     public:
 *      Node getProvenConflictByMethodX(...)
 *      {
 *        Node conf = [construct conflict];
 *        std::shared_ptr<ProofNode> pf = [construct its proof];
 *        setProofForConflict(conf, pf);
 *        return conf;
 *      }
 *   };
 *   // [1] Make objects given user context u and output channel out
 *   MyEagerProofGenerator epg(u);
 *   ProofOutputChannel poc(out, u);
 *
 *   // [2] Assume epg realizes there is a conflict. We have it store the proof
 *   // internally and return the conflict node.
 *   Node conf = epg.getProvenConflictByMethodX(...);
 *
 *   // [3] Send the conflict on the proof output channel, referencing that epg
 *   // is who can provide a proof for it.
 *   poc.conflict(conf, &epg);
 *
 *   // [4] Any time later in the user context, we may ask poc for the proof,
 *   // where notice this calls the getProof method of epg.
 *   std::shared_ptr<ProofNode> pf = poc.getProofForConflict(conf);
 * //-----------------------------------------------------------
 * In other words, the proof generator epg is responsible for creating and
 * storing the proof internally, and the proof output channel is responsible for
 * maintaining the map that epg is who to ask for the proof of the conflict.
 */
class EagerProofGenerator : public ProofGenerator
{
  typedef context::CDHashMap<Node, std::shared_ptr<ProofNode>, NodeHashFunction>
      NodeProofNodeMap;

 public:
  EagerProofGenerator(context::UserContext* u);
  ~EagerProofGenerator() {}
  /** Set that pf is the proof for conflict conf */
  void setProofForConflict(Node conf, std::shared_ptr<ProofNode> pf);
  /** Set that pf is the proof for lemma lem */
  void setProofForLemma(Node lem, std::shared_ptr<ProofNode> pf);
  /** Get the proof for conflict conf. */
  std::shared_ptr<ProofNode> getProofForConflict(Node conf) override;
  /** Get the proof for lemma lem. */
  std::shared_ptr<ProofNode> getProofForLemma(Node lem) override;
 protected:
  /** Get the proof for the given key */
  std::shared_ptr<ProofNode> getProof(Node key);
  /**
   * A user-context-dependent map from lemmas and conflicts to proofs provided
   * by calls to setProofForConflict and setProofForLemma above.
   */
  NodeProofNodeMap d_proofs;
};

}  // namespace theory
}  // namespace CVC4

#endif /* CVC4__THEORY__PROOF_GENERATOR_H */
