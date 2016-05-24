%code requires {
#include "location.hh"
#include "syntax.hh"

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
}

%locations
%error-verbose
%define api.pure

%parse-param {Stmt* res}
%parse-param {long* errors}
%parse-param {LocationFile* locfile}
%lex-param {Stmt* res}
%parse-param {long* errors}
%lex-param {LocationFile* locfile}
%lex-param {struct yyscan_t* lexer}

%union {
  long integer;
  char* string;
  Expr* expr;
  Stmt* stmt;
  char* errmsg;
}
%destructor { free($$); } <string>
%destructor { delete $$; } <expr>
%destructor { delete $$; } <stmt>

%token INVALID_CHARACTER
%token <integer> INTEGER
%token <string> IDENT
%token <string> RANGE
%token <string> STRING_LITERAL
%token <string> BRACED_CODE

%right '|'
%left '+' '*' '?'

%type <stmt> stmt stmt_list
%type <expr> expr

%{
#define FAIL(loc, errmsg)                                             \
  do {                                                             \
    Location l = loc;                                              \
    yyerror(&l, answer, errors, locations, lexer, errmsg);  \
  } while (0)

int my_yylex(YYSTYPE* yylval, YYLTYPE* loc, long* errors, Stmt* res, LocationFile* locfile, struct yyscan_t* lexer)
{
  int token = yylex(yylval, loc, lexer);
  if (token == INVALID_CHARACTER) {
    FAIL(*loc, yylval->errmsg ? yylval->errmsg : "Invalid character");
    free(yylval->errmsg);
    yylval->errmsg = NULL;
  }
  return token;
}

void yyerror(YYLTYPE* loc, long* errors, Stmt* res, int* errors, LocationFile* locfile, struct yyscan_t* lexer, const char *errmsg)
{
  ++*errors;
  locfile->locate(*loc, "%s", errmsg);
}
%}

%%

toplevel: stmt_list;

stmt_list:
    %empty { $$ = new EmptyStmt; }
  | stmt stmt_list { $1->next = $2; $2->prev = $1; $$ = $1; }

stmt:
  IDENT '=' expr { $$ = new AssignStmt($1, $3); }

expr:
    IDENT { $$ = $1; }
  | '[' ']' { $$ = $1; }
  | '&' IDENT { $$ = $2; }

%%

int parse(LocationFile* locfile, Stmt* res)
{
  yyscan_t lexer;
  raw_yylex_init_extra(0, &lexer);
  YY_BUFFER_STATE buf = raw_yy_scan_bytes(locfile->data.c_str(), locfile->data.size(), lexer);
  int errors = 0;
  yyparse(res, &errors, locfile, &lexer);
  raw_yy_delete_buffer(buf, lexer);
  raw_yylex_destroy(lexer);
  if (*errors > 0) {
    // TODO
  }
  return *errors;
}
