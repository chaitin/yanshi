#pragma once
#include <string>
#include <vector>
using std::string;
using std::vector;

extern bool opt_check, opt_dump_action, opt_dump_assoc, opt_dump_automaton, opt_dump_module, opt_dump_tree, opt_substring_grammar;
extern const char* opt_output_filename;
extern const char* opt_mode;
extern vector<string> opt_include_paths;
