/*********************                                                        */
/*! \file didyoumean.cpp
 ** \verbatim
 ** Original author: Kshitij Bansal
 ** Major contributors: none
 ** Minor contributors (to current version): none
 ** This file is part of the CVC4 project.
 ** Copyright (c) 2009-2014  New York University and The University of Iowa
 ** See the file COPYING in the top-level source directory for licensing
 ** information.\endverbatim
 **
 ** \brief did-you-mean style suggestions
 **
 ** ``What do you mean? I don't understand.'' An attempt to be more
 ** helpful than that. Similar to one in git.
 **
 ** There are no dependencies on CVC4, intentionally.
 **/

#include "didyoumean.h"
#include <iostream>
#include <sstream>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp>
using namespace std;

vector<string> DidYouMean::getMatch(string input) {
  /** Magic numbers */
  const int similarityThreshold = 7;
  const unsigned numMatchesThreshold = 10;

  set< pair<int, string> > scores;
  vector<string> ret;
  BOOST_FOREACH(string s, d_words ) {
    if( s == input ) {
      // if input matches AS-IS just return that
      ret.push_back(s);
      return ret;
    }
    int score;
    if(boost::starts_with(s, input)) {
      score = 0;
    } else {
      score = editDistance(input, s) + 1;
    }
    scores.insert( make_pair(score, s) );
  }
  int min_score = scores.begin()->first;
  for(typeof(scores.begin()) i = scores.begin();
      i != scores.end(); ++i) {

    // add if score is overall not too big, and also not much close to
    // the score of the best suggestion
    if(i->first < similarityThreshold && i->first <= min_score + 1) {
      ret.push_back(i->second);
#ifdef DIDYOUMEAN_DEBUG
      cout << i->second << ": " << i->first << std::endl;
#endif
    }
  }
  if(ret.size() > numMatchesThreshold ) ret.resize(numMatchesThreshold);;
  return ret;
}


int DidYouMean::editDistance(const std::string& a, const std::string& b)
{
  // input string: a
  // desired string: b
  
  const int swapCost = 0;
  const int substituteCost = 2;
  const int addCost = 1;
  const int deleteCost = 3;
  const int switchCaseCost = 0;

  int len1 = a.size();
  int len2 = b.size();

  int C[3][len2+1];             // cost

  for(int j = 0; j <= len2; ++j) {
    C[0][j] = j * addCost;
  }

  for(int i = 1; i <= len1; ++i) {

    int cur = i%3;
    int prv = (i+2)%3;
    int pr2 = (i+1)%3;

    C[cur][0] = i * deleteCost;

    for(int j = 1; j <= len2; ++j) {

      C[cur][j] = 100000000;      // INF

      if(a[i-1] == b[j-1]) {
        // match
        C[cur][j] = std::min(C[cur][j], C[prv][j-1]);
      } else if(tolower(a[i-1]) == tolower(b[j-1])){
        // switch case
        C[cur][j] = std::min(C[cur][j], C[prv][j-1] + switchCaseCost);
      } else {
        // substitute
        C[cur][j] = std::min(C[cur][j], C[prv][j-1] + substituteCost);
      }

      // swap
      if(i >= 2 && j >= 2 && a[i-1] == b[j-2] && a[i-2] == b[j-1]) {
        C[cur][j] = std::min(C[cur][j], C[pr2][j-2] + swapCost);
      }

      // add
      C[cur][j] = std::min(C[cur][j], C[cur][j-1] + addCost);

      // delete
      C[cur][j] = std::min(C[cur][j], C[prv][j] + deleteCost);
      
#ifdef DIDYOUMEAN_DEBUG1
      std::cout << "C[" << cur << "][" << 0 << "] = " << C[cur][0] << std::endl;
#endif
    }

  }
  return C[len1%3][len2];
}

string DidYouMean::getMatchAsString(string input, int prefixNewLines, int suffixNewLines) {
  vector<string> matches = getMatch(input);
  ostringstream oss;
  if(matches.size() > 0) {
    while(prefixNewLines --> 0) { oss << endl; }
    if(matches.size() == 1) {
      oss << "Did you mean this?";
    } else {
      oss << "Did you mean any of these?";
    }
    for(unsigned i = 0; i < matches.size(); ++i) {
      oss << "\n        " << matches[i];
    }
    while(suffixNewLines --> 0) { oss << endl; }
  }
  return oss.str();
}
