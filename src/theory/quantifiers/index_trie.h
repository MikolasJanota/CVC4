/*
 * File:  src/theory/quantifiers/index_trie.h
 * Author:  mikolas
 * Created on:  Thu Feb 18 15:20:13 CET 2021
 * Copyright (C) 2021, Mikolas Janota
 */
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

  virtual ~IndexTrie() { free_rec(d_root); }
  bool isEmpty() const { return d_root->d_children.empty(); }
  void add(const std::vector<bool>& mask, const std::vector<size_t>& values)
  {
    const size_t cardinality = std::count(mask.begin(), mask.end(), true);
    if (d_ignoreFullySpecified && cardinality == mask.size())
    {
      return;
    }

    d_root = add_rec(d_root, 0, mask, values);
  }
  /**  Check if the given set of indices is subsumed by something present in the
   * trie. */
  bool find(const std::vector<size_t>& members) const
  {
    return find_rec(d_root, 0, members);
  }

 private:
  const bool d_ignoreFullySpecified;
  IndexTrieNode* d_root;

  void free_rec(IndexTrieNode* n);

  bool find_rec(const IndexTrieNode* n,
                size_t index,
                const std::vector<size_t>& members) const;

  IndexTrieNode* add_rec(IndexTrieNode* n,
                         size_t index,
                         const std::vector<bool>& mask,
                         const std::vector<size_t>& values);
};

}  // namespace CVC4
#endif /* INDEX_TRIE_H_45 */
