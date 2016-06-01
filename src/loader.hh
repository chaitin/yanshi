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

struct Module {
  LocationFile locfile;
  string filename;
  Stmt* toplevel;
  set<string> defined;
  vector<Module*> unqualified_import;
  map<string, Module*> qualified_import;
  map<string, string> named_action;
};

long load(const char* filename);
Module* load_module(long& n_errors, const char* filename);
void unload_all();
