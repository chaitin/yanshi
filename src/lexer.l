%{
#include "parser.hh"
#include "syntax.hh"
#include "lexer_helper.hh"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>
using namespace std;

#define YY_USER_ACTION                      \
  do {                                      \
    yylloc->start = yyget_extra(yyscanner); \
    yylloc->end = yylloc->start + yyleng;   \
    yyset_extra(yylloc->end, yyscanner);    \
  } while (0);

static int my_top_state(yyscan_t yyscanner);

static string tmp_bracket, tmp_str;

static void unexpected_eof(YYSTYPE* yylval, const char* token_end)
{
  yylval->errmsg = aprintf("Missing %s at end of file", token_end);
}

static void unexpected_newline(YYSTYPE* yylval, const char* token_end)
{
  yylval->errmsg = aprintf("Missing %s at end of line", token_end);
}
%}

%option noyywrap nounput noinput
%option reentrant
%option extra-type="long"
%option bison-bridge bison-locations
%option prefix="raw_yy"
%option stack

%x IN_BRACE
%x IN_BRACKET
%x IN_BRACKET_FIRST
%x IN_LINE_COMMENT
%x IN_Q_STRING
%x IN_QQ_STRING

D			[0-9]
L			[a-zA-Z_]
H			[a-fA-F0-9]
E			[Ee][+-]?{D}+
FS			(f|F|l|L)
IS			(u|U|l|L)*

%%

"::" return SEMISEMI;
"."|"!"|"?"|"="|";"|","|":"|"|"|"+"|"-"|"*"|"/"|"%"|"\$"|"<"|">"|"&" { return yytext[0]; }
"("|")" { return yytext[0]; }

"action" return ACTION;
"as" return AS;
"export" return EXPORT;
"import" return IMPORT;
{L}({L}|{D})* { yylval->str = new string(yytext); return IDENT; }

[0-9]+ { yylval->integer = atol(yytext); return INTEGER; }

"#" yy_push_state(IN_LINE_COMMENT, yyscanner);
<IN_LINE_COMMENT>{
  "\n" yy_pop_state(yyscanner);
  <<EOF>> yy_pop_state(yyscanner);
  . {}
}

"[" yy_push_state(IN_BRACKET_FIRST, yyscanner); return '[';
<IN_BRACKET_FIRST>{
  "^" { yy_pop_state(yyscanner); yy_push_state(IN_BRACKET, yyscanner); return '^'; }
  [^-\\\]\n] { yy_pop_state(yyscanner); yy_push_state(IN_BRACKET, yyscanner); yylval->integer = yytext[0]; return CHAR; }
}
<IN_BRACKET>{
  "]" {
    yy_pop_state(yyscanner);
    if (my_top_state(yyscanner) == INITIAL)
      return ']';
  }
  [^-\\\]\n] yylval->integer = yytext[0]; return CHAR;
}
<IN_BRACKET_FIRST,IN_BRACKET>{
  \\. yylval->integer = yytext[1]; return CHAR;
  - return '-';
  <<EOF>> unexpected_eof(yylval, "]"); return INVALID_CHARACTER;
  "\n" unexpected_newline(yylval, "]"); return INVALID_CHARACTER;
}

"{" tmp_bracket.clear(); yy_push_state(IN_BRACE, yyscanner);
<IN_BRACE>{
  "'" { tmp_bracket += '\''; yy_push_state(IN_Q_STRING, yyscanner); }
  "\"" { tmp_bracket += '"'; yy_push_state(IN_QQ_STRING, yyscanner); }
  "[" { tmp_bracket += '['; yy_push_state(IN_BRACKET, yyscanner); }
  "{" { tmp_bracket += '{'; yy_push_state(IN_BRACE, yyscanner); }
  "}" {
    yy_pop_state(yyscanner);
    if (my_top_state(yyscanner) == INITIAL) {
      yylval->str = new string(tmp_bracket);
      return BRACED_CODE;
    }
  }
  "\n" unexpected_newline(yylval, "}"); return INVALID_CHARACTER;
  . tmp_bracket += yytext[0];
  <<EOF>> unexpected_eof(yylval, "}"); return INVALID_CHARACTER;
}

"'" tmp_str.clear(); yy_push_state(IN_Q_STRING, yyscanner);
<IN_Q_STRING>{
  "'" {
    yy_pop_state(yyscanner);
    if (my_top_state(yyscanner) == INITIAL) {
      yylval->str = new string(tmp_str);
      return STRING_LITERAL;
    }
  }
  \\' tmp_str += '\'';
  \\n unexpected_newline(yylval, "'");
  . tmp_str += yytext[0];
  <<EOF>> unexpected_eof(yylval, "'");
}

"\"" tmp_str.clear(); yy_push_state(IN_QQ_STRING, yyscanner);
<IN_QQ_STRING>{
  "\"" {
    yy_pop_state(yyscanner);
    if (my_top_state(yyscanner) == INITIAL) {
      yylval->str = new string(tmp_str);
      return STRING_LITERAL;
    }
  }
  \\[0-7]{1,3} {
    long c = strtol(yytext+1, NULL, 8);
    if (UCHAR_MAX < c) {
      yylval->errmsg = aprintf("Invalid number after \\-escape: %s", yytext+1);
      return INVALID_CHARACTER;
    }
    tmp_str.push_back(c);
  }
  \\x[0-9a-fA-F]+ {
    long c = strtol(yytext+2, NULL, 16);
    if (UCHAR_MAX < c) {
      yylval->errmsg = aprintf("Invalid number after \\-escape: %s", yytext+1);
      return INVALID_CHARACTER;
    }
    tmp_str.push_back(c);
  }
  \\a tmp_str += '\a';
  \\b tmp_str += '\b';
  \\f tmp_str += '\f';
  \\n tmp_str += '\n';
  \\r tmp_str += '\r';
  \\t tmp_str += '\t';
  \\v tmp_str += '\v';
  \\[\"\'?\\] tmp_str += yytext[1];
  \\. {
    yylval->errmsg = aprintf("Invalid \\-escape: %s", yytext);
    return INVALID_CHARACTER;
  }
  . tmp_str += yytext[0];
  <<EOF>> unexpected_eof(yylval, "\""); return INVALID_CHARACTER;
  "\n" unexpected_newline(yylval, "\""); return INVALID_CHARACTER;
}

"\n" return '\n';
[ \t]+ {}
. { yylval->errmsg = strdup("Invalid character"); return INVALID_CHARACTER; }

%%

static int my_top_state(yyscan_t yyscanner)
{
  struct yyguts_t* yyg = (struct yyguts_t*)yyscanner;
  return yyg->yy_start_stack_ptr ? yyg->yy_start_stack[yyg->yy_start_stack_ptr - 1] : INITIAL;
}
