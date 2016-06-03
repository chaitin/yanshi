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

%x IN_ACTION
%x IN_BRACE
%x IN_CODE
%x IN_BRACKET
%x IN_BRACKET_FIRST
%x IN_LINE_COMMENT
%x IN_Q_STRING
%x IN_QQ_STRING

D			[0-9]
L			[a-zA-Z_]

%%

"::" return SEMISEMI;
[-!&*()=+,.?|{}] { return yytext[0]; }

"action" return ACTION;
"as" return AS;
"c++" return CPP;
"epsilon" return EPSILON;
"export" return EXPORT;
"import" return IMPORT;
"intact" return INTACT;
{L}({L}|{D})* { yylval->str = new string(yytext); return IDENT; }
{D}+ { yylval->integer = atol(yytext); return INTEGER; }

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
    if (YY_START == INITIAL)
      return ']';
  }
  [^-\\\]\n] yylval->integer = yytext[0]; return CHAR;
}
<IN_BRACKET_FIRST,IN_BRACKET>{
  \\. yylval->integer = yytext[1]; return CHAR;
  - return '-';
  <<EOF>> yy_pop_state(yyscanner); unexpected_eof(yylval, "]");
  "\n" unexpected_newline(yylval, "]"); return INVALID_CHARACTER;
}

[>@%$] BEGIN IN_ACTION; return yytext[0];
<IN_ACTION>"{" tmp_bracket.clear(); yy_push_state(IN_CODE, yyscanner);
<IN_CODE>{
  "'" { tmp_bracket += '\''; yy_push_state(IN_Q_STRING, yyscanner); }
  "\"" { tmp_bracket += '"'; yy_push_state(IN_QQ_STRING, yyscanner); }
  "{" { tmp_bracket += '{'; yy_push_state(IN_CODE, yyscanner); }
  "}" {
    yy_pop_state(yyscanner);
    if (YY_START == IN_ACTION) {
      BEGIN INITIAL;
      yylval->str = new string(tmp_bracket);
      return BRACED_CODE;
    } else
      tmp_bracket += '}';
  }
  .|"\n" tmp_bracket += yytext[0];
  <<EOF>> yy_pop_state(yyscanner); unexpected_eof(yylval, "}");
}

"'" tmp_str.clear(); yy_push_state(IN_Q_STRING, yyscanner);
<IN_Q_STRING>{
  "'" {
    yy_pop_state(yyscanner);
    if (YY_START == INITIAL) {
      yylval->str = new string(tmp_str);
      return STRING_LITERAL;
    }
    tmp_bracket += yytext;
  }
  \\' tmp_str += '\''; tmp_bracket += yytext;
  \\\\ tmp_str += '\\'; tmp_bracket += yytext;
  .|\n tmp_str += yytext[0]; tmp_bracket += yytext[0];
  <<EOF>> yy_pop_state(yyscanner); unexpected_eof(yylval, "'");
}

"\"" tmp_str.clear(); yy_push_state(IN_QQ_STRING, yyscanner);
<IN_QQ_STRING>{
  "\"" {
    yy_pop_state(yyscanner);
    if (YY_START == INITIAL) {
      yylval->str = new string(tmp_str);
      return STRING_LITERAL;
    }
    tmp_bracket += yytext;
  }
  \\[0-7]{1,3} {
    long c = strtol(yytext+1, NULL, 8);
    if (UCHAR_MAX < c) {
      yylval->errmsg = aprintf("Invalid number after \\-escape: %s", yytext+1);
      return INVALID_CHARACTER;
    }
    tmp_str.push_back(c);
    tmp_bracket += yytext;
  }
  \\x[0-9a-fA-F]+ {
    long c = strtol(yytext+2, NULL, 16);
    if (UCHAR_MAX < c) {
      yylval->errmsg = aprintf("Invalid number after \\-escape: %s", yytext+1);
      return INVALID_CHARACTER;
    }
    tmp_str.push_back(c);
    tmp_bracket += yytext;
  }
  \\a tmp_str += '\a'; tmp_bracket += yytext;
  \\b tmp_str += '\b'; tmp_bracket += yytext;
  \\f tmp_str += '\f'; tmp_bracket += yytext;
  \\n tmp_str += '\n'; tmp_bracket += yytext;
  \\r tmp_str += '\r'; tmp_bracket += yytext;
  \\t tmp_str += '\t'; tmp_bracket += yytext;
  \\v tmp_str += '\v'; tmp_bracket += yytext;
  \\[\n\"\'?\\] tmp_str += yytext[1]; tmp_bracket += yytext;
  \\. {
    yylval->errmsg = aprintf("Invalid \\-escape: %s", yytext);
    return INVALID_CHARACTER;
  }
  .|\n tmp_str += yytext[0]; tmp_bracket += yytext[0];
  <<EOF>> unexpected_eof(yylval, "\""); BEGIN INITIAL;
}

"\n" return '\n';
[ \t]+ {}
. { yylval->errmsg = strdup("Invalid character"); return INVALID_CHARACTER; }
