#pragma once
#include "syntax.hh"
#include <map>
#include <set>
#include <string>

struct Module {
  LocationFile locfile;
  std::string filename;
  Stmt* toplevel;
  std::set<std::string> defined;
  std::vector<Module*> unqualified_import;
  std::map<std::string, Module*> qualified_import;
  std::map<std::string, std::string> named_action;
};

long load(const char* filename);
Module* load_module(long& n_errors, const char* filename);
void unload_all();
