#include "compiler.hh"
#include "fsa_anno.hh"
#include "loader.hh"
#include "syntax.hh"

#include <type_traits>
#include <algorithm>
#include <functional>
#include <sstream>
#include <unicode/utf8.h>
#include <unordered_map>
#include <wctype.h>
#ifdef HAVE_READLINE
# include <readline/readline.h>
# include <readline/history.h>
#endif
using namespace std;

enum class ReplMode {string, integer};
static ReplMode mode = ReplMode::string;
static const FsaAnno* anno;

struct Command
{
  const char* name;
  function<void()> fn;
} commands[] = {
  {".automaton", []() {print_automaton(anno->fsa); }},
  {".assoc", []() {print_assoc(*anno); }},
  {".help",
    []() {
      fputs("Commands available from the prompt:\n"
             "  :automaton    dump automaton\n"
             "  :assoc        dump associated AST Expr for each state\n"
             "  :help         display this help\n"
             "  :integer      input is a list of non-negative integers\n"
             "  :string       input is a string (default)\n"
             , stdout);
    }},
  {".integer", []() {mode = ReplMode::integer; puts("Input a list of non-negative integers"); }},
  {".string", []() {mode = ReplMode::string; puts("Input a string"); }},
};

#ifdef HAVE_READLINE
static char* command_completer(const char* text, int state)
{
  static long i = 0;
  if (! state)
    i = 0;
  while (i < LEN(commands)) {
    Command* x = &commands[i++];
    if (! strncmp(x->name, text, strlen(text)))
      return strdup(x->name);
  }
  return NULL;
}

static char** on_complete(const char* text, int start, int end)
{
  rl_attempted_completion_over = 1;
  if (! start)
    return rl_completion_matches(text, command_completer);
  return NULL;
}
#else
char* readline(const char* prompt)
{
  char* r = NULL;
  size_t s = 0;
  ssize_t n;
  fputs(prompt, stdout);
  if ((n = getline(&r, &s, stdin)) > 0)
    r[n-1] = '\0';
  else {
    free(r);
    r = NULL;
  }
  return r;
}
#endif

void repl(DefineStmt* stmt)
{
#ifdef HAVE_READLINE
  rl_attempted_completion_function = on_complete;
#endif
  anno = &compiled[stmt];
  char* line;
  stringstream ss;
  while ((line = readline("λ ")) != NULL) {
#ifdef HAVE_READLINE
    if (line[0])
      add_history(line);
#endif
    if (line[0] == '.') {
      size_t len = strlen(line);
      while (len && isspace(line[len-1]))
        len--;
      Command* com = NULL;
      REP(i, LEN(commands))
        if (! strncmp(commands[i].name, line, len)) {
          if (com) com = (Command*)1;
          else com = &commands[i];
        }
      if (! com)
        printf("Unknown command '%s'\n", line);
      else if (com == (Command*)1)
        printf("Ambiguous command '%s'\n", line);
      else
        com->fn();
      free(line);
      continue;
    }

    long u = anno->fsa.start;
    int32_t i = 0, len, c;
    if (anno->fsa.is_final(u)) yellow(1);
    else normal_yellow(1);
    printf("%ld ", u); sgr0();

    if (mode == ReplMode::string) {
      len = strlen(line);
      while (i < len) {
        U8_NEXT_OR_FFFD(line, i, len, c);
        if (iswcntrl(c)) printf("%d ", c);
        else printf("%lc ", c);
        u = anno->fsa.transit(u, c);
        if (anno->fsa.is_final(u)) yellow();
        else normal_yellow();
        printf("%ld ", u); sgr0();
        if (u < 0) break;
      }
    } else {
      ss.clear();
      ss.str(line);
      while (ss >> c) {
        printf("%d ", c);
        u = anno->fsa.transit(u, c);
        if (anno->fsa.is_final(u)) yellow();
        else normal_yellow();
        printf("%ld ", u); sgr0();
        if (u < 0) break;
      }
    }
    free(line);
    puts("");
    if (u >= 0) {
      unordered_map<DefineStmt*, vector<long>> start_finals;
      unordered_map<DefineStmt*, vector<pair<long, long>>> inners;
      for (auto aa: anno->assoc[u]) {
        if (has_start(aa.second))
          start_finals[aa.first->stmt].push_back(aa.first->loc.start);
        if (has_inner(aa.second)) {
          inners[aa.first->stmt].emplace_back(aa.first->loc.start, 1);
          inners[aa.first->stmt].emplace_back(aa.first->loc.end, -1);
        }
        if (has_final(aa.second))
          start_finals[aa.first->stmt].push_back(aa.first->loc.end);
      }
      vector<DefineStmt*> stmts;
      for (auto& it: start_finals)
        stmts.push_back(it.first);
      for (auto& it: inners)
        stmts.push_back(it.first);
      // sort DefineStmt by location
      sort(ALL(stmts), [](const DefineStmt* x, const DefineStmt* y) {
        if (x->module != y->module)
          return x->module < y->module;
        if (x->loc.start != y->loc.start)
          return x->loc.start < y->loc.start;
        return x->loc.end < y->loc.end;
      });
      stmts.erase(unique(ALL(stmts)), stmts.end());
      for (auto* stmt: stmts) {
        auto& start_final = start_finals[stmt];
        auto& inner = inners[stmt];
        sort(ALL(start_final));
        sort(ALL(inner));
        auto it0 = start_final.begin();
        auto it1 = inner.begin();
        long nest = 0;
        FOR(i, stmt->loc.start, stmt->loc.end) {
          for (; it0 != start_final.end() && *it0 < i; ++it0);
          if (it0 != start_final.end() && *it0 == i) {
            cyan();
            putchar(':');
            sgr0();
          }
          for (; it1 != inner.end() && it1->first <= i; ++it1)
            nest += it1->second;
          if (nest)
            yellow();
          putchar(stmt->module->locfile.data[i]);
          if (nest)
            sgr0();
        }
      }
    }
  }
}
