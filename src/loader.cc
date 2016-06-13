#include "common.hh"
#include "compiler.hh"
#include "loader.hh"
#include "option.hh"
#include "parser.hh"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <stdio.h>
#include <stack>
#include <string.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <utility>
using namespace std;

static map<pair<dev_t, ino_t>, Module> inode2module;
static unordered_map<DefineStmt*, vector<DefineStmt*>> depended_by; // key ranges over all DefineStmt
FILE* output;

void print_module_info(Module& mo)
{
  yellow(); printf("filename: %s\n", mo.filename.c_str());
  cyan(); puts("qualified imports:"); sgr0();
  for (auto& x: mo.qualified_import)
    printf("  %s as %s\n", x.second->filename.c_str(), x.first.c_str());
  cyan(); puts("unqualified imports:"); sgr0();
  for (auto& x: mo.unqualified_import)
    printf("  %s\n", x->filename.c_str());
  cyan(); puts("defined actions:"); sgr0();
  for (auto& x: mo.defined_action)
    printf("  %s\n", x.first.c_str());
  cyan(); puts("defined:"); sgr0();
  for (auto& x: mo.defined)
    printf("  %s\n", x.first.c_str());
}

struct ModuleImportDef : PreorderStmtVisitor {
  Module& mo;
  long& n_errors;
  ModuleImportDef(Module& mo, long& n_errors) : mo(mo), n_errors(n_errors) {}

  void visit(ActionStmt& stmt) override {
    if (mo.defined_action.count(stmt.ident)) {
      n_errors++;
      mo.locfile.error(stmt.loc, "Redefined '%s'", stmt.ident.c_str());
    }
    mo.defined_action[stmt.ident] = stmt.code;
  }
  void visit(DefineStmt& stmt) override {
    mo.defined.emplace(stmt.lhs, &stmt);
    stmt.module = &mo;
    depended_by[&stmt]; // empty
  }
  void visit(ImportStmt& stmt) override {
    Module* m = load_module(n_errors, stmt.filename);
    if (! m) {
      n_errors++;
      mo.locfile.error(stmt.loc, "'%s': %s", stmt.filename.c_str(), errno ? strerror(errno) : "parse error");
      return;
    }
    if (stmt.qualified.size())
      mo.qualified_import[stmt.qualified] = m;
    else if (count(ALL(mo.unqualified_import), m) == 0)
      mo.unqualified_import.push_back(m);
  }
};

struct ModuleUse : PrePostActionExprStmtVisitor {
  Module& mo;
  long& n_errors;
  DefineStmt* stmt = NULL;
  ModuleUse(Module& mo, long& n_errors) : mo(mo), n_errors(n_errors) {}

  void pre_expr(Expr& expr) override {
    expr.stmt = stmt;
  }

  void post_expr(Expr& expr) override {
    for (Action* a: expr.entering)
      PrePostActionExprStmtVisitor::visit(*a);
    for (Action* a: expr.finishing)
      PrePostActionExprStmtVisitor::visit(*a);
    for (Action* a: expr.leaving)
      PrePostActionExprStmtVisitor::visit(*a);
    for (Action* a: expr.transiting)
      PrePostActionExprStmtVisitor::visit(*a);
  }

  void visit(RefAction& action) override {
    if (action.qualified.size()) {
      if (! mo.qualified_import.count(action.qualified)) {
        n_errors++;
        mo.locfile.error(action.loc, "unknown module '%s'", action.qualified.c_str());
      } else {
        auto it = mo.qualified_import[action.qualified]->defined_action.find(action.ident);
        if (it == mo.qualified_import[action.qualified]->defined_action.end()) {
          n_errors++;
          mo.locfile.error(action.loc, "'%s::%s' undefined", action.qualified.c_str(), action.ident.c_str());
        } else
          action.define_module = mo.qualified_import[action.qualified];
      }
    } else {
      auto it = mo.defined_action.find(action.ident);
      Module* module = it != mo.defined_action.end() ? &mo : NULL;
      for (auto& import: mo.unqualified_import) {
        auto it2 = import->defined_action.find(action.ident);
        if (it2 != import->defined_action.end()) {
          if (module) {
            n_errors++;
            mo.locfile.error(action.loc, "'%s' redefined in unqualified import '%s'", action.ident.c_str(), import->filename.c_str());
          } else {
            it = it2;
            module = import;
          }
        }
      }
      if (! module) {
        n_errors++;
        mo.locfile.error(action.loc, "'%s' undefined", action.ident.c_str());
      } else
        action.define_module = module;
    }
  }

  void visit(BracketExpr& expr) override {}
  void visit(CollapseExpr& expr) override {
    if (expr.qualified.size()) {
      if (! mo.qualified_import.count(expr.qualified)) {
        n_errors++;
        mo.locfile.error(expr.loc, "unknown module '%s'", expr.qualified.c_str());
      } else {
        auto it = mo.qualified_import[expr.qualified]->defined.find(expr.ident);
        if (it == mo.qualified_import[expr.qualified]->defined.end()) {
          n_errors++;
          mo.locfile.error(expr.loc, "'%s::%s' undefined", expr.qualified.c_str(), expr.ident.c_str());
        } else
          expr.define_stmt = it->second;
      }
    } else {
      auto it = mo.defined.find(expr.ident);
      bool found = it != mo.defined.end();
      for (auto& import: mo.unqualified_import) {
        auto it2 = import->defined.find(expr.ident);
        if (it2 != import->defined.end()) {
          if (found) {
            n_errors++;
            mo.locfile.error(expr.loc, "'%s' redefined in unqualified import '%s'", expr.ident.c_str(), import->filename.c_str());
          } else {
            it = it2;
            found = true;
          }
        }
      }
      if (! found) {
        n_errors++;
        mo.locfile.error(expr.loc, "'%s' undefined", expr.ident.c_str());
      } else
        expr.define_stmt = it->second;
    }
  }
  void visit(DefineStmt& stmt) override {
    this->stmt = &stmt;
    PrePostActionExprStmtVisitor::visit(*stmt.rhs);
    this->stmt = NULL;
  }
  void visit(EmbedExpr& expr) override {
    // almost the same to CollapseExpr except for the dependency
    if (expr.qualified.size()) {
      if (! mo.qualified_import.count(expr.qualified)) {
        n_errors++;
        mo.locfile.error(expr.loc, "unknown module '%s'", expr.qualified.c_str());
      } else {
        auto it = mo.qualified_import[expr.qualified]->defined.find(expr.ident);
        if (it == mo.qualified_import[expr.qualified]->defined.end()) {
          n_errors++;
          mo.locfile.error(expr.loc, "'%s::%s' undefined", expr.qualified.c_str(), expr.ident.c_str());
        } else {
          depended_by[it->second].push_back(stmt);
          expr.define_stmt = it->second;
        }
      }
    } else {
      auto it = mo.defined.find(expr.ident);
      bool found = it != mo.defined.end();
      for (auto& import: mo.unqualified_import) {
        auto it2 = import->defined.find(expr.ident);
        if (it2 != import->defined.end()) {
          if (found) {
            n_errors++;
            mo.locfile.error(expr.loc, "'%s' redefined in unqualified import '%s'", expr.ident.c_str(), import->filename.c_str());
          } else {
            it = it2;
            found = true;
          }
        }
      }
      if (! found) {
        n_errors++;
        mo.locfile.error(expr.loc, "'%s' undefined", expr.ident.c_str());
      } else {
        depended_by[it->second].push_back(stmt);
        expr.define_stmt = it->second;
      }
    }
  }
};

Module* load_module(long& n_errors, const string& filename)
{
  FILE* file = stdin;
  if (filename != "-") {
    file = fopen(filename.c_str(), "r");
    for (string& include: opt_include_paths) {
      if (file) break;
      file = fopen((include+'/'+filename).c_str(), "r");
    }
  }
  if (! file) {
    n_errors++;
    return NULL;
  }

  pair<dev_t, ino_t> inode{0, 0}; // stdin -> {0, 0}
  if (file != stdin) {
    struct stat sb;
    if (fstat(fileno(file), &sb) < 0)
      err_exit(EX_OSFILE, "fstat '%s'", filename.c_str());
    inode = {sb.st_dev, sb.st_ino};
  }
  if (inode2module.count(inode)) {
    fclose(file);
    return &inode2module[inode];
  }
  Module& mo = inode2module[inode];

  string module{file != stdin ? filename : "main"};
  string::size_type t = module.find('.');
  if (t != string::npos)
    module.erase(t, module.size()-t);

  long r;
  char buf[BUF_SIZE];
  string data;
  while ((r = fread(buf, 1, sizeof buf, file)) > 0) {
    data += string(buf, buf+r);
    if (r < sizeof buf) break;
  }
  fclose(file);
  if (data.empty() || data.back() != '\n')
    data.push_back('\n');
  LocationFile locfile(filename, data);

  Stmt* toplevel = NULL;
  mo.locfile = locfile;
  mo.filename = filename;
  long errors = parse(locfile, toplevel);
  if (! toplevel) {
    n_errors += errors;
    mo.status = BAD;
    mo.toplevel = NULL;
    return &mo;
  }
  mo.toplevel = toplevel;
  return &mo;
}

static vector<DefineStmt*> topo_define_stmts(long& n_errors)
{
  vector<DefineStmt*> topo;
  vector<DefineStmt*> st;
  unordered_map<DefineStmt*, i8> vis; // 0: unvisited; 1: in stack; 2: visited; 3: in a cycle
  unordered_map<DefineStmt*, long> cnt;
  function<bool(DefineStmt*)> dfs = [&](DefineStmt* u) {
    if (vis[u] == 2)
      return false;
    if (vis[u] == 3)
      return true;
    if (vis[u] == 1) {
      u->module->locfile.error_context(u->loc, "'%s': circular embedding", u->lhs.c_str());
      long i = st.size();
      while (st[i-1] != u)
        i--;
      st.push_back(st[i-1]);
      for (; i < st.size(); i++) {
        vis[st[i]] = 3;
        fputs("  ", stderr);
        st[i]->module->locfile.error_context(st[i]->loc, "required by %s", st[i]->lhs.c_str());
      }
      fputs("\n", stderr);
      return true;
    }
    cnt[u] = u->export_ ? 1 : 0;
    vis[u] = 1;
    st.push_back(u);
    bool cycle = false;
    for (auto v: depended_by[u])
      if (dfs(v))
        cycle = true;
      else
        cnt[u] += cnt[v];
    st.pop_back();
    vis[u] = 2;
    topo.push_back(u);
    return cycle;
  };
  for (auto& d: depended_by)
    if (! vis[d.first] && dfs(d.first)) // detected cycle
      n_errors++;
  reverse(ALL(topo));
  if (opt_dump_embed) {
    magenta(); printf("=== Embed\n"); sgr0();
    for (auto stmt: topo)
      if (cnt[stmt] > 0)
        printf("count(%s::%s) = %ld\n", stmt->module->filename.c_str(), stmt->lhs.c_str(), cnt[stmt]);
  }
  return topo;
}

long load(const string& filename)
{
  long n_errors = 0;
  Module* mo = load_module(n_errors, filename);
  if (! mo) {
    err_exit(EX_OSFILE, "fopen", filename.c_str());
    return n_errors;
  }
  if (mo->status == BAD)
    return n_errors;

  DP(1, "Processing import & def");
  for(;;) {
    bool done = true;
    for (auto& it: inode2module)
      if (it.second.status == UNPROCESSED) {
        done = false;
        Module& mo = it.second;
        mo.status = GOOD;
        long old = n_errors;
        ModuleImportDef p{mo, n_errors};
        for (Stmt* x = mo.toplevel; x; x = x->next)
          x->accept(p);
        mo.status = old == n_errors ? GOOD : BAD;
      }
    if (done) break;
  }
  if (n_errors)
    return n_errors;

  DP(1, "Processing use");
  for (auto& it: inode2module)
    if (it.second.status == GOOD) {
      Module& mo = it.second;
      ModuleUse p{mo, n_errors};
      for (Stmt* x = mo.toplevel; x; x = x->next)
        x->accept(p);
    }
  if (n_errors)
    return n_errors;

  if (opt_dump_module) {
    magenta(); printf("=== Module\n"); sgr0();
    for (auto& it: inode2module)
      if (it.second.status == GOOD) {
        Module& mo = it.second;
        print_module_info(mo);
      }
    puts("");
  }

  if (opt_dump_tree) {
    magenta(); printf("=== Tree\n"); sgr0();
    StmtPrinter p;
    for (auto& it: inode2module)
      if (it.second.status == GOOD) {
        Module& mo = it.second;
        yellow(); printf("filename: %s\n", mo.filename.c_str()); sgr0();
        for (Stmt* x = mo.toplevel; x; x = x->next)
          x->accept(p);
      }
    puts("");
  }

  DP(1, "Topological sorting");
  vector<DefineStmt*> topo = topo_define_stmts(n_errors);
  if (n_errors)
    return n_errors;

  if (opt_check)
    return 0;

  DP(1, "Compiling DefineStmt");
  for (auto stmt: topo)
    compile(stmt);

  output = strcmp(opt_output_filename, "-") ? fopen(opt_output_filename, "w") : stdout;
  if (! output) {
    n_errors++;
    err_exit(EX_OSFILE, "fopen", opt_output_filename);
    return n_errors;
  }

  if (! strcmp(opt_mode, "c++")) {
    DP(1, "Generating C++");
    generate_cxx(mo);
  } else {
    DP(1, "Generating Graphviz dot");
    generate_graphviz(mo);
  }

  fclose(output);
  return n_errors;
}

void unload_all()
{
  for (auto& it: inode2module) {
    Module& mo = it.second;
    stmt_free(mo.toplevel);
  }
}
