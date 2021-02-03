/*
 * File:  quantifier_logger.h
 * Author:  mikolas
 * Created on:  Fri Dec 25 14:39:39 CET 2020
 * Copyright (C) 2020, Mikolas Janota
 */
#ifndef QUANTIFIER_LOGGER_H_15196
#define QUANTIFIER_LOGGER_H_15196
#include <fstream>
#include <map>
#include <set>

#include "base/map_util.h"
#include "theory/quantifiers/instantiation_list.h"
#include "theory/quantifiers_engine.h"

namespace CVC4 {
namespace theory {
namespace quantifiers {

struct CandidateInfo
{
  size_t d_age, d_phase;
  size_t d_tried = 0;
  bool d_relevant = false;
};

struct QuantifierInfo
{
  std::vector<std::map<Node, CandidateInfo>> d_infos;
  std::vector<std::vector<Node>> d_useful;
  std::vector<std::vector<Node>> d_instantiations;
  // std::set<Node> d_touched;
  size_t d_currentPhase = 0;
};

class QuantifierLogger
{
 public:
  static QuantifierLogger s_logger;

  virtual ~QuantifierLogger() { clear(); }
  /*void setQuantifierEngine(QuantifiersEngine* qe) { d_qe = qe; }
  void setSmtEngine(SmtEngine* e) { d_e = e; }*/
  bool registerCandidate(Node quantifier,
                         size_t child_ix,
                         Node candidate,
                         bool relevant);

  void registerTryCandidate(Node quantifier, size_t child_ix, Node candidate);

  std::ostream& print(std::ostream& out)
  {
    printCore(out);
    clear();
    return out;
  }

  std::ostream& printCore(std::ostream& out);

  void registerUseful(const InstantiationList& instantiations);

  void registerInstantiations(Node quantifier, QuantifiersEngine*);
  void increasePhase(Node quantifier)
  {
    if (!ContainsKey(d_infos, quantifier))
    {
      d_infos[quantifier].d_infos.resize(quantifier[0].getNumChildren());
      d_infos[quantifier].d_currentPhase = 1;
    }
    else
    {
      d_infos[quantifier].d_currentPhase++;
    }
  }

 protected:
  std::map<Node, QuantifierInfo> d_infos;
  // QuantifiersEngine* d_qe;
  // SmtEngine* d_e;
  QuantifierLogger() /* :d_qe(nullptr) */ {}
  void clear()
  {
    // std::cout << "clearing logger\n";
    d_infos.clear();
  }
};

}  // namespace quantifiers
}  // namespace theory
}  // namespace CVC4

#endif /* QUANTIFIER_LOGGER_H_15196 */
