#pragma once
#include "common.hh"

////
//
char* q_string_literal();
char* qq_string_literal();

//// Expr

struct Expr {
  virtual ~Expr() {}
};

//// Stmt

struct Stmt {
  virtual ~Stmt() {}
  Stmt *prev, *next;
};

struct EmptyStmt : Stmt {};

struct AssignStmt : Stmt {
  string lhs;
  Expr* rhs;
};

struct InstantiationStmt : Stmt {
  string lhs;
  Expr* rhs;
};

void stmt_free(Stmt* stmt);
