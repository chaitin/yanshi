#pragma once
#include "common.hh"
#include "location.hh"
#include <bitset>
#include <string>
#include <vector>
using std::bitset;
using std::vector;

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

struct Action;
struct InlineAction;
struct RefAction;
template<>
struct Visitor<Action> {
  virtual void visit(Action& action) = 0;
  virtual void visit(InlineAction&) = 0;
  virtual void visit(RefAction&) = 0;
};

struct Expr;
struct BracketExpr;
struct CollapseExpr;
struct ConcatExpr;
struct DifferenceExpr;
struct DotExpr;
struct EmbedExpr;
struct LiteralExpr;
struct PlusExpr;
struct QuestionExpr;
struct StarExpr;
struct UnionExpr;
template<>
struct Visitor<Expr> {
  virtual void visit(Expr&) = 0;
  virtual void visit(BracketExpr&) = 0;
  virtual void visit(CollapseExpr&) = 0;
  virtual void visit(ConcatExpr&) = 0;
  virtual void visit(DifferenceExpr&) = 0;
  virtual void visit(DotExpr&) = 0;
  virtual void visit(EmbedExpr&) = 0;
  virtual void visit(LiteralExpr&) = 0;
  virtual void visit(PlusExpr&) = 0;
  virtual void visit(QuestionExpr&) = 0;
  virtual void visit(StarExpr&) = 0;
  virtual void visit(UnionExpr&) = 0;
};

struct Stmt;
struct ActionStmt;
struct DefineStmt;
struct EmptyStmt;
struct ImportStmt;
template<>
struct Visitor<Stmt> {
  virtual void visit(Stmt&) = 0;
  virtual void visit(ActionStmt&) = 0;
  virtual void visit(DefineStmt&) = 0;
  virtual void visit(EmptyStmt&) = 0;
  virtual void visit(ImportStmt&) = 0;
};

//// Action

struct Action : VisitableBase<Action> {

  Location loc;
  virtual ~Action() = default;
};

struct InlineAction : Visitable<Action, InlineAction> {
  char* code;
  InlineAction(char* code) : code(code) {}
  ~InlineAction() {
    free(code);
  }
};

struct RefAction : Visitable<Action, RefAction> {
  char* ident;
  RefAction(char* ident) : ident(ident) {}
  ~RefAction() {
    free(ident);
  }
};

//// Expr

struct Expr : VisitableBase<Expr> {
  Location loc;
  vector<Action*> entering, finishing, leaving, transiting;
  virtual ~Expr() = default;
};

struct BracketExpr : Visitable<Expr, BracketExpr> {
  bitset<256> charset;
  BracketExpr(bitset<256>* charset) : charset(*charset) { delete charset; }
};

struct CollapseExpr : Visitable<Expr, CollapseExpr> {
  char *qualified, *ident;
  CollapseExpr(char* qualified, char* ident) : qualified(qualified), ident(ident) {}
  ~CollapseExpr() {
    free(qualified); // may be NULL
    free(ident);
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

struct DotExpr : Visitable<Expr, DotExpr> {};

struct EmbedExpr : Visitable<Expr, EmbedExpr> {
  char *qualified, *ident;
  EmbedExpr(char* qualified, char* ident) : qualified(qualified), ident(ident) {}
  ~EmbedExpr() {
    free(qualified); // may be NULL
    free(ident);
  }
};

struct LiteralExpr : Visitable<Expr, LiteralExpr> {
  char* literal;
  LiteralExpr(char* literal) : literal(literal) {}
  ~LiteralExpr() {
    free(literal);
  }
};

struct PlusExpr : Visitable<Expr, PlusExpr> {
  Expr* inner;
  PlusExpr(Expr* inner) : inner(inner) {}
  ~PlusExpr() {
    delete inner;
  }
};

struct QuestionExpr : Visitable<Expr, QuestionExpr> {
  Expr* inner;
  QuestionExpr(Expr* inner) : inner(inner) {}
  ~QuestionExpr() {
    delete inner;
  }
};

struct StarExpr : Visitable<Expr, StarExpr> {
  Expr* inner;
  StarExpr(Expr* inner) : inner(inner) {}
  ~StarExpr() {
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
  Location loc;
  Stmt *prev = NULL, *next = NULL;
  virtual ~Stmt() = default;
  virtual void accept(Visitor<Stmt>& visitor) = 0;
};

struct EmptyStmt : Visitable<Stmt, EmptyStmt> {};

struct ActionStmt : Visitable<Stmt, ActionStmt> {
  char *ident, *code;
  ActionStmt(char* ident, char* code) : ident(ident), code(code) {}
  ~ActionStmt() {
    free(ident);
    free(code);
  }
};

struct DefineStmt : Visitable<Stmt, DefineStmt> {
  bool export_;
  char* lhs;
  Expr* rhs;
  DefineStmt(bool export_, char* lhs, Expr* rhs) : export_(export_), lhs(lhs), rhs(rhs) {}
  ~DefineStmt() {
    free(lhs);
    delete rhs;
  }
};

struct ImportStmt : Visitable<Stmt, ImportStmt> {
  char *filename, *qualified;
  ImportStmt(char* filename, char* qualified) : filename(filename), qualified(qualified) {}
  ~ImportStmt() {
    free(filename);
    free(qualified); // may be NULL
  }
};

void stmt_free(Stmt* stmt);

//// Visitor implementations

struct ExprPrinter : Visitor<Expr> {
  void visit(Expr& expr) {
  }
};

struct StmtPrinter : Visitor<Action>, Visitor<Expr>, Visitor<Stmt> {
  int depth = 0;

  void visit(Action& action) override {
    action.accept(*this);
  }
  void visit(InlineAction& action) override {
    printf("%*s%s\n", 2*depth, "", "InlineAction");
    printf("%*s%s\n", 2*(depth+1), "", action.code);
  }
  void visit(RefAction& action) override {
    printf("%*s%s\n", 2*depth, "", "RefAction");
    printf("%*s%s\n", 2*(depth+1), "", action.ident);
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
  void visit(CollapseExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "CollapseExpr");
    printf("%*s", 2*(depth+1), "");
    if (expr.qualified)
      printf("%s.%s\n", expr.qualified, expr.ident);
    else
      printf("%s\n", expr.ident);
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
  void visit(DotExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "DotExpr");
  }
  void visit(EmbedExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "EmbedExpr");
    printf("%*s", 2*(depth+1), "");
    if (expr.qualified)
      printf("%s.%s\n", expr.qualified, expr.ident);
    else
      printf("%s\n", expr.ident);
  }
  void visit(LiteralExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "LiteralExpr");
    printf("%*s%s\n", 2*(depth+1), "", expr.literal);
  }
  void visit(PlusExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "UnionExpr");
    depth++;
    expr.inner->accept(*this);
    depth--;
  }
  void visit(QuestionExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "QuestionExpr");
    depth++;
    expr.inner->accept(*this);
    depth--;
  }
  void visit(StarExpr& expr) override {
    printf("%*s%s\n", 2*depth, "", "StarExpr");
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

  void visit(Stmt& stmt) override {
    stmt.accept(*this);
  }
  void visit(ActionStmt& stmt) override {
    printf("%*s%s\n", 2*depth, "", "ActionStmt");
    printf("%*s%s\n", 2*(depth+1), "", stmt.ident);
    printf("%*s%s\n", 2*(depth+1), "", stmt.code);
  }
  void visit(DefineStmt& stmt) override {
    printf("%*s%s%s\n", 2*depth, "", "DefineStmt", stmt.export_ ? " export" : "");
    depth++;
    printf("%*s%s\n", 2*depth, "", stmt.lhs);
    visit(*stmt.rhs);
    depth--;
  }
  void visit(EmptyStmt& stmt) override {
    printf("%*s%s\n", 2*depth, "", "EmptyStmt");
  }
  void visit(ImportStmt& stmt) override {
    printf("%*s%s\n", 2*depth, "", "ImportStmt");
    printf("%*s%s\n", 2*(depth+1), "", stmt.filename);
    if (stmt.qualified)
      printf("%*sas %s\n", 2*(depth+1), "", stmt.qualified);
  }
};

//// Visitor implementation

struct PreorderStmtVisitor : Visitor<Stmt> {
  void visit(Stmt& stmt) override { stmt.accept(*this); }
  void visit(ActionStmt& stmt) override {}
  void visit(DefineStmt& stmt) override {}
  void visit(EmptyStmt& stmt) override {}
  void visit(ImportStmt& stmt) override {}
};

struct PreorderActionExprStmtVisitor : Visitor<Action>, Visitor<Expr>, Visitor<Stmt> {
  void visit(Action& expr) override { expr.accept(*this); }
  void visit(InlineAction&) override {}
  void visit(RefAction&) override {}

  void visit(Expr& expr) override { expr.accept(*this); }
  void visit(BracketExpr& expr) override {}
  void visit(CollapseExpr& expr) override {}
  void visit(ConcatExpr& expr) override {}
  void visit(DifferenceExpr& expr) override {}
  void visit(DotExpr& expr) override {}
  void visit(EmbedExpr& expr) override {}
  void visit(LiteralExpr& expr) override {}
  void visit(PlusExpr& expr) override { expr.inner->accept(*this); }
  void visit(QuestionExpr& expr) override { expr.inner->accept(*this); }
  void visit(StarExpr& expr) override { expr.inner->accept(*this); }
  void visit(UnionExpr& expr) override {
    expr.lhs->accept(*this);
    expr.rhs->accept(*this);
  }

  void visit(Stmt& stmt) override { stmt.accept(*this); }
  void visit(ActionStmt& stmt) override {}
  void visit(DefineStmt& stmt) override { stmt.rhs->accept(*this); }
  void visit(EmptyStmt& stmt) override {}
  void visit(ImportStmt& stmt) override {}
};
