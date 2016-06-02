#include "common.hh"
#include "fsa.hh"
#include "loader.hh"
#include "option.hh"

#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sysexits.h>
#include <unistd.h>
using namespace std;

void print_help(FILE *fh)
{
  fprintf(fh, "Usage: %s [OPTIONS] dir\n", program_invocation_short_name);
  fputs(
        "\n"
        "Options:\n"
        "  -h, --help                display this help and exit\n"
        "\n"
        "Examples:\n"
        , fh);
  exit(fh == stdout ? 0 : EX_USAGE);
}

int main(int argc, char *argv[])
{
  int opt;
  static struct option long_options[] = {
    {"help",                no_argument,       0,   'h'},
    {"debug",               required_argument, 0,   'd'},
    {"debug-output",        required_argument, 0,   'l'},
    {"dump-assoc",          no_argument,       0,   1000},
    {"dump-automaton",      no_argument,       0,   1001},
    {"dump-module",         no_argument,       0,   1002},
    {"dump-tree",           no_argument,       0,   1003},
    {"module-info",         required_argument, 0,   'm'},
    {"output",              required_argument, 0,   'O'},
    {0,                     0,                 0,   0},
  };

  char* opt_output_filename = NULL;
#ifdef DEBUG
  opt_dump_assoc = opt_dump_automaton = true;
#endif

  while ((opt = getopt_long(argc, argv, "Dd:hl:o:t", long_options, NULL)) != -1) {
    switch (opt) {
    case 'D':
      break;
    case 'd':
      debug_level = get_long(optarg);
      break;
    case 'l':
      if (debug_file)
        err_exit(EX_USAGE, "multiple '-l'");
      debug_file = fopen(optarg, "w");
      if (! debug_file)
        err_exit(EX_OSFILE, "fopen");
      break;
    case 'h':
      print_help(stdout);
      break;
    case 'o':
      opt_output_filename = optarg;
      break;
    case 1000: opt_dump_assoc = true; break;
    case 1001: opt_dump_automaton = true; break;
    case 1002: opt_dump_module = true; break;
    case 1003: opt_dump_tree = true; break;
    case '?':
      print_help(stderr);
      break;
    }
  }
  if (! debug_file)
    debug_file = stderr;
  argc -= optind;
  argv += optind;

  long n_errors = load(argc ? argv[0] : "-");
  unload_all();
  fclose(debug_file);
  return n_errors ? 2 : 0;
}
