/*
 * File:  quantifier_logger.cpp
 * Author:  mikolas
 * Created on:  Fri Dec 25 14:39:45 CET 2020
 * Copyright (C) 2020, Mikolas Janota
 */
#include "theory/quantifiers/quantifier_logger.h"

#include <set>

#include "base/map_util.h"

namespace CVC4 {
namespace theory {
namespace quantifiers {
QuantifierLogger QuantifierLogger::s_logger;

std::ostream& QuantifierLogger::print(std::ostream& out)
{
  Assert(d_qe);
  if (!d_qe)
  {
    return out;
  }
  std::set<Node> all_candidates;
  out << "(quantifier_candidates " << std::endl;
  for (const auto& entry : d_infos)
  {
    const auto& quantifier = entry.first;

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
    out << ")" << std::endl;
  }
  out << ")" << std::endl;

  TermUtil te(d_qe);
  out << "(candidate_infos " << std::endl;
  for (const auto& term : all_candidates)
  {
    out << "(candidate_info " << term << " " << std::endl;
    out << "   (depth " << te.getTermDepth(term) << ") ";
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
