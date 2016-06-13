#include "common.hh"
#include "fsa_anno.hh"
#include "option.hh"

#include <algorithm>
#include <limits.h>
#include <map>
#include <unicode/utf8.h>
#include <utility>
using namespace std;

bool operator<(ExprTag x, ExprTag y)
{
  return long(x) < long(y);
}

bool assoc_has_expr(vector<pair<Expr*, ExprTag>>& as, Expr* x)
{
  auto it = lower_bound(ALL(as), make_pair(x, ExprTag(0)));
  return it != as.end() && it->first == x;
}

void sort_assoc(vector<pair<Expr*, ExprTag>>& as)
{
  sort(ALL(as));
  auto i = as.begin(), j = i, k = i;
  for (; i != as.end(); i = j) {
    while (++j != as.end() && i->first == j->first)
      i->second = ExprTag(long(i->second) | long(j->second));
    *k++ = *i;
  }
  as.erase(k, as.end());
}

void FsaAnno::add_assoc(Expr& expr)
{
  // has actions: actions need tags to differentiate 'entering', 'leaving', ...
  // 'intact': states with the 'inner' tag cannot be connected to start/final in substring grammar
  // 'CollapseExpr': differentiate states representing 'CollapseExpr' (u, special, v)
  if (expr.no_action() && ! expr.stmt->intact && ! dynamic_cast<CollapseExpr*>(&expr))
    return;
  auto j = fsa.finals.begin();
  REP(i, fsa.n()) {
    ExprTag tag = ExprTag(0);
    if (i == fsa.start)
      tag = ExprTag::start;
    while (j != fsa.finals.end() && *j < i)
      ++j;
    if (j != fsa.finals.end() && *j == i)
      tag = ExprTag(long(tag) | long(ExprTag::final));
    if (tag == ExprTag(0))
      tag = ExprTag::inner;
    sorted_insert(assoc[i], make_pair(&expr, tag));
  }
}

void FsaAnno::accessible() {
  long allo = 0;
  auto relate = [&](long x) {
    if (allo != x)
      assoc[allo] = move(assoc[x]);
    allo++;
  };
  fsa.accessible(relate);
  assoc.resize(allo);
}

void FsaAnno::co_accessible() {
  long allo = 0;
  auto relate = [&](long x) {
    if (allo != x)
      assoc[allo] = move(assoc[x]);
    allo++;
  };
  fsa.co_accessible(relate);
  if (fsa.finals.empty()) { // 'start' does not produce acceptable strings
    assoc.assign(1, {});
    deterministic = true;
    return;
  }
  if (! deterministic)
    REP(i, fsa.n())
      sort(ALL(fsa.adj[i]));
  assoc.resize(allo);
}

void FsaAnno::complement(ComplementExpr* expr) {
  fsa = ~ fsa;
  assoc.assign(fsa.n(), {});
  // 'deterministic' is preserved
}

void FsaAnno::concat(FsaAnno& rhs, ConcatExpr* expr) {
  long ln = fsa.n(), rn = rhs.fsa.n();
  for (long f: fsa.finals)
    fsa.adj[f].emplace(fsa.adj[f].begin(), -1, ln+rhs.fsa.start);
  for (auto& es: rhs.fsa.adj) {
    for (auto& e: es)
      e.second += ln;
    fsa.adj.emplace_back(move(es));
  }
  fsa.finals = move(rhs.fsa.finals);
  for (long& f: fsa.finals)
    f += ln;
  assoc.resize(fsa.n());
  REP(i, rhs.fsa.n())
    assoc[ln+i] = move(rhs.assoc[i]);
  if (expr)
    add_assoc(*expr);
  deterministic = false;
}

void FsaAnno::determinize() {
  if (deterministic)
    return;
  decltype(assoc) new_assoc;
  auto relate = [&](long id, const vector<long>& xs) {
    if (id+1 > new_assoc.size())
      new_assoc.resize(id+1);
    auto& as = new_assoc[id];
    for (long x: xs)
      as.insert(as.end(), ALL(assoc[x]));
    sort_assoc(as);
  };
  fsa = fsa.determinize(relate);
  assoc = move(new_assoc);
  deterministic = true;
}

void FsaAnno::difference(FsaAnno& rhs, DifferenceExpr* expr) {
  vector<vector<long>> rel0;
  decltype(rhs.assoc) new_assoc;
  auto relate0 = [&](long id, const vector<long>& xs) {
    if (id+1 > rel0.size())
      rel0.resize(id+1);
    rel0[id] = xs;
  };
  auto relate = [&](long x) {
    if (rel0.empty())
      new_assoc.emplace_back(assoc[x]);
    else {
      new_assoc.emplace_back();
      auto& as = new_assoc.back();
      for (long u: rel0[x])
        as.insert(as.end(), ALL(assoc[u]));
      sort_assoc(as);
    }
  };
  if (! deterministic)
    fsa = fsa.determinize(relate0);
  if (! rhs.deterministic)
    rhs.fsa = rhs.fsa.determinize([](long, const vector<long>&) {});
  fsa = fsa.difference(rhs.fsa, relate);
  assoc = move(new_assoc);
  if (expr)
    add_assoc(*expr);
  deterministic = true;
}

FsaAnno FsaAnno::epsilon(EpsilonExpr* expr) {
  FsaAnno r;
  r.fsa.start = 0;
  r.fsa.finals.push_back(0);
  r.fsa.adj.resize(1);
  r.assoc.resize(1);
  if (expr)
    r.add_assoc(*expr);
  r.deterministic = true;
  return r;
}

void FsaAnno::intersect(FsaAnno& rhs, IntersectExpr* expr) {
  decltype(rhs.assoc) new_assoc;
  vector<vector<long>> rel0, rel1;
  auto relate0 = [&](long id, const vector<long>& xs) {
    if (id+1 > rel0.size())
      rel0.resize(id+1);
    rel0[id] = xs;
  };
  auto relate1 = [&](long id, const vector<long>& xs) {
    if (id+1 > rel1.size())
      rel1.resize(id+1);
    rel1[id] = xs;
  };
  auto relate = [&](long x, long y) {
    new_assoc.emplace_back();
    auto& as = new_assoc.back();
    if (rel0.empty())
      as.insert(as.end(), ALL(assoc[x]));
    else
      for (long u: rel0[x])
        as.insert(as.end(), ALL(assoc[u]));
    if (rel1.empty())
      as.insert(as.end(), ALL(rhs.assoc[y]));
    else
      for (long v: rel1[y])
        as.insert(as.end(), ALL(rhs.assoc[v]));
    sort_assoc(as);
  };
  if (! deterministic)
    fsa = fsa.determinize(relate0);
  if (! rhs.deterministic)
    rhs.fsa = rhs.fsa.determinize(relate1);
  fsa = fsa.intersect(rhs.fsa, relate);
  assoc = move(new_assoc);
  if (expr)
    add_assoc(*expr);
  deterministic = true;
}

void FsaAnno::minimize() {
  assert(deterministic);
  decltype(assoc) new_assoc;
  auto relate = [&](vector<long>& xs) {
    new_assoc.emplace_back();
    auto& as = new_assoc.back();
    for (long x: xs)
      as.insert(as.end(), ALL(assoc[x]));
    sort_assoc(as);
  };
  fsa = fsa.distinguish(relate);
  assoc = move(new_assoc);
}

void FsaAnno::union_(FsaAnno& rhs, UnionExpr* expr) {
  long ln = fsa.n(), rn = rhs.fsa.n(), src = ln+rn,
       old_lsrc = fsa.start;
  fsa.start = src;
  for (long f: rhs.fsa.finals)
    fsa.finals.push_back(ln+f);
  for (auto& es: rhs.fsa.adj) {
    for (auto& e: es)
      e.second += ln;
    fsa.adj.emplace_back(move(es));
  }
  fsa.adj.emplace_back();
  fsa.adj[src].emplace_back(-1, old_lsrc);
  fsa.adj[src].emplace_back(-1, ln+rhs.fsa.start);
  assoc.resize(fsa.n());
  REP(i, rhs.fsa.n())
    assoc[ln+i] = move(rhs.assoc[i]);
  if (expr)
    add_assoc(*expr);
  deterministic = false;
}

void FsaAnno::plus(PlusExpr* expr) {
  for (long f: fsa.finals)
    sorted_insert(fsa.adj[f], make_pair(-1L, fsa.start));
  if (expr)
    add_assoc(*expr);
  deterministic = false;
}

void FsaAnno::question(QuestionExpr* expr) {
  long src = fsa.n(), sink = src+1, old_src = fsa.start;
  fsa.start = src;
  fsa.adj.emplace_back();
  fsa.adj.emplace_back();
  fsa.adj[src].emplace_back(-1, old_src);
  fsa.adj[src].emplace_back(-1, sink);
  fsa.finals.push_back(sink);
  assoc.resize(fsa.n());
  if (expr)
    add_assoc(*expr);
  deterministic = false;
}

void FsaAnno::repeat(RepeatExpr& expr) {
  FsaAnno r = epsilon(NULL);
  REP(i, expr.low) {
    FsaAnno t = *this;
    r.concat(t, NULL);
  }
  if (expr.high == LONG_MAX) {
    star(NULL);
    r.concat(*this, NULL);
  } else if (expr.low < expr.high) {
    FsaAnno rhs = epsilon(NULL), x = *this;
    ROF(i, 0, expr.high-expr.low) {
      FsaAnno t = x;
      rhs.union_(t, NULL);
      if (i) {
        t = *this;
        x.concat(t, NULL);
      }
    }
    r.concat(rhs, NULL);
  }
  r.deterministic = false;
  *this = move(r);
}

void FsaAnno::star(StarExpr* expr) {
  long src = fsa.n(), sink = src+1, old_src = fsa.start;
  fsa.start = src;
  fsa.adj.emplace_back();
  fsa.adj.emplace_back();
  fsa.adj[src].emplace_back(-1, old_src);
  fsa.adj[src].emplace_back(-1, sink);
  for (long f: fsa.finals) {
    sorted_insert(fsa.adj[f], make_pair(-1L, old_src));
    sorted_insert(fsa.adj[f], make_pair(-1L, sink));
  }
  fsa.finals.assign(1, sink);
  assoc.resize(fsa.n());
  if (expr)
    add_assoc(*expr);
  deterministic = false;
}

FsaAnno FsaAnno::bracket(BracketExpr& expr) {
  FsaAnno r;
  r.fsa.start = 0;
  r.fsa.finals = {1};
  r.fsa.adj.resize(2);
  for (auto& x: expr.intervals.to)
    FOR(c, x.first, x.second)
      r.fsa.adj[0].emplace_back(c, 1);
  r.assoc.resize(2);
  r.add_assoc(expr);
  r.deterministic = true;
  return r;
}

FsaAnno FsaAnno::collapse(CollapseExpr& expr) {
  // represented by (0, special, 1)
  static long label = AB;
  FsaAnno r;
  r.fsa.start = 0;
  r.fsa.finals = {1};
  r.fsa.adj.resize(2);
  r.fsa.adj[0].emplace_back(label++, 1);
  r.assoc.resize(2);
  r.add_assoc(expr);
  r.deterministic = true;
  return r;
}

FsaAnno FsaAnno::dot(DotExpr* expr) {
  FsaAnno r;
  r.fsa.start = 0;
  r.fsa.finals = {1};
  r.fsa.adj.resize(2);
  REP(c, AB)
    r.fsa.adj[0].emplace_back(c, 1);
  r.assoc.resize(2);
  if (expr)
    r.add_assoc(*expr);
  r.deterministic = true;
  return r;
}

FsaAnno FsaAnno::literal(LiteralExpr& expr) {
  FsaAnno r;
  r.fsa.start = 0;
  r.fsa.finals.assign(1, expr.literal.size());
  r.fsa.adj.resize(expr.literal.size()+1);
  REP(i, expr.literal.size())
    r.fsa.adj[i].emplace_back((unsigned char)expr.literal[i], i+1);
  r.assoc.resize(expr.literal.size()+1);
  r.add_assoc(expr);
  r.deterministic = true;
  return r;
}

void FsaAnno::substring_grammar() {
  long src = fsa.n(), sink = src+1, old_src = fsa.start;
  fsa.start = src;
  fsa.adj.emplace_back();
  fsa.adj.emplace_back();
  REP(i, src) {
    bool ok = true;
    for (auto aa: assoc[i])
      if (auto e = dynamic_cast<CollapseExpr*>(aa.first)) {
        if (e->define_stmt->intact && long(aa.second) & long(ExprTag::inner)) {
          ok = false;
          break;
        }
      } else if (aa.first->stmt->intact && long(aa.second) & long(ExprTag::inner)) {
        ok = false;
        break;
      }
    if (ok || i == old_src)
      fsa.adj[src].emplace_back(-1, i);
    if (ok || fsa.is_final(i))
      sorted_insert(fsa.adj[i], make_pair(-1L, sink));
  }
  fsa.finals.assign(1, sink);
  assoc.resize(fsa.n());
  deterministic = false;
}

FsaAnno FsaAnno::unicode_range(UnicodeRangeExpr& expr) {
  FsaAnno r;
  long n = 0;
  struct Trie {
    long id, refcnt = 1;
    map<int, Trie*> ch;
    ~Trie() {
      for (auto c: ch)
        if (! --c.second->refcnt)
          delete c.second;
    }
  } root;
  root.id = n++;
  r.fsa.start = 0;

  // build Trie
  Trie* last = NULL;
  FOR(i, expr.start, expr.end) {
    u8 s[4];
    long len = 0;
    U8_APPEND_UNSAFE(s, len, i);
    Trie *x = &root, *y;
    REP(j, len) {
      auto it = x->ch.find(s[j]);
      if (it == x->ch.end()) {
        if (j == len-1 && last) {
          y = last;
          y->refcnt++;
        } else {
          y = new Trie;
          y->id = n++;
        }
        x->ch[s[j]] = y;
        x = y;
      } else
        x = it->second;
    }
    if (! last)
      last = x;
    r.fsa.finals.push_back(x->id);
  }

  // insert edges
  r.fsa.adj.resize(n);
  function<void(Trie*)> dfs = [&](Trie* x) {
    for (auto& c: x->ch) {
      r.fsa.adj[x->id].emplace_back(c.first, c.second->id);
      dfs(c.second);
    }
  };
  dfs(&root);
  sort(ALL(r.fsa.finals));

  r.assoc.resize(n);
  r.add_assoc(expr);
  r.deterministic = true;
  return r;
}
