#include "compiler.hh"
#include "fsa_anno.hh"

#include <map>
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
  void visit(Expr& expr) override { expr.accept(*this); }
  void visit(BracketExpr& expr) override {
    st.push(FsaAnno::bracket(expr));
  }
  void visit(CollapseExpr& expr) override {
    st.push(FsaAnno::collapse(expr));
  }
  void visit(ConcatExpr& expr) override {
    expr.rhs->accept(*this);
    FsaAnno rhs = move(st.top());
    expr.lhs->accept(*this);
    st.top().concat(rhs);
  }
  void visit(DifferenceExpr& expr) override {
    expr.rhs->accept(*this);
    FsaAnno rhs = move(st.top());
    expr.lhs->accept(*this);
    st.top().difference(rhs);
  }
  void visit(DotExpr& expr) override {
    st.push(FsaAnno::dot(expr));
  }
  void visit(EmbedExpr& expr) override {
    st.push(compiled[expr.define_stmt]);
  }
  void visit(IntersectExpr& expr) override {
    expr.rhs->accept(*this);
    FsaAnno rhs = move(st.top());
    expr.lhs->accept(*this);
    st.top().intersect(rhs);
  }
  void visit(LiteralExpr& expr) override {
    st.push(FsaAnno::literal(expr));
  }
  void visit(PlusExpr& expr) override {
    expr.inner->accept(*this);
    st.top().plus();
  }
  void visit(QuestionExpr& expr) override {
    expr.inner->accept(*this);
    st.top().question(expr);
  }
  void visit(StarExpr& expr) override {
    expr.inner->accept(*this);
    st.top().star(expr);
  }
  void visit(UnionExpr& expr) override {
    expr.rhs->accept(*this);
    FsaAnno rhs = move(st.top());
    expr.lhs->accept(*this);
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
