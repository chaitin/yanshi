#pragma once
#include <functional>
#include <utility>
#include <vector>
using std::function;
using std::pair;
using std::vector;

struct Fsa {
  long start;
  vector<long> finals; // sorted
  vector<vector<pair<long, long>>> adj; // sorted

  long n() const { return adj.size(); }
  bool is_final(long x) const;
  bool has(long u, long a) const;
  void epsilon_closure(vector<long>& src) const;
  Fsa operator~() const;
  // DFA -> DFA -> DFA
  Fsa intersect(const Fsa& rhs, function<void(long, long)> relate) const;
  // DFA -> DFA -> DFA
  Fsa difference(const Fsa& rhs, function<void(long)> relate) const;
  // DFA -> DFA
  Fsa distinguish(function<void(vector<long>&)> relate) const;
  // * -> DFA
  Fsa determinize(function<void(const vector<long>&)> relate) const;
  // a -> a
  void remove_dead(function<void(long)> relate);
};
