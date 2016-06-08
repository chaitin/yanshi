#include "common.hh"
#include "fsa.hh"

#include <algorithm>
#include <limits.h>
#include <queue>
#include <set>
#include <stack>
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

bool Fsa::has(long u, long a) const
{
  auto it = lower_bound(ALL(adj[u]), make_pair(a, 0L));
  return it != adj[u].end() && it->first == a;
}

bool Fsa::is_final(long x) const
{
  return binary_search(ALL(finals), x);
}

void Fsa::epsilon_closure(vector<long>& src) const
{
  static vector<bool> vis;
  if (n() > vis.size())
    vis.resize(n());
  for (long i: src)
    vis[i] = true;
  REP(i, src.size()) {
    long u = src[i];
    for (auto& e: adj[u]) {
      if (-1 < e.first) break;
      if (! vis[e.second]) {
        vis[e.second] = true;
        src.push_back(e.second);
      }
    }
  }
  for (long i: src)
    vis[i] = false;
  sort(ALL(src));
}

Fsa Fsa::operator~() const
{
  long accept = n();
  Fsa r;
  r.start = start;
  r.adj.resize(accept+1);
  REP(i, accept) {
    long j = 0;
    for (auto& e: adj[i]) {
      for (; j < e.first; j++)
        r.adj[i].emplace_back(j, accept);
      r.adj[i].emplace_back(e.first, e.second);
      j = e.first+1;
    }
    for (; j < AB; j++)
      r.adj[i].emplace_back(j, accept);
  }
  r.adj.emplace_back();
  REP(i, AB)
    r.adj[accept].emplace_back(i, accept);
  vector<long> new_finals;
  auto j = finals.begin();
  REP(i, accept+1) {
    while (j != finals.end() && *j < i)
      ++j;
    if (j == finals.end() || *j != i)
      new_finals.push_back(i);
  }
  r.finals = move(new_finals);
  return r;
}

void Fsa::accessible(function<void(long)> relate)
{
  long nid = 0;
  vector<long> q{start}, id(n(), -1);
  id[start] = nid++;
  REP(i, q.size()) {
    long u = q[i];
    for (auto& e: adj[u])
      if (id[e.second] < 0) {
        id[e.second] = nid++;
        q.push_back(e.second);
      }
  }

  auto it = finals.begin(), it2 = it;
  REP(i, n())
    if (id[i] >= 0) {
      relate(i);
      if (start == i)
        start = id[i];
      while (it != finals.end() && *it < i)
        ++it;
      if (it != finals.end() && *it == i)
        *it2++ = id[i];
      long k = 0;
      for (auto& e: adj[i])
        if (id[e.second] >= 0)
          adj[i][k++] = {e.first, id[e.second]}; // unordered unless deterministic
      adj[i].resize(k);
      if (id[i] != i)
        adj[id[i]] = move(adj[i]);
    }
  finals.erase(it2, finals.end());
  adj.resize(nid);
}

void Fsa::co_accessible(function<void(long)> relate)
{
  vector<vector<pair<long, long>>> radj(n());
  REP(i, n())
    for (auto& e: adj[i])
      radj[e.second].emplace_back(e.first, i);
  REP(i, n())
    sort(ALL(radj[i]));
  vector<long> q = finals, id(n(), 0);
  for (long f: finals)
    id[f] = 1;
  REP(i, q.size()) {
    long u = q[i];
    for (auto& e: radj[u])
      if (! id[e.second]) {
        id[e.second] = 1;
        q.push_back(e.second);
      }
  }
  if (! id[start]) {
    start = 0;
    finals.clear();
    adj.assign(1, {});
    return;
  }

  long j = 0;
  REP(i, n())
    id[i] = id[i] ? j++ : -1;

  auto it = finals.begin(), it2 = it;
  REP(i, n())
    if (id[i] >= 0) {
      relate(i);
      if (start == i)
        start = id[i];
      while (it != finals.end() && *it < i)
        ++it;
      if (it != finals.end() && *it == i)
        *it2++ = id[i];
      long k = 0;
      for (auto& e: adj[i])
        if (id[e.second] >= 0)
          adj[i][k++] = {e.first, id[e.second]}; // unordered unless deterministic
      adj[i].resize(k);
      if (id[i] != i)
        adj[id[i]] = move(adj[i]);
    }
  finals.erase(it2, finals.end());
  adj.resize(j);
}

Fsa Fsa::difference(const Fsa& rhs, function<void(long)> relate) const
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
    relate(u0);
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
      long v1 = it1 != it1e && it1->first == it0->first ? it1->second : rhs.n(),
           t = (rhs.n()+1) * it0->second + v1;
      auto mit = m.find(t);
      if (mit == m.end()) {
        mit = m.emplace(t, m.size()).first;
        q.emplace_back(it0->second, v1);
      }
      r.adj[i].emplace_back(it0->first, mit->second);
    }
  }
  return r;
}

Fsa Fsa::intersect(const Fsa& rhs, function<void(long, long)> relate) const
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
    relate(u0, u1);
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

Fsa Fsa::determinize(function<void(const vector<long>&)> relate) const
{
  Fsa r;
  r.start = 0;
  unordered_map<vector<long>, long> m;
  vector<vector<pair<long, long>>::const_iterator> its(n());
  vector<long> vs;
  vector<long> initial{start};
  epsilon_closure(initial);
  m[initial] = 0;
  stack<vector<long>> st;
  st.push(move(initial));
  while (st.size()) {
    vector<long> x = move(st.top());
    st.pop();
    long id = m[x];
    if (id+1 > r.adj.size())
      r.adj.resize(id+1);
    relate(x);
    if (id % 100 == 0)
      DP(5, "%ld %ld", id, x.size());
    bool final = false;
    for (long u: x) {
      if (is_final(u))
        final = true;
      its[u] = adj[u].begin();
      // skip epsilon
      while (its[u] != adj[u].end() && its[u]->first < 0)
        ++its[u];
    }
    if (final)
      r.finals.push_back(id);
    for(;;) {
      // find minimum transition
      long c = LONG_MAX;
      for (long u: x)
        if (its[u] != adj[u].end())
          c = min(c, its[u]->first);
      if (c == LONG_MAX) break;
      // successors
      vs.clear();
      for (long u: x)
        for (; its[u] != adj[u].end() && its[u]->first == c; ++its[u])
          vs.push_back(its[u]->second);
      sort(ALL(vs));
      vs.erase(unique(ALL(vs)), vs.end());
      epsilon_closure(vs);
      auto mit = m.find(vs);
      if (mit == m.end()) {
        mit = m.emplace(vs, m.size()).first;
        st.push(vs);
      }
      r.adj[id].emplace_back(c, mit->second);
    }
  }
  sort(ALL(r.finals));
  return r;
}

Fsa Fsa::distinguish(function<void(vector<long>&)> relate) const
{
  vector<vector<pair<long, long>>> radj(n());
  REP(i, n())
    for (auto& e: adj[i])
      radj[e.second].emplace_back(e.first, i);
  REP(i, n())
    sort(ALL(radj[i]));

  vector<long> L(n()), R(n()), B(n()), C(n(), 0), CC(n(), 0);
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
  if (fx >= 0)
    REP(a, AB+1)
      refines.emplace(a, fx);
  if (fy >= 0)
    REP(a, AB+1)
      refines.emplace(a, fy);
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
    for (long fy: bs) {
      if (CC[fy] < C[fy]) {
        long fu = -1, u = -1, cu = 0,
             fv = -1, v = -1, cv = 0;
        for (long i = fy; ; ) {
          if (mark[i]) {
            mark[i] = false;
            if (u < 0)
              C[fu = i] = 0;
            else
              R[u] = i;
            C[fu]++;
            B[i] = fu;
            L[i] = u;
            u = i;
          } else {
            if (v < 0)
              C[fv = i] = 0;
            else
              R[v] = i;
            C[fv]++;
            B[i] = fv;
            L[i] = v;
            v = i;
          }
          if ((i = R[i]) == fy) break;
        }
        L[fu] = u, R[u] = fu;
        L[fv] = v, R[v] = fv;
        REP(a, AB+1)
          if (refines.count({a, fy}))
            refines.emplace(a, fu != fy ? fu : fv);
          else
            refines.emplace(a, C[fu] < C[fv] ? fu : fv);
      } else
        for (long i = fy; ; ) {
          mark[i] = false;
          if ((i = R[i]) == fy) break;
        }
      CC[fy] = 0;
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
  vector<long> vs;
  REP(i, n())
    if (B[i] == i) {
      vs.clear();
      for (long j = i; ; ) {
        B[j] = nn;
        vs.push_back(j);
        if ((j = R[j]) == i) break;
      }
      relate(vs);
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
