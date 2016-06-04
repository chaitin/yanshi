#include "compiler.hh"
#include "fsa_anno.hh"
#include "loader.hh"
#include "option.hh"

#include <algorithm>
#include <ctype.h>
#include <limits.h>
#include <map>
#include <sstream>
#include <stack>
#include <typeinfo>
#include <unordered_map>
using namespace std;

static map<DefineStmt*, FsaAnno> compiled;

static void print_assoc(const FsaAnno& anno)
{
  magenta(); printf("=== Associated Expr of each state\n"); sgr0();
  REP(i, anno.fsa.n()) {
    printf("%ld:", i);
    for (auto aa: anno.assoc[i]) {
      auto a = aa.first;
      const char* name = typeid(*a).name();
      while (name && isdigit(name[0]))
        name++;
      string t = name;
      t = t.substr(t.size()-4); // suffix 'Expr'
      printf(" %s(%ld-%ld", t.c_str(), a->loc.start, a->loc.end);
      if (a->entering.size())
        printf(",>%zd", a->entering.size());
      if (a->leaving.size())
        printf(",%%%zd", a->leaving.size());
      if (a->finishing.size())
        printf(",@%zd", a->finishing.size());
      if (a->transiting.size())
        printf(",$%zd", a->transiting.size());
      printf(")");
    }
    puts("");
  }
  puts("");
}

static void print_automaton(const Fsa& fsa)
{
  magenta(); printf("=== Automaton\n"); sgr0();
  green(); printf("start: %ld\n", fsa.start);
  red(); printf("finals:");
  for (long i: fsa.finals)
    printf(" %ld", i);
  puts("");
  sgr0(); puts("edges:");
  REP(i, fsa.n()) {
    printf("%ld:", i);
    for (auto& x: fsa.adj[i])
      printf(" (%ld,%ld)", x.first, x.second);
    puts("");
  }
  puts("");
}

Expr* find_lca(Expr* u, Expr* v)
{
  if (u->depth > v->depth)
    swap(u, v);
  if (u->depth < v->depth)
    for (long k = 63-__builtin_clzl(v->depth-u->depth); k >= 0; k--)
      if (u->depth <= v->depth-(1L<<k))
        v = v->anc[k];
  if (u == v)
    return u;
  if (v->depth)
    for (long k = 63-__builtin_clzl(v->depth); k >= 0; k--)
      if (u->anc[k] != v->anc[k])
        u = u->anc[k], v = v->anc[k];
  return u->anc[0] == v->anc[0] ? u->anc[0] : NULL;
}

struct Compiler : Visitor<Expr> {
  stack<FsaAnno> st;
  stack<Expr*> path;
  long tick = 0;

  void pre_expr(Expr& expr) {
    expr.pre = tick++;
    expr.depth = path.size();
    if (path.size()) {
      expr.anc.assign(1, path.top());
      for (long k = 1; 1L << k <= expr.depth; k++)
        expr.anc.push_back(expr.anc[k-1]->anc[k-1]);
    } else
      expr.anc.assign(1, nullptr);
    path.push(&expr);
  }
  void post_expr(Expr& expr) {
    path.pop();
    expr.post = tick;
  }

  void visit(Expr& expr) override {
    pre_expr(expr);
    expr.accept(*this);
    post_expr(expr);
  }
  void visit(BracketExpr& expr) override {
    st.push(FsaAnno::bracket(expr));
  }
  void visit(CollapseExpr& expr) override {
    st.push(FsaAnno::collapse(expr));
  }
  void visit(ComplementExpr& expr) override {
    visit(*expr.inner);
    st.top().complement(&expr);
  }
  void visit(ConcatExpr& expr) override {
    visit(*expr.rhs);
    FsaAnno rhs = move(st.top());
    visit(*expr.lhs);
    st.top().concat(rhs, &expr);
  }
  void visit(DifferenceExpr& expr) override {
    visit(*expr.rhs);
    FsaAnno rhs = move(st.top());
    visit(*expr.lhs);
    st.top().difference(rhs, &expr);
  }
  void visit(DotExpr& expr) override {
    st.push(FsaAnno::dot(&expr));
  }
  void visit(EmbedExpr& expr) override {
    FsaAnno anno = compiled[expr.define_stmt];
    anno.add_assoc(expr);
    st.push(anno);
  }
  void visit(EpsilonExpr& expr) override {
    st.push(FsaAnno::epsilon(&expr));
  }
  void visit(IntersectExpr& expr) override {
    visit(*expr.rhs);
    FsaAnno rhs = move(st.top());
    visit(*expr.lhs);
    st.top().intersect(rhs, &expr);
  }
  void visit(LiteralExpr& expr) override {
    st.push(FsaAnno::literal(expr));
  }
  void visit(PlusExpr& expr) override {
    visit(*expr.inner);
    st.top().plus(&expr);
  }
  void visit(QuestionExpr& expr) override {
    visit(*expr.inner);
    st.top().question(&expr);
  }
  void visit(RepeatExpr& expr) override {
    visit(*expr.inner);
    st.top().repeat(expr);
  }
  void visit(StarExpr& expr) override {
    visit(*expr.inner);
    st.top().star(&expr);
  }
  void visit(UnionExpr& expr) override {
    visit(*expr.rhs);
    FsaAnno rhs = move(st.top());
    visit(*expr.lhs);
    st.top().union_(rhs, &expr);
  }
};

void compile(DefineStmt* stmt)
{
  if (compiled.count(stmt))
    return;
  FsaAnno& anno = compiled[stmt];
  Compiler comp;
  comp.visit(*stmt->rhs);
  anno = move(comp.st.top());
}

void compile_actions(DefineStmt* stmt)
{
  FsaAnno& anno = compiled[stmt];
  auto find_within = [&](long u) {
    vector<pair<Expr*, ExprTag>> within;
    Expr* last = NULL;
    sort(ALL(anno.assoc[u]), [](const pair<Expr*, ExprTag>& x, const pair<Expr*, ExprTag>& y) {
      if (x.first->pre != y.first->pre)
        return x.first->pre < y.first->pre;
      return x.second < y.second;
    });
    for (auto aa: anno.assoc[u]) {
      Expr* stop = last ? find_lca(last, aa.first) : NULL;
      last = aa.first;
      for (Expr* x = aa.first; x != stop; x = x->anc[0])
        within.emplace_back(x, aa.second);
    }
    sort(ALL(within));
    return within;
  };
  decltype(anno.assoc) withins(anno.fsa.n());
  REP(i, anno.fsa.n())
    withins[i] = move(find_within(i));

  auto get_code = [](Action* action) {
    if (auto t = dynamic_cast<InlineAction*>(action))
      return t->code;
    else if (auto t = dynamic_cast<RefAction*>(action))
      return t->define_module->defined_action[t->ident];
    return string();
  };

#define D(S) \
            if (auto t = dynamic_cast<InlineAction*>(action)) \
              printf(S " %ld %ld %ld %s\n", u, e.first, v, t->code.c_str()); \
            else if (auto t = dynamic_cast<RefAction*>(action)) \
              printf(S " %ld %ld %ld %s\n", u, e.first, v, t->define_module->defined_action[t->ident].c_str());
#undef D
#define D(S)

  fprintf(output, "long yanshi_%s_transit(long u, long c)\n", stmt->lhs.c_str());
  fprintf(output, "{\n");
  indent(output, 1);
  fprintf(output, "long v = -1;\n");
  indent(output, 1);
  fprintf(output, "switch (u) {\n");
  REP(u, anno.fsa.n()) {
    if (anno.fsa.adj[u].empty())
      continue;
    indent(output, 1);
    fprintf(output, "case %ld:\n", u);
    indent(output, 2);
    fprintf(output, "switch (c) {\n");
    for (auto& e: anno.fsa.adj[u]) {
      long v = e.second;
      indent(output, 2);
      fprintf(output, "case %ld:\n", e.first);
      indent(output, 3);
      fprintf(output, "v = %ld;\n", v);
      auto ie = withins[u].end(), je = withins[v].end();

      // leaving = {u} - {v}
      for (auto i = withins[u].begin(), j = withins[v].begin(); i != ie; ++i) {
        while (j != je && i->first > j->first)
          ++j;
        if (j == je || i->first != j->first)
          for (auto action: i->first->leaving) {
            D("%%");
            indent(output, 3);
            fprintf(output, "{%s}\n", get_code(action).c_str());
          }
      }

      // entering = {v} - {u}
      for (auto i = withins[u].begin(), j = withins[v].begin(); j != je; ++j) {
        while (i != ie && i->first < j->first)
          ++i;
        if (i == ie || i->first != j->first)
          for (auto action: j->first->entering) {
            D(">");
            indent(output, 3);
            fprintf(output, "{%s}\n", get_code(action).c_str());
          }
      }

      // transiting = {v}
      for (auto j = withins[v].begin(); j != je; ++j)
        for (auto action: j->first->transiting) {
          D("$");
          indent(output, 3);
          fprintf(output, "{%s}\n", get_code(action).c_str());
        }

      // finishing = {v} if final
      for (auto j = withins[v].begin(); j != je; ++j)
        if (long(j->second) & long(ExprTag::final))
          for (auto action: j->first->finishing) {
            D("@");
            indent(output, 3);
            fprintf(output, "{%s}\n", get_code(action).c_str());
          }

      indent(output, 3);
      fprintf(output, "break;\n");
    }
    indent(output, 2);
    fprintf(output, "}\n");
    indent(output, 2);
    fprintf(output, "break;\n");
  }
  indent(output, 1);
  fprintf(output, "}\n");
  indent(output, 1);
  fprintf(output, "return v;\n");
  fprintf(output, "}\n");
}

void compile_export(DefineStmt* stmt)
{
  DP(2, "Exporting %s", stmt->lhs.c_str());
  FsaAnno& anno = compiled[stmt];

  DP(3, "Construct automaton with all referenced CollapseExpr's DefineStmt");
  vector<vector<pair<long, long>>> adj;
  decltype(anno.assoc) assoc;
  vector<vector<DefineStmt*>> cllps;
  long allo = 0;
  unordered_map<DefineStmt*, long> stmt2offset;
  function<void(DefineStmt*)> allocate_collapse = [&](DefineStmt* stmt) {
    if (stmt2offset.count(stmt))
      return;
    DP(4, "Allocate %ld to %s", allo, stmt->lhs.c_str());
    FsaAnno& anno = compiled[stmt];
    long old = stmt2offset[stmt] = allo;
    allo += anno.fsa.n()+1;
    adj.insert(adj.end(), ALL(anno.fsa.adj));
    REP(i, anno.fsa.n())
      for (auto& e: adj[old+i])
        e.second += old;
    adj.emplace_back(); // appeared in other automata, this is a vertex corresponding to the completion of 'stmt'
    assoc.insert(assoc.end(), ALL(anno.assoc));
    assoc.emplace_back();
    FOR(i, old, old+anno.fsa.n())
      if (anno.fsa.has(i-old, AB)) {
        for (auto aa: assoc[i])
          if (auto e = dynamic_cast<CollapseExpr*>(aa.first)) {
            DefineStmt* v = e->define_stmt;
            allocate_collapse(v);
            // (i@{CollapseExpr,...}, AB, _) -> ({CollapseExpr,...}, epsilon, CollapseExpr.define_stmt.start)
            sorted_insert(adj[i], make_pair(-1L, stmt2offset[v]+compiled[v].fsa.start));
          }
        long j = adj[i].size();
        while (j && adj[i][j-1].first == AB) {
          long v = adj[i][--j].second;
          for (auto aa: assoc[v])
            if (auto e = dynamic_cast<CollapseExpr*>(aa.first)) {
              DefineStmt* w = e->define_stmt;
              allocate_collapse(w);
              // (_, AB, v@{CollapseExpr,...}) -> (CollapseExpr.define_stmt.final, epsilon, v)
              for (long f: compiled[w].fsa.finals) {
                long g = stmt2offset[w]+f;
                sorted_insert(adj[g], make_pair(-1L, v));
                if (g == i)
                  j++;
              }
            }
        }
        // remove (i, AB, _)
        adj[i].resize(j);
      }
  };
  allocate_collapse(stmt);
  anno.fsa.adj = move(adj);
  anno.assoc = move(assoc);
  anno.deterministic = false;

  // substring grammar & this nonterminal is not marked as intact
  if (opt_substring_grammar && ! stmt->intact) {
    DP(3, "Constructing substring grammar");
    anno.substring_grammar();
  }

  DP(3, "Determinize, minimize, remove dead states");
  anno.determinize();
  anno.minimize();
  allo = 0;
  auto relate = [&](long x) {
    if (allo != x)
      anno.assoc[allo] = move(anno.assoc[x]);
    allo++;
  };
  anno.fsa.remove_dead(relate);
  if (anno.fsa.finals.empty()) // 'start' does not produce acceptable strings
    anno.assoc.assign(1, {});
  else
    anno.assoc.resize(allo);

  if (opt_dump_automaton)
    print_automaton(anno.fsa);
  if (opt_dump_assoc)
    print_assoc(anno);
}

//// Graphviz dot renderer

void generate_graphviz(Module* mo)
{
  fprintf(output, "// Generated by 偃师, %s\n", mo->filename.c_str());
  for (Stmt* x = mo->toplevel; x; x = x->next)
    if (auto stmt = dynamic_cast<DefineStmt*>(x)) {
      if (stmt->export_) {
        compile_export(stmt);
        FsaAnno& anno = compiled[stmt];

        fprintf(output, "digraph \"%s\" {\n", mo->filename.c_str());
        bool start_is_final = false;

        // finals
        indent(output, 1);
        fprintf(output, "node[shape=doublecircle,color=olivedrab1,style=filled,fontname=Monospace];");
        for (long f: anno.fsa.finals)
          if (f == anno.fsa.start)
            start_is_final = true;
          else
            fprintf(output, " %ld", f);
        fprintf(output, "\n");

        // start
        indent(output, 1);
        if (start_is_final)
          fprintf(output, "node[shape=doublecircle,color=orchid];");
        else
          fprintf(output, "node[shape=circle,color=orchid];");
        fprintf(output, " %ld\n", anno.fsa.start);

        // other states
        indent(output, 1);
        fprintf(output, "node[shape=circle,color=black,style=\"\"]\n");

        // edges
        REP(u, anno.fsa.n()) {
          unordered_map<long, stringstream> labels;
          bool first = true;
          auto it = anno.fsa.adj[u].begin(), it2 = it, ite = anno.fsa.adj[u].end();
          for (; it != ite; it = it2) {
            long v = it->first;
            while (++it2 != ite && it->second == it2->second)
              v = it2->first;
            stringstream& lb = labels[it->second];
            if (! lb.str().empty())
              lb << ',';
            if (it->first == v)
              lb << v;
            else
              lb << it->first << '-' << v;
          }
          for (auto& lb: labels) {
            indent(output, 1);
            fprintf(output, "%ld -> %ld[label=\"%s\"]\n", u, lb.first, lb.second.str().c_str());
          }
        }
      }
    }
  fprintf(output, "}\n");
}

//// C++ renderer

void generate_cxx_export(DefineStmt* stmt)
{
  compile_export(stmt);
  FsaAnno& anno = compiled[stmt];

  fprintf(output, "void yanshi_%s_init(long& start, vector<long>& finals)\n", stmt->lhs.c_str());
  fprintf(output, "{\n");
  indent(output, 1);
  fprintf(output, "start = %ld;\n", anno.fsa.start);
  indent(output, 1);
  fprintf(output, "finals = {");
  bool first = true;
  for (long f: anno.fsa.finals) {
    if (first) first = false;
    else fprintf(output, ",");
    fprintf(output, "%ld", f);
  }
  fprintf(output, "};\n");
  fprintf(output, "}\n\n");

  DP(3, "Compiling actions");
  compile_actions(stmt);
}

void generate_cxx(Module* mo)
{
  fprintf(output, "// Generated by 偃师, %s\n", mo->filename.c_str());
  fprintf(output, "#include <vector>\n");
  fprintf(output, "using std::vector;\n");
  fprintf(output, "\n");
  for (Stmt* x = mo->toplevel; x; x = x->next)
    if (auto xx = dynamic_cast<DefineStmt*>(x)) {
      if (xx->export_)
        generate_cxx_export(xx);
    } else if (auto xx = dynamic_cast<CppStmt*>(x))
      fprintf(output, "%s", xx->code.c_str());
}
