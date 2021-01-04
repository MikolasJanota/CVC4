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
  const auto i = d_infos.find(instantiations.d_quant);
  if (i == d_infos.end())
  {
    std::cerr << "Warning unregistered quantifier:" << instantiations.d_quant
              << std::endl;
    return;
  }
  for (const auto& instantiation : instantiations.d_inst)
  {
    i->second.d_useful.push_back(instantiation);
  }
}

std::ostream& QuantifierLogger::print(std::ostream& out)
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
    const auto& ages = entry.second.d_age;
    out << "(candidates " << name << " " << std::endl;
    for (size_t index = 0; index < ages.size(); index++)
    {
      out << "(variable " << index << " ";
      for (const auto& term_index : ages[index])
      {
        all_candidates.insert(term_index.first);
        out << "(age " << term_index.first << " " << term_index.second << ") ";
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
                                         Node candidate)
{
  if (!ContainsKey(d_infos, quantifier))
  {
    d_infos[quantifier].d_age.resize(quantifier[0].getNumChildren());
  }
  std::map<Node, size_t>& candidates = d_infos[quantifier].d_age[child_ix];
  if (ContainsKey(candidates, candidate))
  {
    return false;
  }
  candidates[candidate] = candidates.size();
  return true;
}

}  // namespace quantifiers
}  // namespace theory
}  // namespace CVC4
