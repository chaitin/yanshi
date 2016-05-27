#include "fsa.hh"
#include <algorithm>
#include <iostream>
#include <type_traits>
using namespace std;

#define ALL(x) (x).begin(), (x).end()
#define FOR(i, a, b) for (remove_cv<remove_reference<decltype(b)>::type>::type i = (a); i < (b); i++)
#define REP(i, n) FOR(i, 0, n)

int main()
{
  long n, m, k, u, v;
  cin >> n >> m >> k;
  Fsa r;
  r.start = 0;
  r.adj.resize(n);
  while (k--) {
    cin >> u;
    r.finals.push_back(u);
  }
  sort(ALL(r.finals));
  while (m--) {
    char a;
    cin >> u >> a >> v;
    if (u < 0 || u >= n || v < 0 || v >= n)
      return 1;
    r.adj[u].emplace_back(a, v);
  }
  REP(i, n) {
    sort(ALL(r.adj[i]));
    if (r.adj[i].size())
      REP(j, r.adj[i].size()-1)
        if (r.adj[i][j].first == r.adj[i][j+1].first)
          return 1;
  }
  if (cin.bad()) return 2;

  r.hopcroft_minimize();
}
