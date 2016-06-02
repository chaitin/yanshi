#pragma once
#include "syntax.hh"

void compile(DefineStmt*);
void generate_header(Module* mo);
void generate_body(Module* mo);
