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
  void product(const Fsa& rhs, vector<pair<long, long>>& states, vector<vector<pair<long, long>>>& res_adj) const;
  Fsa operator~() const;
  Fsa operator&(const Fsa& rhs) const;
  Fsa operator|(const Fsa& rhs) const;
  Fsa operator-(const Fsa& rhs) const;
  void hopcroft_minimize();
  Fsa determinize() const;
};
