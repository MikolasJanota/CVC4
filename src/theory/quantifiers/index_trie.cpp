/*
 * File:  src/theory/quantifiers/index_trie.cpp
 * Author:  mikolas
 * Created on:  Wed Feb 24 09:32:17 CET 2021
 * Copyright (C) 2021, Mikolas Janota
 */
#include "theory/quantifiers/index_trie.h"

namespace CVC4 {
void IndexTrie::free_rec(IndexTrieNode* n)
{
  if (!n)
  {
    return;
  }
  for (auto c : n->d_children)
  {
    free_rec(c.second);
  }
  free_rec(n->d_blank);
  delete n;
}

bool IndexTrie::find_rec(const IndexTrieNode* n,
                         size_t index,
                         const std::vector<size_t>& members) const
{
  if (!n)
  {
    return false;
  }
  if (index >= members.size())  // all elements of members matched
  {
    return true;
  }
  if (find_rec(n->d_blank, index + 1, members))
  {
    return true;
  }
  for (const auto& c : n->d_children)
  {
    if (c.first == members[index] && find_rec(c.second, index + 1, members))
    {
      return true;
    }
  }
  return false;
}

IndexTrieNode* IndexTrie::add_rec(IndexTrieNode* n,
                                  size_t index,
                                  const std::vector<bool>& mask,
                                  const std::vector<size_t>& values)
{
  Assert(n);
  if (index >= mask.size())  // all elements on the part matched
  {
    return n;
  }

  if (!mask[index])  // empty position in the added vector
  {
    auto blank = n->d_blank ? n->d_blank : new IndexTrieNode();
    n->d_blank = add_rec(blank, index + 1, mask, values);
    return n;
  }

  for (auto& edge : n->d_children)
  {
    if (edge.first == values[index])
    {
      // value already amongst the children
      edge.second = add_rec(edge.second, index + 1, mask, values);
      return n;
    }
  }
  // new child needs to be added
  auto child = add_rec(new IndexTrieNode(), index + 1, mask, values);
  n->d_children.push_back(std::make_pair(values[index], child));
  return n;
}
}  // namespace CVC4

