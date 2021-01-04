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

#include "theory/quantifiers_engine.h"
#include "theory/quantifiers/instantiation_list.h"

namespace CVC4 {
namespace theory {
namespace quantifiers {

struct QuantifierInfo
{
  std::vector<std::map<Node, size_t> > d_age;
  std::vector<std::vector<Node >> d_useful;
};

class QuantifierLogger
{
 public:
  static QuantifierLogger s_logger;

  void clear()
  {
    d_infos.clear();
  }
  virtual ~QuantifierLogger() { clear(); }
  void setQuantifierEngine(QuantifiersEngine* qe) { d_qe = qe; }
  void setSmtEngine(SmtEngine* e) { d_e = e; }
  bool registerCandidate(Node quantifier, size_t child_ix, Node candidate);
  std::ostream& print(std::ostream& out);

  void registerUseful(const InstantiationList& instantiations);

 protected:
  std::map<Node, QuantifierInfo> d_infos;
  QuantifiersEngine* d_qe;
  SmtEngine* d_e;
  QuantifierLogger() : d_qe(nullptr) {}
};

}  // namespace quantifiers
}  // namespace theory
}  // namespace CVC4

#endif /* QUANTIFIER_LOGGER_H_15196 */
