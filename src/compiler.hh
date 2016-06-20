#pragma once
#include "fsa_anno.hh"
#include "syntax.hh"

void compile(DefineStmt*);
void generate_cxx(Module* mo);
void generate_graphviz(Module* mo);
extern map<DefineStmt*, FsaAnno> compiled;
