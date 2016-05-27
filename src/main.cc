#include "common.hh"
#include "fsa.hh"
#include "loader.hh"
#include "option.hh"

#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sysexits.h>
#include <string>
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
    {"module-info",         required_argument, 0,   'm'},
    {"output",              required_argument, 0,   'O'},
    {0,                     0,                 0,   0},
  };

  char* opt_output_filename = NULL;

  while ((opt = getopt_long(argc, argv, "Dhmo:t", long_options, NULL)) != -1) {
    switch (opt) {
    case 'D':
      break;
    case 'h':
      print_help(stdout);
      break;
    case 'm':
      opt_module_info = true;
      break;
    case 'o':
      opt_output_filename = optarg;
      break;
    case 't':
      opt_dump_tree = true;
      break;
    case '?':
      print_help(stderr);
      break;
    }
  }
  argc -= optind;
  argv += optind;

  load(argc ? argv[0] : NULL);
  unload_all();
}
