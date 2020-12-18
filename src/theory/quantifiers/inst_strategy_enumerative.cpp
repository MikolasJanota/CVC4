/*********************                                                        */
/*! \file inst_strategy_enumerative.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Andrew Reynolds, Morgan Deters
 ** This file is part of the CVC4 project.
 ** Copyright (c) 2009-2020 by the authors listed in the file AUTHORS
 ** in the top-level source directory and their institutional affiliations.
 ** All rights reserved.  See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief Implementation of an enumerative instantiation strategy.
 **/

#include "theory/quantifiers/inst_strategy_enumerative.h"

#include "options/quantifiers_options.h"
#include "theory/quantifiers/instantiate.h"
#include "theory/quantifiers/relevant_domain.h"
#include "theory/quantifiers/term_database.h"
#include "theory/quantifiers/term_util.h"
#include "theory/quantifiers/term_tuple_enumerator.h"
#include "theory/quantifiers_engine.h"

namespace CVC4 {

using namespace kind;
using namespace context;

namespace theory {

using namespace inst;

namespace quantifiers {

InstStrategyEnum::InstStrategyEnum(QuantifiersEngine* qe, RelevantDomain* rd)
    : QuantifiersModule(qe), d_rd(rd), d_fullSaturateLimit(-1)
{
}
void InstStrategyEnum::presolve()
{
  d_fullSaturateLimit = options::fullSaturateLimit();
}
bool InstStrategyEnum::needsCheck(Theory::Effort e)
{
  if (d_fullSaturateLimit == 0)
  {
    return false;
  }
  if (options::fullSaturateInterleave())
  {
    if (d_quantEngine->getInstWhenNeedsCheck(e))
    {
      return true;
    }
  }
  if (options::fullSaturateQuant())
  {
    if (e >= Theory::EFFORT_LAST_CALL)
    {
      return true;
    }
  }
  return false;
}

void InstStrategyEnum::reset_round(Theory::Effort e) {}
void InstStrategyEnum::check(Theory::Effort e, QEffort quant_e)
{
  bool doCheck = false;
  bool fullEffort = false;
  if (d_fullSaturateLimit != 0)
  {
    if (options::fullSaturateInterleave())
    {
      // we only add when interleaved with other strategies
      doCheck = quant_e == QEFFORT_STANDARD && d_quantEngine->hasAddedLemma();
    }
    if (options::fullSaturateQuant() && !doCheck)
    {
      if (!d_quantEngine->theoryEngineNeedsCheck())
      {
        doCheck = quant_e == QEFFORT_LAST_CALL;
        fullEffort = true;
      }
    }
  }
  if (!doCheck)
  {
    return;
  }
  Assert(!d_quantEngine->inConflict());
  double clSet = 0;
  if (Trace.isOn("fs-engine"))
  {
    clSet = double(clock()) / double(CLOCKS_PER_SEC);
    Trace("fs-engine") << "---Full Saturation Round, effort = " << e << "---"
                       << std::endl;
  }
  unsigned rstart = options::fullSaturateQuantRd() ? 0 : 1;
  unsigned rend = fullEffort ? 1 : rstart;
  unsigned addedLemmas = 0;
  // First try in relevant domain of all quantified formulas, if no
  // instantiations exist, try arbitrary ground terms.
  // Notice that this stratification of effort levels makes it so that some
  // quantified formulas may not be instantiated (if they have no instances
  // at effort level r=0 but another quantified formula does). We prefer
  // this stratification since effort level r=1 may be highly expensive in the
  // case where we have a quantified formula with many entailed instances.
  FirstOrderModel* fm = d_quantEngine->getModel();
  unsigned nquant = fm->getNumAssertedQuantifiers();
  std::map<Node, bool> alreadyProc;
  for (unsigned r = rstart; r <= rend; r++)
  {
    if (d_rd || r > 0)
    {
      if (r == 0)
      {
        Trace("inst-alg") << "-> Relevant domain instantiate..." << std::endl;
        Trace("inst-alg-debug") << "Compute relevant domain..." << std::endl;
        d_rd->compute();
        Trace("inst-alg-debug") << "...finished" << std::endl;
      }
      else
      {
        Trace("inst-alg") << "-> Ground term instantiate..." << std::endl;
      }
      for (unsigned i = 0; i < nquant; i++)
      {
        Node q = fm->getAssertedQuantifier(i, true);
        bool doProcess = d_quantEngine->hasOwnership(q, this)
                         && fm->isQuantifierActive(q)
                         && alreadyProc.find(q) == alreadyProc.end();
        if (doProcess)
        {
          if (process(q, fullEffort, r == 0))
          {
            // don't need to mark this if we are not stratifying
            if (!options::fullSaturateStratify())
            {
              alreadyProc[q] = true;
            }
            // added lemma
            addedLemmas++;
          }
          if (d_quantEngine->inConflict())
          {
            break;
          }
        }
      }
      if (d_quantEngine->inConflict()
          || (addedLemmas > 0 && options::fullSaturateStratify()))
      {
        // we break if we are in conflict, or if we added any lemma at this
        // effort level and we stratify effort levels.
        break;
      }
    }
  }
  if (Trace.isOn("fs-engine"))
  {
    Trace("fs-engine") << "Added lemmas = " << addedLemmas << std::endl;
    double clSet2 = double(clock()) / double(CLOCKS_PER_SEC);
    Trace("fs-engine") << "Finished full saturation engine, time = "
                       << (clSet2 - clSet) << std::endl;
  }
  if (d_fullSaturateLimit > 0)
  {
    d_fullSaturateLimit--;
  }
}

bool InstStrategyEnum::process(Node f, bool fullEffort, bool isRd)
{
  std::unique_ptr<TermTupleEnumeratorInterface> enumerator(mkTermTupleEnumerator(d_quantEngine, f, fullEffort, isRd));
  return enumerator->next();
  // TODO : term enumerator instantiation?
}

} /* CVC4::theory::quantifiers namespace */
} /* CVC4::theory namespace */
} /* CVC4 namespace */
