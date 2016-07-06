#pragma once
#include "syntax.hh"

#include <set>
#include <string>
#include <unordered_map>
#include <vector>
using std::set;
using std::string;
using std::unordered_map;
using std::vector;

enum ModuleStatus { UNPROCESSED = 0, BAD, GOOD };

struct Module {
  ModuleStatus status;
  LocationFile locfile;
  string filename;
  Stmt* toplevel;
  unordered_map<string, DefineStmt*> defined;
  vector<Module*> unqualified_import;
  unordered_map<string, Module*> qualified_import;
  unordered_map<string, ActionStmt*> defined_action;
  unordered_map<string, PreprocessDefineStmt*> macro;
};

Stmt* resolve(Module& mo, const string qualified, const string& ident);
long load(const string& filename);
Module* load_module(long& n_errors, const string& filename);
void unload_all();
extern Module* main_module;
extern FILE *output, *output_header;
