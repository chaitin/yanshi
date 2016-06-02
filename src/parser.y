%code requires {
#include "common.hh"
#include "location.hh"
#include "syntax.hh"
using std::bitset;

#define YYLTYPE Location
#define YYLLOC_DEFAULT(Loc, Rhs, N)             \
  do {                                          \
    if (N) {                                    \
      (Loc).start = YYRHSLOC(Rhs, 1).start;     \
      (Loc).end = YYRHSLOC(Rhs, N).end;         \
    } else {                                    \
      (Loc).start = YYRHSLOC(Rhs, 0).end;       \
      (Loc).end = YYRHSLOC(Rhs, 0).end;         \
    }                                           \
  } while (0)

int parse(const LocationFile& locfile, Stmt*& res);
}

%locations
%error-verbose
%define api.pure

%parse-param {Stmt*& res}
%parse-param {long& errors}
%parse-param {const LocationFile& locfile}
%parse-param {void** lexer}
%lex-param {Stmt*& res}
%lex-param {long& errors}
%lex-param {const LocationFile& locfile}
%lex-param {void** lexer}

%union {
  long integer;
  string* str;
  bitset<AB>* charset;
  Action* action;
  Expr* expr;
  Stmt* stmt;
  char* errmsg;
}
%destructor { delete $$; } <str>
%destructor { delete $$; } <action>
%destructor { delete $$; } <expr>
%destructor { delete $$; } <stmt>
%destructor { delete $$; } <charset>

%token ACTION AS EXPORT IMPORT INVALID_CHARACTER SEMISEMI
%token <integer> CHAR INTEGER
%token <str> IDENT
%token <str> BRACED_CODE
%token <str> STRING_LITERAL

%nonassoc IDENT
%nonassoc '.'

%type <action> action
%type <stmt> stmt stmt_list
%type <expr> concat_expr difference_expr factor intersect_expr union_expr
%type <charset> bracket bracket_items

%{
#include "lexer.hh"

#define FAIL(loc, errmsg)                                          \
  do {                                                             \
    Location l = loc;                                              \
    yyerror(&l, res, errors, locfile, lexer, errmsg);              \
  } while (0)

void yyerror(YYLTYPE* loc, Stmt*& res, long& errors, const LocationFile& locfile, yyscan_t* lexer, const char *errmsg)
{
  errors++;
  locfile.error_context(*loc, "%s", errmsg);
}

int yylex(YYSTYPE* yylval, YYLTYPE* loc, Stmt*& res, long& errors, const LocationFile& locfile, yyscan_t* lexer)
{
  int token = raw_yylex(yylval, loc, *lexer);
  if (token == INVALID_CHARACTER) {
    FAIL(*loc, yylval->errmsg ? yylval->errmsg : "invalid character");
    free(yylval->errmsg);
  }
  return token;
}
%}

%%

toplevel:
  stmt_list { res = $1; }

stmt_list:
    %empty { $$ = new EmptyStmt; }
  | '\n' stmt_list { $$ = $2; }
  | stmt '\n' stmt_list { $1->next = $3; $3->prev = $1; $$ = $1; }
  | error '\n' stmt_list { $$ = $3; }

stmt:
    IDENT '=' union_expr { $$ = new DefineStmt(false, *$1, $3); delete $1; $$->loc = yyloc; }
  | EXPORT IDENT '=' union_expr { $$ = new DefineStmt(true, *$2, $4); delete $2; $$->loc = yyloc; }
  | IMPORT STRING_LITERAL AS IDENT { $$ = new ImportStmt(*$2, *$4); delete $2; delete $4; $$->loc = yyloc; }
  | IMPORT STRING_LITERAL { string t; $$ = new ImportStmt(*$2, t); delete $2; $$->loc = yyloc; }
  | ACTION IDENT BRACED_CODE { $$ = new ActionStmt(*$2, *$3); delete $2; delete $3; $$->loc = yyloc; }
  /*| error {}*/

union_expr:
    intersect_expr { $$ = $1; }
  | union_expr '|' intersect_expr { $$ = new UnionExpr($1, $3); }

intersect_expr:
    difference_expr { $$ = $1; }
  | intersect_expr '&' difference_expr { $$ = new IntersectExpr($1, $3); }

difference_expr:
    concat_expr { $$ = $1; }
  | difference_expr '-' concat_expr { $$ = new DifferenceExpr($1, $3); }

concat_expr:
    factor { $$ = $1; }
  | concat_expr factor { $$ = new ConcatExpr($1, $2); }

factor:
    IDENT { string t; $$ = new EmbedExpr(t, *$1); delete $1; $$->loc = yyloc; }
  | IDENT SEMISEMI IDENT { $$ = new EmbedExpr(*$1, *$3); delete $1; delete $3; $$->loc = yyloc; }
  | '!' IDENT { string t; $$ = new CollapseExpr(t, *$2); delete $2; $$->loc = yyloc; }
  | '!' IDENT SEMISEMI IDENT { $$ = new CollapseExpr(*$2, *$4); delete $2; delete $4; $$->loc = yyloc; }
  | STRING_LITERAL { $$ = new LiteralExpr(*$1); delete $1; $$->loc = yyloc; }
  | '.' { $$ = new DotExpr(); $$->loc = yyloc; }
  | bracket { $$ = new BracketExpr($1); }
  | '(' union_expr ')' { $$ = $2; }
  | '(' error ')' { $$ = new DotExpr; }
  | factor '>' action { $$ = $1; $$->entering.push_back($3); }
  | factor '@' action { $$ = $1; $$->finishing.push_back($3); }
  | factor '%' action { $$ = $1; $$->leaving.push_back($3); }
  | factor '$' action { $$ = $1; $$->transiting.push_back($3); }
  | factor '+' { $$ = new PlusExpr($1); }
  | factor '?' { $$ = new QuestionExpr($1); }
  | factor '*' { $$ = new StarExpr($1); }

action:
    IDENT { $$ = new RefAction(*$1); delete $1; $$->loc = yyloc; }
  | BRACED_CODE { $$ = new InlineAction(*$1); delete $1; $$->loc = yyloc; }

bracket:
    '[' bracket_items ']' { $$ = $2; }
  | '[' '^' bracket_items ']' {
      $$ = $3;
      REP(i, $$->size())
        $3->flip(i);
    }

bracket_items:
    bracket_items CHAR '-' CHAR {
      $$ = $1;
      if ($2 > $4)
        FAIL(yyloc, "negative range in character class");
      else
        FOR(i, $2, $4+1)
          $$->set(i);
    }
  | bracket_items CHAR {
      $$ = $1;
      $$->set($2);
    }
  | %empty { $$ = new bitset<AB>; }

%%

int parse(const LocationFile& locfile, Stmt*& res)
{
  yyscan_t lexer;
  raw_yylex_init_extra(0, &lexer);
  YY_BUFFER_STATE buf = raw_yy_scan_bytes(locfile.data.c_str(), locfile.data.size(), lexer);
  long errors = 0;
  yyparse(res, errors, locfile, &lexer);
  raw_yy_delete_buffer(buf, lexer);
  raw_yylex_destroy(lexer);
  if (errors > 0) {
    stmt_free(res);
    res = NULL;
  }
  return errors;
}
