/*
 * File:  quantifier_logger.cpp
 * Author:  mikolas
 * Created on:  Fri Dec 25 14:39:45 CET 2020
 * Copyright (C) 2020, Mikolas Janota
 */
#include <iostream>
#include <iterator>
#include <set>

#include "base/map_util.h"
#include "theory/quantifiers/quantifier_logger.h"

namespace CVC4 {
namespace theory {
namespace quantifiers {
QuantifierLogger QuantifierLogger::s_logger;

void QuantifierLogger::registerInstantiations(Node quantifier,
                                              QuantifiersEngine* qe)
{
  //  Assert(qe == d_qe);
  auto i = d_infos.find(quantifier);
  if (i == d_infos.end())
  {
    std::cerr << "Warning unregistered quantifier:" << quantifier << std::endl;
    d_infos[quantifier].d_infos.resize(quantifier[0].getNumChildren());
    d_infos[quantifier].d_currentPhase = 0;
    i = d_infos.find(quantifier);
    Assert(i != d_infos.end());
  }
  qe->getInstantiationTermVectors(quantifier, i->second.d_instantiations);
  for (const auto& terms : i->second.d_instantiations)
  {
    auto& touched = i->second.d_touched;
    std::copy(
        terms.begin(), terms.end(), std::inserter(touched, touched.end()));
  }
}

void QuantifierLogger::registerUseful(const InstantiationList& instantiations)
{
  const auto quantifier = instantiations.d_quant;
  auto i = d_infos.find(quantifier);
  if (i == d_infos.end())
  {
    std::cerr << "Warning unregistered quantifier:" << quantifier << std::endl;
    d_infos[quantifier].d_infos.resize(quantifier[0].getNumChildren());
    d_infos[quantifier].d_currentPhase = 0;
    i = d_infos.find(quantifier);
    Assert(i != d_infos.end());
  }
  i->second.d_useful.insert(i->second.d_useful.end(),
                            instantiations.d_inst.begin(),
                            instantiations.d_inst.end());
}

static std::ostream& printVectorList(
    std::ostream& out, const std::vector<std::vector<Node>>& nodes)
{
  for (const auto& ns : nodes)
  {
    out << "    ( ";
    std::copy(ns.begin(), ns.end(), std::ostream_iterator<Node>(out, " "));
    out << ")" << std::endl;
  }
  return out;
}

std::ostream& QuantifierLogger::printCore(std::ostream& out)
{
  /*
  Assert(d_qe);
  if (!d_qe)
  {
    return out;
  }
*/

  // smt::SmtScope smts(d_qe->getTheoryEngine());

  std::set<Node> useful_terms;
  std::set<Node> all_candidates;
  out << "(quantifier_candidates " << std::endl;
  for (const auto& entry : d_infos)
  {
    const auto& quantifier = entry.first;
    const auto& usefulInstantiations = entry.second.d_useful;
    const auto& allInstantiations = entry.second.d_instantiations;
    const auto& touched = entry.second.d_touched;
    const auto name = quantifier;

    // d_qe->getNameForQuant(quantifier, name, false);
    const auto& infos = entry.second.d_infos;
    out << "(candidates " << name << " " << std::endl;
    for (size_t index = 0; index < infos.size(); index++)
    {
      out << "  (variable " << index;
      for (const auto& term_index : infos[index])
      {
        const Node& term = term_index.first;
        if (touched.find(term) == touched.end())
        {
          continue;
        }
        all_candidates.insert(term);
        out << " (candidate " << term;
        out << " (age " << term_index.second.d_age << ")";
        out << " (phase " << term_index.second.d_phase << ")";
        out << " (relevant " << term_index.second.d_relevant << ")";
        out << ")";
      }
      out << "  )" << std::endl;
    }

    printVectorList(out << "  (all_successful_instantiations " << std::endl,
                    allInstantiations)
        << "  ) " << std::endl;
    printVectorList(out << "  (useful_instantiations " << std::endl,
                    usefulInstantiations)
        << "  )" << std::endl;
    out << ")" << std::endl;
  }
  out << ")" << std::endl;

  /*
  TermUtil te(d_qe);

  out << "(candidate_infos " << std::endl;
  for (const auto& term : all_candidates)
  {
    out << "(candidate_info " << term << " " << std::endl;
    out << "   (depth " << TermUtil::getTermDepth(term) << ") ";
    out << "   (useful " << (useful_terms.find(term) != useful_terms.end())
        << ") ";
    // out << "   (containsUninterpretedConstant "  <<
    // te.containsUninterpretedConstant(term) << ") ";
    out << ")" << std::endl;
  }
  out << ")" << std::endl;
*/
  return out;
}

bool QuantifierLogger::registerCandidate(Node quantifier,
                                         size_t child_ix,
                                         Node candidate,
                                         bool relevant)
{
  if (!ContainsKey(d_infos, quantifier))
  {
    d_infos[quantifier].d_infos.resize(quantifier[0].getNumChildren());
    d_infos[quantifier].d_currentPhase = 0;
  }
  auto& candidates = d_infos[quantifier].d_infos[child_ix];
  if (ContainsKey(candidates, candidate))
  {
    return false;
  }
  candidates[candidate].d_age = candidates.size();
  candidates[candidate].d_phase = d_infos[quantifier].d_currentPhase;
  candidates[candidate].d_relevant = relevant;
  return true;
}

}  // namespace quantifiers
}  // namespace theory
}  // namespace CVC4
