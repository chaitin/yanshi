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

void print_module_info(Module& mo)
{
  printf("filename: %s\n", mo.filename.c_str());
  puts("qualified imports:");
  for (auto& x: mo.qualified_import)
    printf("  %s as %s\n", x.second->filename.c_str(), x.first.c_str());
  puts("unqualified imports:");
  for (auto& x: mo.unqualified_import)
    printf("  %s\n", x->filename.c_str());
  puts("defined:");
  for (auto& x: mo.defined)
    printf("  %s\n", x.first.c_str());
}

struct ModuleImportDef : PreorderStmtVisitor {
  Module& mo;
  long& n_errors;
  ModuleImportDef(Module& mo, long& n_errors) : mo(mo), n_errors(n_errors) {}

  void visit(ActionStmt& stmt) override {
    if (mo.named_action.count(stmt.ident)) {
      n_errors++;
      mo.locfile.error(stmt.loc, "Redefined '%s'", stmt.ident.c_str());
    }
    mo.named_action[stmt.ident] = stmt.code;
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

struct ModuleUse : PreorderActionExprStmtVisitor {
  Module& mo;
  long& n_errors;
  DefineStmt* define_stmt = NULL;
  ModuleUse(Module& mo, long& n_errors) : mo(mo), n_errors(n_errors) {}

  void visit(DefineStmt& stmt) override {
    define_stmt = &stmt;
    stmt.rhs->accept(*this);
    define_stmt = NULL;
  }

  void visit(BracketExpr& expr) override {}
  void visit(CollapseExpr& expr) override {
    if (expr.qualified.size()) {
      if (! mo.qualified_import.count(expr.qualified)) {
        n_errors++;
        mo.locfile.error(expr.loc, "Unknown module '%s'", expr.qualified.c_str());
      } else if (! mo.qualified_import[expr.qualified]->defined.count(expr.ident)) {
        n_errors++;
        mo.locfile.error(expr.loc, "'%s::%s' undefined", expr.qualified.c_str(), expr.ident.c_str());
      }
    } else {
      long c = mo.defined.count(expr.ident);
      for (auto& it: mo.unqualified_import)
        c += it->defined.count(expr.ident);
      if (! c) {
        n_errors++;
        mo.locfile.error(expr.loc, "'%s' undefined", expr.ident.c_str());
      }
    }
  }
  void visit(EmbedExpr& expr) override {
    if (expr.qualified.size()) {
      if (! mo.qualified_import.count(expr.qualified)) {
        n_errors++;
        mo.locfile.error(expr.loc, "Unknown module '%s'", expr.qualified.c_str());
      } else {
        auto it = mo.qualified_import[expr.qualified]->defined.find(expr.ident);
        if (it == mo.qualified_import[expr.qualified]->defined.end()) {
          n_errors++;
          mo.locfile.error(expr.loc, "'%s::%s' undefined", expr.qualified.c_str(), expr.ident.c_str());
        } else {
          depended_by[it->second].push_back(define_stmt);
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
        depended_by[it->second].push_back(define_stmt);
        expr.define_stmt = it->second;
      }
    }
  }
};

Module* load_module(long& n_errors, const string& filename)
{
  FILE* file = filename != "-" ? fopen(filename.c_str(), "r") : stdin;
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
  if (inode2module.count(inode))
    return &inode2module[inode];
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
  unordered_map<DefineStmt*, i8> vis; // 0: unvisited; 1: in stack: 2: visited
  function<bool(DefineStmt*)> dfs = [&](DefineStmt* u) {
    if (vis[u] == 2)
      return false;
    if (vis[u] == 1) {
      u->module->locfile.error(u->loc, "'%s': circular embedding", u->lhs.c_str());
      long i = st.size();
      while (st[i-1] != u)
        i--;
      st.push_back(st[i-1]);
      for (; i < st.size(); i++) {
        long line1, col1, _line2, col2;
        st[i]->module->locfile.locate(st[i]->loc, line1, col1, _line2, col2);
        fprintf(stderr, YELLOW"  %s" CYAN":%ld:%ld-%ld: " RED"required by %s\n", st[i]->module->locfile.filename.c_str(), line1+1, col1+1, col2, st[i]->lhs.c_str());
        st[i]->module->locfile.context(st[i]->loc);
      }
      fputs("\n", stderr);
      return true;
    }
    vis[u] = 1;
    st.push_back(u);
    for (auto v: depended_by[u])
      if (dfs(v))
        return true;
    st.pop_back();
    vis[u] = 2;
    topo.push_back(u);
    return false;
  };
  for (auto& d: depended_by)
    if (dfs(d.first)) { // detected cycle
      n_errors++;
      return topo;
    }
  reverse(ALL(topo));
  return topo;
}

long load(const string& filename)
{
  long n_errors = 0;
  Module* mo = load_module(n_errors, filename);
  if (! mo) {
    err_exit(EX_OSFILE, "open", filename.c_str());
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

  if (opt_module_info)
    for (auto& it: inode2module)
      if (it.second.status == GOOD) {
        Module& mo = it.second;
        print_module_info(mo);
      }

  if (opt_dump_tree) {
    StmtPrinter p;
    for (auto& it: inode2module)
      if (it.second.status == GOOD) {
        Module& mo = it.second;
        printf("=== %s\n", mo.filename.c_str());
        for (Stmt* x = mo.toplevel; x; x = x->next)
          x->accept(p);
      }
  }

  DP(1, "Topological sorting");
  vector<DefineStmt*> topo = topo_define_stmts(n_errors);
  if (n_errors)
    return n_errors;

  DP(1, "Compiling DefineStmt");
  if (1) {
    //for (auto stmt: topo)
    //  printf("%s %s\n", stmt->module->filename.c_str(), stmt->lhs.c_str());
    for (auto stmt: topo)
      compile(stmt);
  }

  DP(1, "Linking CollapseExpr");

  DP(1, "Output");
  for (Stmt* x = mo->toplevel; x; x = x->next)
    if (auto xx = dynamic_cast<DefineStmt*>(x))
      if (xx->export_)
        export_statement(xx);

  return n_errors;
}

void unload_all()
{
  for (auto& it: inode2module) {
    Module& mo = it.second;
    stmt_free(mo.toplevel);
  }
}
