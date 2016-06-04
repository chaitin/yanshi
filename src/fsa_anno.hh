#pragma once
#include "fsa.hh"
#include "syntax.hh"

enum class ExprTag {
  start = 1,
  inner = 2,
  final = 4,
};

bool operator<(ExprTag x, ExprTag y);
bool assoc_has_expr(vector<pair<Expr*, ExprTag>>& as, const Expr* x);

struct FsaAnno {
  bool deterministic;
  Fsa fsa;
  vector<vector<pair<Expr*, ExprTag>>> assoc;
  void add_assoc(Expr& expr);
  void complement(ComplementExpr* expr);
  void concat(FsaAnno& rhs, ConcatExpr* expr);
  void determinize();
  void difference(FsaAnno& rhs, DifferenceExpr* expr);
  void intersect(FsaAnno& rhs, IntersectExpr* expr);
  void minimize();
  void plus(PlusExpr* expr);
  void question(QuestionExpr* expr);
  void repeat(RepeatExpr& expr);
  void star(StarExpr* expr);
  void substring_grammar();
  void union_(FsaAnno& rhs, UnionExpr* expr);
  static FsaAnno bracket(BracketExpr& expr);
  static FsaAnno collapse(CollapseExpr& expr);
  static FsaAnno dot(DotExpr* expr);
  static FsaAnno epsilon(EpsilonExpr* expr);
  static FsaAnno literal(LiteralExpr& expr);
};
