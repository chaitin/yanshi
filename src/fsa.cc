#include "common.hh"
#include "fsa.hh"

#include <algorithm>
#include <limits.h>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
using namespace std;

namespace std
{
  template<typename T>
  struct hash<vector<T>> {
    size_t operator()(const vector<T>& v) const {
      hash<T> h;
      size_t r = 0;
      for (auto x: v)
        r = r*17+h(x);
      return r;
    }
  };
}

bool Fsa::is_final(long x) const
{
  return binary_search(ALL(finals), x);
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
  sort(ALL(src));
}

Fsa Fsa::operator-(const Fsa& rhs) const
{
  Fsa r;
  vector<pair<long, long>> q;
  long u0, u1, v0, v1;
  unordered_map<long, long> m;
  q.emplace_back(start, rhs.start);
  m[(rhs.n()+1) * start + rhs.start] = 0;
  r.start = 0;
  REP(i, q.size()) {
    tie(u0, u1) = q[i];
    if (is_final(u0) && ! rhs.is_final(u1))
      r.finals.push_back(i);
    r.adj.emplace_back();
    vector<pair<long, long>>::const_iterator it0 = adj[u0].begin(), it1, it1e;
    if (u1 == rhs.n())
      it1 = it1e = rhs.adj[0].end();
    else {
      it1 = rhs.adj[u1].begin();
      it1e = rhs.adj[u1].end();
    }
    for (; it0 != adj[u0].end(); ++it0) {
      while (it1 != it1e && it1->first < it0->first)
        ++it1;
      long v1 = it1 != it1e || it1->first == it1e->first ? it1->second : rhs.n(),
           t = (rhs.n()+1) * it0->second + v1;
      auto mit = m.find(t);
      if (mit == m.end()) {
        mit = m.emplace(t, m.size()).first;
        q.emplace_back(it0->second, it1->second);
      }
      r.adj[i].emplace_back(it0->first, mit->second);
    }
  }
  return r;
}

Fsa Fsa::operator&(const Fsa& rhs) const
{
  Fsa r;
  vector<pair<long, long>> q;
  long u0, u1, v0, v1;
  unordered_map<long, long> m;
  q.emplace_back(start, rhs.start);
  m[rhs.n() * start + rhs.start] = 0;
  r.start = 0;
  REP(i, q.size()) {
    tie(u0, u1) = q[i];
    if (is_final(u0) && rhs.is_final(u1))
      r.finals.push_back(i);
    r.adj.emplace_back();
    auto it0 = adj[u0].begin(), it1 = rhs.adj[u1].begin();
    while (it0 != adj[u0].end() && it1 != rhs.adj[u1].end()) {
      if (it0->first < it1->first)
        ++it0;
      else if (it0->first > it1->first)
        ++it1;
      else {
        long t = rhs.n() * it0->second + it1->second;
        auto mit = m.find(t);
        if (mit == m.end()) {
          mit = m.emplace(t, m.size()).first;
          q.emplace_back(it0->second, it1->second);
        }
        r.adj[i].emplace_back(it0->first, mit->second);
        ++it0;
        ++it1;
      }
    }
  }
  return r;
}

Fsa Fsa::operator|(const Fsa& rhs) const
{
  Fsa r;
  r.start = n()+rhs.n();
  r.finals = finals;
  for (long i: rhs.finals)
    r.finals.push_back(n()+i);
  r.adj = adj;
  r.adj.resize(r.start+1);
  r.adj[r.start].emplace_back(-1, 0);
  r.adj[r.start].emplace_back(-1, n());
  REP(i, rhs.n())
    for (auto& e: rhs.adj[i])
      r.adj[n()+i].emplace_back(e.first, n()+e.second);
  return r.determinize().minimize();
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
  unordered_map<vector<long>, long> m;
  vector<vector<long>> q{{start}};
  vector<vector<pair<long, long>>::const_iterator> its(n());
  vector<long> vs;
  epsilon_closure(q[0]);
  m[q[0]] = 0;
  r.start = 0;
  REP(i, q.size()) {
    bool final = false;
    for (long u: q[i]) {
      if (binary_search(ALL(finals), u))
        final = true;
      its[u] = adj[u].begin();
      // skip epsilon
      while (its[u] != adj[u].end() && its[u]->first < 0)
        ++its[u];
    }
    if (final)
      r.finals.push_back(i);
    r.adj.emplace_back();
    for(;;) {
      // find minimum transition
      long c = LONG_MAX;
      for (long u: q[i])
        if (its[u] != adj[u].end())
          c = min(c, its[u]->first);
      if (c == LONG_MAX) break;
      // successors
      vs.clear();
      for (long u: q[i])
        for (; its[u] != adj[u].end() && its[u]->first == c; ++its[u])
          vs.push_back(its[u]->second);
      sort(ALL(vs));
      vs.erase(unique(ALL(vs)), vs.end());
      epsilon_closure(vs);
      auto mit = m.find(vs);
      if (mit == m.end()) {
        mit = m.emplace(vs, m.size()).first;
        q.push_back(vs);
      }
      r.adj[i].emplace_back(c, mit->second);
    }
  }
  return r;
}

Fsa Fsa::minimize() const
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
      refines.emplace(a, fy < 0 || fx >= 0 && C[fx] < C[fy] ? fx : fy);
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

  Fsa r;
  long nn = 0;
  REP(i, n())
    if (B[i] == i) {
      for (long j = i; ; ) {
        B[j] = nn;
        if ((j = R[j]) == i) break;
      }
      if (binary_search(ALL(finals), i))
        r.finals.push_back(nn);
      nn++;
    }
  r.start = B[start];
  r.adj.resize(nn);
  REP(i, n())
    for (auto& e: adj[i])
      r.adj[B[i]].emplace_back(e.first, B[e.second]);
  REP(i, nn) {
    sort(ALL(r.adj[i]));
    r.adj[i].erase(unique(ALL(r.adj[i])), r.adj[i].end());
  }
  return r;

  //REP(i, n())
  //  if (B[i] == i) {
  //    printf("%ld:", i);
  //    for (long j = i; ; ) {
  //      printf(" %ld", j);
  //      if ((j = R[j]) == i) break;
  //    }
  //    puts("");
  //  }
}
