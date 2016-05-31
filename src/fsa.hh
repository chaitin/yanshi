#pragma once
#include <utility>
#include <vector>
using std::vector;
using std::pair;

struct Fsa {
  long start;
  vector<long> finals; // sorted
  vector<vector<pair<long, long>>> adj; // sorted

  long n() const { return adj.size(); }
  bool is_final(long x) const;
  void epsilon_closure(vector<long>& src) const;
  // DFA -> DFA
  Fsa operator~() const;
  // DFA -> DFA -> DFA
  Fsa operator&(const Fsa& rhs) const;
  // * -> * -> DFA
  Fsa operator|(const Fsa& rhs) const;
  // DFA -> DFA -> DFA
  Fsa operator-(const Fsa& rhs) const;
  // DFA -> DFA
  Fsa minimize() const;
  // * -> DFA
  Fsa determinize() const;
};
