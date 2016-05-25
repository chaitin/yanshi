#pragma once
#include "common.hh"
#include <bitset>
#include <string>

//// Visitor

template<class T>
struct Visitor;

template<class T>
struct VisitableBase {
  virtual void accept(Visitor<T>& visitor) = 0;
};

template<class Base, class Derived>
struct Visitable : Base {
  void accept(Visitor<Base>& visitor) override {
    visitor.visit(static_cast<Derived&>(*this));
  }
};

struct Expr;
struct BracketExpr;
struct ClosureExpr;
struct CollapseExpr;
struct ConcatExpr;
struct DifferenceExpr;
struct EmbedExpr;
struct PlusExpr;
struct UnionExpr;
struct ExprVisitor {
  virtual void visit(Expr&) = 0;
  virtual void visit(BracketExpr&) = 0;
  virtual void visit(ClosureExpr&) = 0;
  virtual void visit(CollapseExpr&) = 0;
  virtual void visit(ConcatExpr&) = 0;
  virtual void visit(DifferenceExpr&) = 0;
  virtual void visit(EmbedExpr&) = 0;
  virtual void visit(PlusExpr&) = 0;
  virtual void visit(UnionExpr&) = 0;
};
template<> struct Visitor<Expr> : ExprVisitor {};

struct Stmt;
struct AssignStmt;
struct EmptyStmt;
struct InstantiationStmt;
struct StmtVisitor {
  virtual void visit(Stmt&) = 0;
  virtual void visit(AssignStmt&) = 0;
  virtual void visit(EmptyStmt&) = 0;
  virtual void visit(InstantiationStmt&) = 0;
};
template<> struct Visitor<Stmt> : StmtVisitor {};

//// Expr

struct Expr : VisitableBase<Expr> {
  virtual ~Expr() = default;
};

struct BracketExpr : Visitable<Expr, BracketExpr> {
  std::bitset<256> charset;
  BracketExpr(std::bitset<256>* charset) : charset(*charset) {}
};

struct CollapseExpr : Visitable<Expr, CollapseExpr> {
  char* ident;
  CollapseExpr(char* ident) : ident(ident) {}
  ~CollapseExpr() {
    free(ident);
  }
};

struct ClosureExpr : Visitable<Expr, ClosureExpr> {
  Expr* inner;
  ClosureExpr(Expr* inner) : inner(inner) {}
  ~ClosureExpr() {
    delete inner;
  }
};

struct ConcatExpr : Visitable<Expr, ConcatExpr> {
  Expr *lhs, *rhs;
  ConcatExpr(Expr* lhs, Expr* rhs) : lhs(lhs), rhs(rhs) {}
  ~ConcatExpr() {
    delete lhs;
    delete rhs;
  }
};

struct DifferenceExpr : Visitable<Expr, DifferenceExpr> {
  Expr *lhs, *rhs;
  DifferenceExpr(Expr* lhs, Expr* rhs) : lhs(lhs), rhs(rhs) {}
  ~DifferenceExpr() {
    delete lhs;
    delete rhs;
  }
};

struct EmbedExpr : Visitable<Expr, EmbedExpr> {
  char* ident;
  EmbedExpr(char* ident) : ident(ident) {}
  ~EmbedExpr() {
    free(ident);
  }
};

struct PlusExpr : Visitable<Expr, PlusExpr> {
  Expr* inner;
  PlusExpr(Expr* inner) : inner(inner) {}
  ~PlusExpr() {
    delete inner;
  }
};

struct UnionExpr : Visitable<Expr, UnionExpr> {
  Expr *lhs, *rhs;
  UnionExpr(Expr* lhs, Expr* rhs) : lhs(lhs), rhs(rhs) {}
  ~UnionExpr() {
    delete lhs;
    delete rhs;
  }
};

//// Stmt

struct Stmt {
  Stmt *prev = NULL, *next = NULL;
  virtual ~Stmt() = default;
  virtual void accept(Visitor<Stmt>& visitor) = 0;
};

struct EmptyStmt : Visitable<Stmt, EmptyStmt> {};

struct AssignStmt : Visitable<Stmt, AssignStmt> {
  char* lhs;
  Expr* rhs;
  AssignStmt(char* lhs, Expr* rhs) : lhs(lhs), rhs(rhs) {}
  ~AssignStmt() {
    free(lhs);
    delete rhs;
  }
};

struct InstantiationStmt : Visitable<Stmt, InstantiationStmt> {
  char* lhs;
  Expr* rhs;
  InstantiationStmt(char* lhs, Expr* rhs) : lhs(lhs), rhs(rhs) {}
  ~InstantiationStmt() {
    free(lhs);
    delete rhs;
  }
};

void stmt_free(Stmt* stmt);

//// Visitor implementations

struct ExprPrinter : Visitor<Expr> {
  void visit(Expr& expr) {
  }
};

struct StmtPrinter : Visitor<Expr>, Visitor<Stmt> {
  int depth = 0;

  void visit(Stmt& stmt) override {
    stmt.accept(*this);
  }
  void visit(AssignStmt& stmt) override {
    printf("%*s%s\n", 2*depth, "", "AssignStmt");
    depth++;
    printf("%*s%s\n", 2*depth, "", stmt.lhs);
    visit(*stmt.rhs);
    depth--;
  }
  void visit(EmptyStmt& stmt) override {
    printf("%*s%s\n", 2*depth, "", "EmptyStmt");
  }
  void visit(InstantiationStmt& stmt) override {
    printf("%*s%s\n", 2*depth, "", "InstantiationStmt");
  }

  void visit(Expr& expr) override {
    expr.accept(*this);
  }
  void visit(BracketExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "BracketExpr");
    printf("%*s", 2*(depth+1), "");
    for (long i = 0, j; i < expr.charset.size(); )
      if (! expr.charset[i])
        i++;
      else {
        for (j = i; j < expr.charset.size() && expr.charset[j]; j++);
        printf(" %ld-%ld", i, j-1);
        i = j;
      }
    puts("");
  }
  void visit(ClosureExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "ClosureExpr");
    depth++;
    expr.inner->accept(*this);
    depth--;
  }
  void visit(CollapseExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "CollapseExpr");
    printf("%*s%s\n", 2*(depth+1), "", expr.ident);
  }
  void visit(ConcatExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "ConcatExpr");
    depth++;
    expr.lhs->accept(*this);
    expr.rhs->accept(*this);
    depth--;
  }
  void visit(DifferenceExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "DifferenceExpr");
    depth++;
    expr.lhs->accept(*this);
    expr.rhs->accept(*this);
    depth--;
  }
  void visit(EmbedExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "EmbedExpr");
    printf("%*s%s\n", 2*(depth+1), "", expr.ident);
  }
  void visit(PlusExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "UnionExpr");
    depth++;
    expr.inner->accept(*this);
    depth--;
  }
  void visit(UnionExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "UnionExpr");
    depth++;
    expr.lhs->accept(*this);
    expr.rhs->accept(*this);
    depth--;
  }
};
