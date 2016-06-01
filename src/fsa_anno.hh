#pragma once
#include "fsa.hh"
#include "syntax.hh"

struct FsaAnno {
  bool deterministic;
  Fsa fsa;
  vector<vector<Expr*> > assoc;
  void collapse(CollapseExpr& expr);
  void concat(FsaAnno& rhs);
  void determinize();
  void difference(FsaAnno& rhs);
  void embed(EmbedExpr& expr);
  void intersect(FsaAnno& rhs);
  void minimize();
  void plus(PlusExpr* expr);
  void question(QuestionExpr* expr);
  void star(StarExpr* expr);
  void union_(FsaAnno& rhs, UnionExpr* expr);
};
