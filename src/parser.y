%code requires {
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
  char* string;
  bitset<256>* charset;
  Expr* expr;
  Stmt* stmt;
  char* errmsg;
}
%destructor { free($$); } <string>
%destructor { if ($$) delete $$; } <expr>
%destructor { if ($$) delete $$; } <stmt>
%destructor { if ($$) delete $$; } <charset>

%token INVALID_CHARACTER
%token <integer> CHAR INTEGER
%token <string> IDENT
%token <string> RANGE
%token <string> STRING_LITERAL
%token <string> BRACED_CODE

%right '|'
%left '+' '*' '?'

%type <stmt> stmt stmt_list
%type <expr> expr
%type <charset> bracket bracket_items

%{
#include "lexer.hh"

#define FAIL(loc, errmsg)                                             \
  do {                                                             \
    Location l = loc;                                              \
    yyerror(&l, res, errors, locfile, lexer, errmsg);  \
  } while (0)

void yyerror(YYLTYPE* loc, Stmt*& res, long& errors, const LocationFile& locfile, yyscan_t* lexer, const char *errmsg)
{
  errors++;
  locfile.locate(*loc, "%s", errmsg);
}

int yylex(YYSTYPE* yylval, YYLTYPE* loc, Stmt*& res, long& errors, const LocationFile& locfile, yyscan_t* lexer)
{
  int token = raw_yylex(yylval, loc, *lexer);
  if (token == INVALID_CHARACTER) {
    FAIL(*loc, yylval->errmsg ? yylval->errmsg : "Invalid character");
    free(yylval->errmsg);
    yylval->errmsg = NULL;
  }
  return token;
}
%}

%%

toplevel:
  stmt_list { res = $1; }

stmt_list:
    %empty { $$ = new EmptyStmt; }
  | stmt stmt_list { $1->next = $2; $2->prev = $1; $$ = $1; }

stmt:
  IDENT '=' expr { $$ = new AssignStmt($1, $3); }

expr:
    IDENT { $$ = new EmbedExpr($1); }
  | bracket { $$ = new BracketExpr($1); }
  | '&' IDENT { $$ = new CollapseExpr($2); }

bracket:
    '[' bracket_items ']' { $$ = $2; }
  | '[' '^' bracket_items ']' { $$ = $3; }

bracket_items:
    bracket_items CHAR '-' CHAR {
      $$ = $1;
      $1 = NULL;
      FOR(i, $2, $4+1)
        $$->set(i);
    }
  | bracket_items CHAR {
      $$ = $1;
      $1 = NULL;
      $$->set($2);
    }
  | %empty { $$ = new bitset<256>; }

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
    // TODO
  }
  return errors;
}
