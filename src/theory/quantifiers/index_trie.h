/*
 * File:  src/theory/quantifiers/index_trie.h
 * Author:  mikolas
 * Created on:  Thu Feb 18 15:20:13 CET 2021
 * Copyright (C) 2021, Mikolas Janota
 */
#ifndef INDEX_TRIE_H_45
#define INDEX_TRIE_H_45
#include <vector>

#include "base/check.h"
namespace CVC4 {
struct IndexTrieNode
{
  size_t d_value;
  std::vector<IndexTrieNode*> d_children;
};

class IndexTrie
{
 public:
  IndexTrie() : d_root(new IndexTrieNode()) {}
  virtual ~IndexTrie() { free_rec(d_root); }
  bool isEmpty() const { return d_root->d_children.empty(); }
  void add(const std::vector<size_t>& members)
  {
    d_root = add_rec(d_root, 0, members);
  }
  bool find(const std::vector<size_t>& members) const
  {
    return find_rec(d_root, 0, members);
  }

 private:
  IndexTrieNode* d_root;

  void free_rec(IndexTrieNode* n)
  {
    if (!n)
    {
      return;
    }
    for (IndexTrieNode* c : n->d_children)
    {
      free_rec(c);
    }
    delete n;
  }

  bool find_rec(const IndexTrieNode* n,
                size_t index,
                const std::vector<size_t>& members) const
  {
    if (index >= members.size())  // all elements matched
    {
      return true;
    }
    if (!n)
    {
      return false;
    }
    for (const IndexTrieNode* c : n->d_children)
    {
      if ((c->d_value == members[index] && find_rec(c, index + 1, members))
          || (c->d_value != members[index] && find_rec(c, index, members)))
      {
        return true;
      }
    }
    return false;
  }

  IndexTrieNode* add_rec(IndexTrieNode* n,
                         size_t index,
                         const std::vector<size_t>& members)
  {
    if (index >= members.size())  // the old strings are already subsumed
    {
      free_rec(n);
      return nullptr;
    }
    if (!n)
    {
      n = new IndexTrieNode();
      n->d_value = members[index];
    }
    for (size_t j = 0; j < n->d_children.size(); j++)
    {
      if (n->d_children[j]->d_value == members[index])
      {
        n->d_children[j] = add_rec(n->d_children[j], index + 1, members);
        return n;
      }
    }
    n->d_children.push_back(add_rec(nullptr, index + 1, members));
    return n;
  }
};

}  // namespace CVC4
#endif /* INDEX_TRIE_H_45 */
