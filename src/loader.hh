#pragma once
#include "syntax.hh"

#include <map>
#include <set>
#include <string>
#include <vector>
using std::map;
using std::set;
using std::string;
using std::vector;

enum ModuleStatus { UNPROCESSED = 0, BAD, GOOD };

struct Module {
  ModuleStatus status;
  LocationFile locfile;
  string filename;
  Stmt* toplevel;
  map<string, DefineStmt*> defined;
  vector<Module*> unqualified_import;
  map<string, Module*> qualified_import;
  map<string, string> named_action;
};

long load(const string& filename);
Module* load_module(long& n_errors, const string& filename);
void unload_all();
