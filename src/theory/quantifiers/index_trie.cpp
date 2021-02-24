/*
 * File:  src/theory/quantifiers/index_trie.cpp
 * Author:  mikolas
 * Created on:  Wed Feb 24 09:32:17 CET 2021
 * Copyright (C) 2021, Mikolas Janota
 */
#include "theory/quantifiers/index_trie.h"

namespace CVC4 {
void IndexTrie::add(const std::vector<bool>& mask,
                    const std::vector<size_t>& values)
{
  const size_t cardinality = std::count(mask.begin(), mask.end(), true);
  if (d_ignoreFullySpecified && cardinality == mask.size())
  {
    return;
  }

  d_root = add_rec(d_root, 0, cardinality, mask, values);
}

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
  if (!n || index >= members.size())
  {
    return true;  // all elements of members matched
  }
  if (n->d_blank && find_rec(n->d_blank, index + 1, members))
  {
    return true;  // found in the blank branch
  }
  for (const auto& c : n->d_children)
  {
    if (c.first == members[index] && find_rec(c.second, index + 1, members))
    {
      return true;  // found in the matching subtree
    }
  }
  return false;
}

IndexTrieNode* IndexTrie::add_rec(IndexTrieNode* n,
                                  size_t index,
                                  size_t cardinality,
                                  const std::vector<bool>& mask,
                                  const std::vector<size_t>& values)
{
  if (!n)
  {
    return nullptr;  // this tree matches everything, no point to add
  }
  if (cardinality == 0)  // all blanks, all strings match
  {
    free_rec(n);
    return nullptr;
  }

  Assert(index < mask.size());

  if (!mask[index])  // blank position in the added vector
  {
    auto blank = n->d_blank ? n->d_blank : new IndexTrieNode();
    n->d_blank = add_rec(blank, index + 1, cardinality, mask, values);
    return n;
  }
  Assert(cardinality);

  for (auto& edge : n->d_children)
  {
    if (edge.first == values[index])
    {
      // value already amongst the children
      edge.second =
          add_rec(edge.second, index + 1, cardinality - 1, mask, values);
      return n;
    }
  }
  // new child needs to be added
  auto child =
      add_rec(new IndexTrieNode(), index + 1, cardinality - 1, mask, values);
  n->d_children.push_back(std::make_pair(values[index], child));
  return n;
}
}  // namespace CVC4

