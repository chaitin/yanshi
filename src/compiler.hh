#pragma once
#include "syntax.hh"

void compile(DefineStmt*);
void generate_cxx(Module* mo);
void generate_graphviz(Module* mo);
