#pragma once
#include "common.hh"
#include <bitset>
#include <string>

////
//
char* q_string_literal();
char* qq_string_literal();

//// Expr

struct Expr {
  virtual ~Expr() {}
};

struct BracketExpr : Expr {
  std::bitset<256> bitset;
  BracketExpr(std::bitset<256>* bitset) : bitset(*bitset) {}
};

struct EmbedExpr : Expr {
  std::string ident;
  EmbedExpr(const std::string& ident) : ident(ident) {}
};

struct CollapseExpr : Expr {
  std::string ident;
  CollapseExpr(const std::string& ident) : ident(ident) {}
};

//// Stmt

struct Stmt {
  virtual ~Stmt() {}
  Stmt *prev, *next;
};

struct EmptyStmt : Stmt {};

struct AssignStmt : Stmt {
  std::string lhs;
  Expr* rhs;
  AssignStmt(const std::string& lhs, Expr* rhs) : lhs(lhs), rhs(rhs) {}
};

struct InstantiationStmt : Stmt {
  std::string lhs;
  Expr* rhs;
};

void stmt_free(Stmt* stmt);
