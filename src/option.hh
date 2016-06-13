#pragma once
#include <string>
#include <vector>
using std::string;
using std::vector;

extern bool opt_bytes, opt_check, opt_dump_action, opt_dump_assoc, opt_dump_automaton, opt_dump_embed, opt_dump_module, opt_dump_tree, opt_standalone, opt_substring_grammar;
extern long AB;
extern const char* opt_output_filename;
extern const char* opt_mode;
extern vector<string> opt_include_paths;
