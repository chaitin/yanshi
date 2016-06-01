#include "common.hh"
#include "loader.hh"
#include "option.hh"
#include "parser.hh"

#include <algorithm>
#include <string.h>
#include <stdio.h>
#include <sysexits.h>
#include <utility>
#include <sys/stat.h>
using namespace std;

static map<pair<dev_t, ino_t>, Module> inode2module;

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
    printf("  %s\n", x.c_str());
}

struct ModuleImportDef : PreorderStmtVisitor {
  Module& mo;
  long& n_errors;
  ModuleImportDef(Module& mo, long& n_errors) : mo(mo), n_errors(n_errors) {}

  void visit(ActionStmt& stmt) override {
    if (mo.named_action.count(stmt.ident)) {
      n_errors++;
      mo.locfile.locate(stmt.loc, "Redefined '%s'", stmt.ident);
    }
    mo.named_action[stmt.ident] = stmt.code;
  }
  void visit(DefineStmt& stmt) override {
    mo.defined.insert(stmt.lhs);
  }
  void visit(ImportStmt& stmt) override {
    Module* m = load_module(n_errors, stmt.filename);
    if (! m) {
      n_errors++;
      mo.locfile.locate(stmt.loc, "'%s': %s", stmt.filename, errno ? strerror(errno) : "parse error");
      return;
    }
    if (stmt.qualified)
      mo.qualified_import[stmt.qualified] = m;
    else if (count(ALL(mo.unqualified_import), m) == 0)
      mo.unqualified_import.push_back(m);
  }
};

struct ModuleUse : PreorderActionExprStmtVisitor {
  Module& mo;
  long& n_errors;
  ModuleUse(Module& mo, long& n_errors) : mo(mo), n_errors(n_errors) {}

  void visit(BracketExpr& expr) override {}
  void visit(CollapseExpr& expr) override {
    if (expr.qualified) {
      if (! mo.qualified_import.count(expr.qualified)) {
        n_errors++;
        mo.locfile.locate(expr.loc, "Unknown module '%s'", expr.qualified);
      } else if (! mo.qualified_import[expr.qualified]->defined.count(expr.ident)) {
        n_errors++;
        mo.locfile.locate(expr.loc, "'%s::%s' undefined", expr.qualified, expr.ident);
      }
    } else {
      long c = mo.defined.count(expr.ident);
      for (auto& it: mo.unqualified_import)
        c += it->defined.count(expr.ident);
      if (! c) {
        n_errors++;
        mo.locfile.locate(expr.loc, "'%s' undefined", expr.ident);
      }
    }
  }
  void visit(EmbedExpr& expr) override {
    if (expr.qualified) {
      if (! mo.qualified_import.count(expr.qualified)) {
        n_errors++;
        mo.locfile.locate(expr.loc, "Unknown module '%s'", expr.qualified);
      } else if (! mo.qualified_import[expr.qualified]->defined.count(expr.ident)) {
        n_errors++;
        mo.locfile.locate(expr.loc, "'%s::%s' undefined", expr.qualified, expr.ident);
      }
    } else {
      long c = mo.defined.count(expr.ident);
      for (auto& it: mo.unqualified_import)
        c += it->defined.count(expr.ident);
      if (! c) {
        n_errors++;
        mo.locfile.locate(expr.loc, "'%s' undefined", expr.ident);
      }
    }
  }
  //void visit(PlusExpr& expr) override {
  //  expr.inner->accept(*this);
  //}
  //void visit(UnionExpr& expr) override {
  //  expr.lhs->accept(*this);
  //  expr.rhs->accept(*this);
  //}
};

Module* load_module(long& n_errors, const char* filename)
{
  FILE* file = strcmp(filename, "-") ? fopen(filename, "r") : stdin;
  if (! file)
    return NULL;
  pair<dev_t, ino_t> inode{0, 0}; // stdin -> {0, 0}
  if (file != stdin) {
    struct stat sb;
    if (fstat(fileno(file), &sb) < 0)
      err_exit(EX_OSFILE, "fstat '%s'", filename);
    inode = {sb.st_dev, sb.st_ino};
  }
  if (inode2module.count(inode))
    return &inode2module[inode];

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
  long errors = parse(locfile, toplevel);
  if (! toplevel) {
    n_errors += errors;
    return NULL;
  }
  Module& mo = inode2module[inode];
  mo.locfile = locfile;
  mo.filename = filename;
  mo.toplevel = toplevel;
  return &mo;
}

long load(const char* filename)
{
  long n_errors = 0;
  if (! load_module(n_errors, filename))
    return n_errors;

  if (! n_errors)
    for (auto& it: inode2module) {
      Module& mo = it.second;
      ModuleImportDef p{mo, n_errors};
      for (Stmt* x = mo.toplevel; x; x = x->next)
        x->accept(p);
    }

  if (! n_errors)
    for (auto& it: inode2module) {
      Module& mo = it.second;
      ModuleUse p{mo, n_errors};
      for (Stmt* x = mo.toplevel; x; x = x->next)
        x->accept(p);
    }

  if (opt_module_info && ! n_errors)
    for (auto& it: inode2module) {
      Module& mo = it.second;
      print_module_info(mo);
    }

  if (opt_dump_tree && ! n_errors) {
    StmtPrinter p;
    for (auto& it: inode2module) {
      Module& mo = it.second;
      for (Stmt* x = mo.toplevel; x; x = x->next)
        x->accept(p);
    }
  }

  return n_errors;
}

void unload_all()
{
  for (auto& it: inode2module) {
    Module& mo = it.second;
    stmt_free(mo.toplevel);
  }
}
