#pragma once
#include "fsa.hh"
#include "syntax.hh"

struct FsaAnno {
  bool deterministic;
  Fsa fsa;
  vector<vector<Expr*> > assoc;
  void concat(FsaAnno& rhs);
  void determinize();
  void difference(FsaAnno& rhs);
  void intersect(FsaAnno& rhs);
  void minimize();
  void plus();
  void question(QuestionExpr& expr);
  void star(StarExpr& expr);
  void union_(FsaAnno& rhs, UnionExpr& expr);
  static FsaAnno bracket(BracketExpr& expr);
  static FsaAnno collapse(CollapseExpr& expr);
  static FsaAnno dot(DotExpr& expr);
  static FsaAnno literal(LiteralExpr& expr);
};
