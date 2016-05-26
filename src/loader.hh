#pragma once
#include "syntax.hh"
#include <map>
#include <set>
#include <string>

struct Module {
  std::string filename;
  Stmt* toplevel;
  std::set<std::string> defined;
  std::vector<Module*> unqualified_import;
  std::map<std::string, Module*> qualified_import;
  std::map<std::string, std::string> named_action;
};

void load(const char* filename);
Module* load_module(const char* filename);
void unload_all();
