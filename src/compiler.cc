#include "compiler.hh"
#include "fsa_anno.hh"
#include "loader.hh"
#include "option.hh"

#include <algorithm>
#include <ctype.h>
#include <limits.h>
#include <map>
#include <stack>
#include <typeinfo>
#include <unordered_map>
using namespace std;

static map<DefineStmt*, FsaAnno> compiled;

static void print_assoc(const FsaAnno& anno)
{
  REP(i, anno.fsa.n()) {
    printf("%ld:", i);
    for (auto a: anno.assoc[i]) {
      const char* name = typeid(*a).name();
      while (name && isdigit(name[0]))
        name++;
      string t = name;
      t = t.substr(t.size()-4); // suffix 'Expr'
      printf(" %s(%ld-%ld)", t.c_str(), a->loc.start, a->loc.end);
    }
    puts("");
  }
}

static void print_automaton(const Fsa& fsa)
{
  green(); printf("start: %ld\n", fsa.start);
  red(); printf("finals:");
  for (long i: fsa.finals)
    printf(" %ld", i);
  puts("");
  sgr0(); puts("edges:");
  REP(i, fsa.n()) {
    printf("%ld:", i);
    for (auto& x: fsa.adj[i])
      printf(" (" YELLOW"%ld" SGR0",%ld)", x.first, x.second);
    puts("");
  }
}

Expr* find_lca(Expr* u, Expr* v)
{
  if (u->depth > v->depth)
    swap(u, v);
  if (u->depth < v->depth)
    for (long k = 63-__builtin_clzl(v->depth-u->depth); k >= 0; k--)
      if (u->depth <= v->depth-(1L<<k))
        v = v->anc[k];
  if (u == v)
    return u;
  for (long k = 63-__builtin_clzl(v->depth); k >= 0; k--)
    if (u->anc[k] != v->anc[k])
      u = u->anc[k], v = v->anc[k];
  return u->anc[0];
}

struct Compiler : Visitor<Expr> {
  stack<FsaAnno> st;
  stack<Expr*> path;
  long tick = 0;

  void pre_expr(Expr& expr) {
    expr.pre = tick++;
    expr.depth = path.size();
    if (path.size()) {
      expr.anc.assign(1, path.top());
      for (long k = 1; 1L << k <= expr.depth; k++)
        expr.anc.push_back(expr.anc[k-1]->anc[k-1]);
    } else
      expr.anc.assign(1, nullptr);
    path.push(&expr);
  }
  void post_expr(Expr& expr) {
    path.pop();
    expr.post = tick;
  }

  void visit(Expr& expr) override {
    pre_expr(expr);
    expr.accept(*this);
    post_expr(expr);
  }
  void visit(BracketExpr& expr) override {
    st.push(FsaAnno::bracket(expr));
  }
  void visit(CollapseExpr& expr) override {
    st.push(FsaAnno::collapse(expr));
  }
  void visit(ConcatExpr& expr) override {
    visit(*expr.rhs);
    FsaAnno rhs = move(st.top());
    visit(*expr.lhs);
    path.pop();
    st.top().concat(rhs);
  }
  void visit(DifferenceExpr& expr) override {
    visit(*expr.rhs);
    FsaAnno rhs = move(st.top());
    visit(*expr.lhs);
    st.top().difference(rhs);
  }
  void visit(DotExpr& expr) override {
    st.push(FsaAnno::dot(expr));
  }
  void visit(EmbedExpr& expr) override {
    st.push(compiled[expr.define_stmt]);
  }
  void visit(IntersectExpr& expr) override {
    visit(*expr.rhs);
    FsaAnno rhs = move(st.top());
    visit(*expr.lhs);
    st.top().intersect(rhs);
  }
  void visit(LiteralExpr& expr) override {
    st.push(FsaAnno::literal(expr));
  }
  void visit(PlusExpr& expr) override {
    visit(*expr.inner);
    st.top().plus();
  }
  void visit(QuestionExpr& expr) override {
    visit(*expr.inner);
    st.top().question(expr);
  }
  void visit(StarExpr& expr) override {
    visit(*expr.inner);
    st.top().star(expr);
  }
  void visit(UnionExpr& expr) override {
    visit(*expr.rhs);
    FsaAnno rhs = move(st.top());
    visit(*expr.lhs);
    st.top().union_(rhs, expr);
  }
};

void compile(DefineStmt* stmt)
{
  if (compiled.count(stmt))
    return;
  FsaAnno& anno = compiled[stmt];
  Compiler comp;
  comp.visit(*stmt->rhs);
  anno = move(comp.st.top());
}

void compile_actions(FsaAnno& anno)
{
  REP(i, anno.fsa.n())
    sort(ALL(anno.assoc[i]), [](const Expr* x, const Expr* y) {
      return x->pre < y->pre;
    });
  REP(u, anno.fsa.n()) {
    for (auto& e: anno.fsa.adj[u]) {
      long v = e.second;
      if (anno.fsa.is_final(v)) {
        Expr* last = NULL;
        for (auto a: anno.assoc[v]) {
          Expr* stop = last ? find_lca(last, a) : NULL;
          last = a;
          for (Expr* x = a; a != stop; a = a->anc[0])
            for (auto action: x->finishing) {
              if (auto t = dynamic_cast<InlineAction*>(action)) {
                printf("%ld %ld %ld %s\n", u, e.first, v, t->code.c_str());
              } else if (auto t = dynamic_cast<RefAction*>(action)) {
                printf("%ld %ld %ld %s\n", u, e.first, v, t->define_module->defined_action[t->ident].c_str());
              }
            }
        }
      }
    }
  }
}

void export_statement(DefineStmt* stmt)
{
  DP(2, "Exporting %s", stmt->lhs.c_str());
  FsaAnno& anno = compiled[stmt];

  DP(3, "Construct automaton with all referenced CollapseExpr's DefineStmt");
  vector<vector<pair<long, long>>> adj;
  vector<vector<Expr*>> assoc;
  vector<vector<DefineStmt*>> cllps;
  long allo = 0;
  unordered_map<DefineStmt*, long> stmt2offset;
  function<void(DefineStmt*)> allocate_collapse = [&](DefineStmt* stmt) {
    if (stmt2offset.count(stmt))
      return;
    DP(4, "Allocate %ld to %s", allo, stmt->lhs.c_str());
    FsaAnno& anno = compiled[stmt];
    long old = stmt2offset[stmt] = allo;
    allo += anno.fsa.n()+1;
    adj.insert(adj.end(), ALL(anno.fsa.adj));
    REP(i, anno.fsa.n())
      for (auto& e: adj[old+i])
        e.second += old;
    adj.emplace_back(); // appeared in other automata, this is a vertex corresponding to the completion of 'stmt'
    assoc.insert(assoc.end(), ALL(anno.assoc));
    assoc.emplace_back();
    FOR(i, old, old+anno.fsa.n())
      if (anno.fsa.has(i-old, AB)) {
        for (auto a: assoc[i])
          if (auto e = dynamic_cast<CollapseExpr*>(a)) {
            DefineStmt* v = e->define_stmt;
            allocate_collapse(v);
            // (i@{CollapseExpr,...}, AB, _) -> ({CollapseExpr,...}, epsilon, CollapseExpr.define_stmt.start)
            sorted_insert(adj[i], make_pair(-1L, stmt2offset[v]+compiled[v].fsa.start));
          }
        long j = adj[i].size();
        while (j && adj[i][j-1].first == AB) {
          long v = adj[i][--j].second;
          for (auto a: assoc[v])
            if (auto e = dynamic_cast<CollapseExpr*>(a)) {
              DefineStmt* w = e->define_stmt;
              allocate_collapse(w);
              // (_, AB, v@{CollapseExpr,...}) -> (CollapseExpr.define_stmt.final, epsilon, v)
              for (long f: compiled[w].fsa.finals) {
                long g = stmt2offset[w]+f;
                sorted_insert(adj[g], make_pair(-1L, v));
                if (g == i)
                  j++;
              }
            }
        }
        // remove (i, AB, _)
        adj[i].resize(j);
      }
  };
  allocate_collapse(stmt);
  anno.fsa.adj = move(adj);
  anno.assoc = move(assoc);
  anno.deterministic = false;

  anno.determinize();
  anno.minimize();
  allo = 0;
  auto relate = [&](long x) {
    if (allo != x)
      anno.assoc[allo] = move(anno.assoc[x]);
    allo++;
  };
  anno.fsa.remove_dead(relate);
  if (anno.fsa.finals.empty())
    anno.assoc.assign(1, {});
  else
    anno.assoc.resize(allo);

  if (opt_dump_automaton)
    print_automaton(anno.fsa);
  if (opt_dump_assoc)
    print_assoc(anno);

  DP(3, "Compiling actions");
  compile_actions(anno);
}
