#include "compiler.hh"
#include "fsa_anno.hh"

#include <algorithm>
#include <map>
#include <limits.h>
#include <unordered_map>
#include <stack>
using namespace std;

static map<DefineStmt*, FsaAnno> compiled;

static void print_fsa(const Fsa& fsa)
{
  printf("start: %ld\n", fsa.start);
  printf("finals:");
  for (long i: fsa.finals)
    printf(" %ld", i);
  puts("");
  puts("edges:");
  REP(i, fsa.n()) {
    printf("%ld:", i);
    for (auto& x: fsa.adj[i])
      printf(" (%ld,%ld)", x.first, x.second);
    puts("");
  }
}

struct Compiler : Visitor<Expr> {
  stack<FsaAnno> st;
  stack<Expr*> path;

  void pre(Expr& expr) {
    expr.depth = path.size();
    if (path.size())
      expr.anc.assign(1, &expr);
  }

  void visit(Expr& expr) override { expr.accept(*this); }
  void visit(BracketExpr& expr) override {
    pre(expr);
    st.push(FsaAnno::bracket(expr));
  }
  void visit(CollapseExpr& expr) override {
    pre(expr);
    st.push(FsaAnno::collapse(expr));
  }
  void visit(ConcatExpr& expr) override {
    pre(expr);
    path.push(&expr);
    expr.rhs->accept(*this);
    FsaAnno rhs = move(st.top());
    expr.lhs->accept(*this);
    path.pop();
    st.top().concat(rhs);
  }
  void visit(DifferenceExpr& expr) override {
    pre(expr);
    path.push(&expr);
    expr.rhs->accept(*this);
    FsaAnno rhs = move(st.top());
    expr.lhs->accept(*this);
    path.pop();
    st.top().difference(rhs);
  }
  void visit(DotExpr& expr) override {
    pre(expr);
    st.push(FsaAnno::dot(expr));
  }
  void visit(EmbedExpr& expr) override {
    pre(expr);
    st.push(compiled[expr.define_stmt]);
  }
  void visit(IntersectExpr& expr) override {
    pre(expr);
    path.push(&expr);
    expr.rhs->accept(*this);
    FsaAnno rhs = move(st.top());
    expr.lhs->accept(*this);
    path.pop();
    st.top().intersect(rhs);
  }
  void visit(LiteralExpr& expr) override {
    pre(expr);
    st.push(FsaAnno::literal(expr));
  }
  void visit(PlusExpr& expr) override {
    pre(expr);
    path.push(&expr);
    expr.inner->accept(*this);
    path.pop();
    st.top().plus();
  }
  void visit(QuestionExpr& expr) override {
    pre(expr);
    path.push(&expr);
    expr.inner->accept(*this);
    path.pop();
    st.top().question(expr);
  }
  void visit(StarExpr& expr) override {
    pre(expr);
    path.push(&expr);
    expr.inner->accept(*this);
    path.pop();
    st.top().star(expr);
  }
  void visit(UnionExpr& expr) override {
    pre(expr);
    path.push(&expr);
    expr.rhs->accept(*this);
    FsaAnno rhs = move(st.top());
    expr.lhs->accept(*this);
    path.pop();
    st.top().union_(rhs, expr);
  }
};

void compile(DefineStmt* stmt)
{
  if (compiled.count(stmt))
    return;
  FsaAnno& anno = compiled[stmt];
  Compiler comp;
  stmt->rhs->accept(comp);
  anno = move(comp.st.top());
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
    if (allo != x) {
      anno.fsa.adj[allo] = move(anno.fsa.adj[x]);
      anno.assoc[allo] = move(anno.assoc[x]);
    }
    allo++;
  };
  anno.fsa.remove_dead(relate);
  print_fsa(anno.fsa);
}
