#pragma once
#include <functional>
#include <utility>
#include <vector>
using std::function;
using std::pair;
using std::vector;

typedef pair<long, long> Label;
typedef pair<Label, long> Edge;

const Label epsilon{-1L, 0L};

struct Fsa {
  long start;
  vector<long> finals; // sorted
  vector<vector<Edge>> adj; // sorted

  long n() const { return adj.size(); }
  bool is_final(long x) const;
  bool has(long u, long a) const;
  bool has_special(long u) const;
  void epsilon_closure(vector<long>& src) const;
  Fsa operator~() const;
  // a -> a
  void accessible(function<void(long)> relate);
  // a -> a
  void co_accessible(function<void(long)> relate);
  // DFA -> DFA -> DFA
  Fsa intersect(const Fsa& rhs, function<void(long, long)> relate) const;
  // DFA -> DFA -> DFA
  Fsa difference(const Fsa& rhs, function<void(long)> relate) const;
  // DFA -> DFA
  Fsa distinguish(function<void(vector<long>&)> relate) const;
  // * -> DFA
  Fsa determinize(function<void(long, const vector<long>&)> relate) const;
};
