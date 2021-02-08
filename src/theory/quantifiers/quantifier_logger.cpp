/*
 * File:  quantifier_logger.cpp
 * Author:  mikolas
 * Created on:  Fri Dec 25 14:39:45 CET 2020
 * Copyright (C) 2020, Mikolas Janota
 */
#include <iostream>
#include <iterator>
#include <set>
#include <utility>

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
  // for (const auto& terms : i->second.d_instantiations)
  // {
  //   auto& touched = i->second.d_touched;
  //   std::copy(
  //       terms.begin(), terms.end(), std::inserter(touched, touched.end()));
  // }
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

void QuantifierLogger::registerTryCandidate(Node quantifier,
                                            size_t child_ix,
                                            Node candidate)
{
  if (!ContainsKey(d_infos, quantifier))
  {
    d_infos[quantifier].d_infos.resize(quantifier[0].getNumChildren());
    d_infos[quantifier].d_currentPhase = 0;
  }

  auto& info = d_infos.at(quantifier);
  auto& vinfos = info.d_infos;
  AlwaysAssert(vinfos.size() == quantifier[0].getNumChildren());
  auto& vinfo = info.d_infos[child_ix];

  if (!ContainsKey(vinfo, candidate))
  {
    vinfo[candidate];
  }

  auto& cinfo = vinfo.at(candidate);
  cinfo.d_tried++;
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

static std::ostream& printSample(std::ostream& out,
                                 const std::vector<int32_t>& sample)
{
  for (size_t i = 0; i < sample.size(); i++)
  {
    out << (i ? ", " : "") << sample[i];
  }
  return out;
}

std::ostream& QuantifierLogger::printCore(std::ostream& out)
{
  std::set<Node> useful_terms;
  std::set<Node> all_candidates;
  std::vector<std::vector<int32_t>> samples;
  out << "(quantifier_candidates " << std::endl;
  for (const auto& entry : d_infos)
  {
    const auto& quantifier = entry.first;
    const auto& usefulInstantiations = entry.second.d_useful;
    const auto& allInstantiations = entry.second.d_instantiations;
    const auto name = quantifier;
    const auto& infos = entry.second.d_infos;
    const auto variableCount = infos.size();

    // Record if a term was ever useful for each variable
    std::vector<std::map<Node, int32_t>> usefulPerVariable(variableCount);
    for (const auto& instantiation : usefulInstantiations)
    {
      Assert(instantiation.size() == variableCount);
      for (size_t varIx = 0; varIx < variableCount; varIx++)
      {
        auto& vm = usefulPerVariable[varIx];
        const auto& term = instantiation[varIx];
        const auto i = vm.find(term);
        if (i == vm.end())
        {
          vm.insert(i, std::make_pair(term, 1));
        }
        else
        {
          i->second++;
        }
      }
    }

    // d_qe->getNameForQuant(quantifier, name, false);

    out << "(candidates " << name << " " << std::endl;
    for (size_t varIx = 0; varIx < variableCount; varIx++)
    {
      out << "  (variable " << varIx;
      for (const auto& term_index : infos[varIx])
      {
        const Node& term = term_index.first;
        const auto& candidateInfo = term_index.second;
        if (candidateInfo.d_tried == 0)
        {
          continue;
        }

        all_candidates.insert(term);
        const auto i = usefulPerVariable[varIx].find(term);
        const auto termUseful =
            (i == usefulPerVariable[varIx].end()) ? 0 : i->second;
        out << " (candidate " << term;
        out << " (age " << candidateInfo.d_age << ")";
        out << " (phase " << candidateInfo.d_phase << ")";
        out << " (relevant " << candidateInfo.d_relevant << ")";
        out << " (depth " << TermUtil::getTermDepth(term) << ")";
        out << " (tried " << candidateInfo.d_tried << ")";

        out << " (useful " << termUseful << ")";
        out << ")";
        const auto sz = samples.size();
        samples.resize(sz + 1);
        samples[sz].push_back(candidateInfo.d_age);
        samples[sz].push_back(candidateInfo.d_phase);
        samples[sz].push_back(candidateInfo.d_relevant);
        samples[sz].push_back(TermUtil::getTermDepth(term));
        samples[sz].push_back(candidateInfo.d_tried);
        samples[sz].push_back(termUseful);
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

  out << "; SAMPLES" << std::endl;
  out << "; age, phase, relevant, depth, tried, useful " << std::endl;
  for (const auto& sample : samples)
  {
    printSample(out, sample) << std::endl;
  }
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
