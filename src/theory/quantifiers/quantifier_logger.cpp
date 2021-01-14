/*
 * File:  quantifier_logger.cpp
 * Author:  mikolas
 * Created on:  Fri Dec 25 14:39:45 CET 2020
 * Copyright (C) 2020, Mikolas Janota
 */
#include <iostream>
#include <set>

#include "base/map_util.h"
#include "theory/quantifiers/quantifier_logger.h"

namespace CVC4 {
namespace theory {
namespace quantifiers {
QuantifierLogger QuantifierLogger::s_logger;

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

std::ostream& QuantifierLogger::printCore(std::ostream &out)
{
  Assert(d_qe);
  if (!d_qe)
  {
    return out;
  }
  std::set<Node> useful_terms;
  std::set<Node> all_candidates;
  out << "(quantifier_candidates " << std::endl;
  for (const auto& entry : d_infos)
  {
    const auto& quantifier = entry.first;

    const auto& instantiations = entry.second.d_useful;
    Node name = quantifier;

    // d_qe->getNameForQuant(quantifier, name, false);
    const auto& infos = entry.second.d_infos;
    out << "(candidates " << name << " " << std::endl;
    for (size_t index = 0; index < infos.size(); index++)
    {
      out << "(variable " << index;
      for (const auto& term_index : infos[index])
      {
        all_candidates.insert(term_index.first);
        out << " (candidate " << term_index.first;
        out << " (age " << term_index.second.d_age << ")";
        out << " (phase " << term_index.second.d_phase << ")";
        out << " (relevant " << term_index.second.d_relevant << ")";
        out << ")";
      }
      out << ")" << std::endl;
    }

    out << "(useful_instantiations " << std::endl;
    for (const auto& instantiation : instantiations)
    {
      out << "(";
      for (const auto& term : instantiation)
      {
        useful_terms.insert(term);
        all_candidates.insert(term);
        out << term << " ";
      }
      out << ") " << std::endl;
    }
    out << ") " << std::endl;

    out << ")" << std::endl;
  }
  out << ")" << std::endl;

  TermUtil te(d_qe);

  out << "(candidate_infos " << std::endl;
  for (const auto& term : all_candidates)
  {
    out << "(candidate_info " << term << " " << std::endl;
    out << "   (depth " << te.getTermDepth(term) << ") ";
    out << "   (useful " << (useful_terms.find(term) != useful_terms.end())
        << ") ";
    // out << "   (containsUninterpretedConstant "  <<
    // te.containsUninterpretedConstant(term) << ") ";
    out << ")" << std::endl;
  }
  out << ")" << std::endl;
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
