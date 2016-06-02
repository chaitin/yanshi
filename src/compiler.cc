#include "compiler.hh"
#include "fsa_anno.hh"

#include <map>
#include <stack>
using namespace std;

static map<DefineStmt*, FsaAnno> compiled;

struct LCA {
  long depth;
  vector<Expr*> anc;
};

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
    expr.lca->depth = path.size();
    if (path.size())
      expr.lca->anc.assign(1, &expr);
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
  anno.determinize();
  anno.minimize();
  print_fsa(anno.fsa);
}
