#include "common.hh"
#include "fsa.hh"

#include <algorithm>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
using namespace std;

bool Fsa::is_final(long x) const
{
  return lower_bound(ALL(finals), x) != finals.end();
}

void Fsa::epsilon_closure(vector<long>& src) const
{
  unordered_set<long> vis{ALL(src)};
  for (long i = 0; i < src.size(); i++) {
    long u = src[i];
    for (auto& e: adj[u]) {
      if (-1 < e.first) break;
      if (! vis.count(e.second)) {
        vis.insert(e.second);
        src.push_back(e.second);
      }
    }
  }
}

void Fsa::product(const Fsa& rhs, vector<pair<long, long>>& nodes, vector<vector<pair<long, long>>>& edges) const
{
  long u0, u1, v0, v1;
  unordered_map<long, long> m;
  nodes.emplace_back(start, rhs.start);
  m[rhs.n() * start + rhs.start] = 0;
  REP(i, nodes.size()) {
    tie(u0, u0) = nodes[i];
    auto it0 = adj[u0].begin(), it1 = rhs.adj[u1].begin();
    while (it0 != adj[u0].end() && it1 != rhs.adj[u1].end()) {
      if (it0->first < it1->first)
        ++it0;
      else if (it0->first > it1->first)
        ++it1;
      else {
        long t = rhs.n() * it0->second + it1->second;
        if (m.count(t)) {
          long id = m.size();
          m[t] = id;
          nodes.emplace_back(it0->second, it1->second);
        }
        edges[i].emplace_back(it0->first, m[t]);
        ++it0;
        ++it1;
      }
    }
  }
}

Fsa Fsa::operator&(const Fsa& rhs) const
{
  Fsa r;
  r.start = 0;
  vector<pair<long, long>> nodes;
  product(rhs, nodes, r.adj);
  REP(i, nodes.size())
    if (is_final(nodes[i].first) && rhs.is_final(nodes[i].second))
      r.finals.push_back(i);
  return r;
}

Fsa Fsa::operator|(const Fsa& rhs) const
{
  Fsa r;
  r.start = 0;
  vector<pair<long, long>> nodes;
  product(rhs, nodes, r.adj);
  REP(i, nodes.size())
    if (is_final(nodes[i].first) || rhs.is_final(nodes[i].second))
      r.finals.push_back(i);
  return r;
}

Fsa Fsa::operator~() const
{
  Fsa r;
  r.adj.resize(n()+1);
  r.start = start;
  REP(i, n()) {
    long last = 0;
    for (auto& e: adj[i]) {
      for (; last < e.first; last++)
        r.adj[i].emplace_back(last, n());
      r.adj[i].emplace_back(e.first, e.second);
    }
  }
  REP(i, 256)
    r.adj[n()].emplace_back(i, n());
  long j = 0;
  REP(i, n()+1)
    if (j < finals.size() && i == finals[j])
      j++;
    else
      r.finals.push_back(i);
  return r;
}

Fsa Fsa::determinize() const
{
  Fsa r;
  return r;
}

void Fsa::hopcroft_minimize()
{
  vector<vector<pair<long, long>>> radj(n());
  REP(i, n())
    for (auto& e: adj[i])
      radj[e.second].emplace_back(e.first, i);
  REP(i, n())
    sort(ALL(radj[i]));

  vector<long> L(n()), R(n()), B(n()), C(n(), 0), CC(n());
  vector<bool> mark(n(), false);
  long fx = -1, x = -1, fy = -1, y = -1, j = 0;
  REP(i, n())
    if (j < finals.size() && finals[j] == i) {
      j++;
      if (y < 0)
        fy = i;
      else
        R[y] = i;
      C[B[i] = fy]++;
      L[i] = y;
      y = i;
    } else {
      if (x < 0)
        fx = i;
      else
        R[x] = i;
      C[B[i] = fx]++;
      L[i] = x;
      x = i;
    }
  if (x >= 0)
    L[fx] = x, R[x] = fx;
  if (y >= 0)
    L[fy] = y, R[y] = fy;
  set<pair<long, long>> refines;
  if (x >= 0 || y >= 0)
    // insert (a, min(finals, non-finals))
    REP(a, 256)
      refines.emplace(a, fx >= 0 && C[fx] < C[fy] ? fx : fy);
  while (refines.size()) {
    long a;
    tie(a, fx) = *refines.begin();
    refines.erase(refines.begin());
    // count
    vector<long> bs;
    for (x = fx; ; ) {
      auto it = lower_bound(ALL(radj[x]), make_pair(a, 0L)),
           ite = upper_bound(ALL(radj[x]), make_pair(a, n()));
      for (; it != ite; ++it) {
        y = it->second;
        if (! CC[B[y]]++)
          bs.push_back(B[y]);
        mark[y] = true;
      }
      if ((x = R[x]) == fx) break;
    }
    // for each refinable set
    for (long fy: bs)
      if (CC[fy] < C[fy]) {
        long fu = -1, u = -1, cu = 0,
             fv = -1, v = -1, cv = 0;
        for (long i = fy; ; ) {
          if (mark[i]) {
            if (u < 0)
              C[fu = i] = 0;
            else
              R[fu] = i;
            C[fu]++;
            B[i] = fu;
            L[i] = u;
            u = i;
          } else {
            if (v < 0)
              C[fv = i] = 0;
            else
              R[fv] = i;
            C[fv]++;
            B[i] = fv;
            L[i] = v;
            v = i;
          }
          if ((i = R[i]) == fy) break;
        }
        L[fu] = u, R[u] = fu;
        L[fv] = v, R[v] = fv;
        REP(a, 256) {
          pair<long, long> t{a, fu != y ? fu : fv};
          if (refines.count({a, fy}))
            refines.emplace(a, fu != fy ? fu : fv);
          else
            refines.emplace(a, C[fu] < C[fv] ? fu : fv);
        }
      }
    // clear marks
    for (x = fx; ; ) {
      auto it = lower_bound(ALL(radj[x]), make_pair(a, 0L)),
           ite = upper_bound(ALL(radj[x]), make_pair(a, n()));
      for (; it != ite; ++it) {
        y = it->second;
        CC[B[y]] = 0;
        mark[y] = false;
      }
      if ((x = R[x]) == fx) break;
    }
  }

  REP(i, n())
    if (B[i] == i) {
      printf("%ld:", i);
      for (long j = i; ; ) {
        printf(" %ld", j);
        if ((j = R[j]) == i) break;
      }
      puts("");
    }
}
