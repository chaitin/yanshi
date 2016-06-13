#include "option.hh"
#include <stdio.h>

bool opt_check, opt_dump_action, opt_dump_assoc, opt_dump_automaton, opt_dump_module, opt_dump_tree, opt_standalone, opt_substring_grammar;

long AB = 256;
long debug_level = 3;
FILE* debug_file;
const char* opt_output_filename = "-";
const char* opt_mode = "c++";
vector<string> opt_include_paths;
