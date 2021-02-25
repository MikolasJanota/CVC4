/*********************                                                        */
/*! \file index_trie.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Mikolas Janota
 ** This file is part of the CVC4 project.
 ** Copyright (c) 2009-2020 by the authors listed in the file AUTHORS
 ** in the top-level source directory and their institutional affiliations.
 ** All rights reserved.  See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief Implementation of a trie that store subsets of tuples of term indices
 ** that are not yielding  useful instantiations. of quantifier instantiation.
 ** This is used in the term_tuple_enumerator.
 **/
#ifndef INDEX_TRIE_H_45
#define INDEX_TRIE_H_45
#include <algorithm>
#include <utility>
#include <vector>

#include "base/check.h"

namespace CVC4 {
struct IndexTrieNode
{
  std::vector<std::pair<size_t, IndexTrieNode*>> d_children;
  IndexTrieNode* d_blank = nullptr;
};

/** Trie  of indices, used to check subsumption.
 *
 */
class IndexTrie
{
 public:
  IndexTrie(bool ignoreFullySpecified)
      : d_ignoreFullySpecified(ignoreFullySpecified),
        d_root(new IndexTrieNode())
  {
  }

  virtual ~IndexTrie() { freeRec(d_root); }

  /**  Add a tuple of values into the trie  masked by a bitmask. */
  void add(const std::vector<bool>& mask, const std::vector<size_t>& values);

  /** Check if the given set of indices is subsumed by something present in the
   * trie. If it is subsumed,  give the maximum non-blank index. */
  bool find(const std::vector<size_t>& members,
            /*out*/ size_t& maxNonBlank) const
  {
    maxNonBlank = 0;
    return findRec(d_root, 0, members, maxNonBlank);
  }

 private:
  const bool d_ignoreFullySpecified;
  IndexTrieNode* d_root;

  void freeRec(IndexTrieNode* n);

  bool findRec(const IndexTrieNode* n,
               size_t index,
               const std::vector<size_t>& members,
               size_t& maxNonBlank) const;

  IndexTrieNode* addRec(IndexTrieNode* n,
                        size_t index,
                        size_t cardinality,
                        const std::vector<bool>& mask,
                        const std::vector<size_t>& values);
};

}  // namespace CVC4
#endif /* INDEX_TRIE_H_45 */
