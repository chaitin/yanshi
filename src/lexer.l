%{
#include <cstdlib>
#include <cstring>
#include <string>
using namespace std;

#include "parser.hh"
#include "syntax.hh"

#define YY_USER_ACTION                      \
  do {                                      \
    yylloc->start = yyget_extra(yyscanner); \
    yylloc->end = yylloc->start + yyleng;   \
    yyset_extra(yylloc->end, yyscanner);    \
  } while (0);

static string tmp_str;
%}

%option noyywrap nounput noinput
%option reentrant
%option extra-type="long"
%option bison-bridge bison-locations
%option prefix="raw_yy"
%option stack

%x IN_BRACE
%x IN_BRACKET
%x IN_Q_STRING
%x IN_QQ_STRING

D			[0-9]
L			[a-zA-Z_]
H			[a-fA-F0-9]
E			[Ee][+-]?{D}+
FS			(f|F|l|L)
IS			(u|U|l|L)*

%%

"."|"?"|"="|";"|","|":"|"|"|"+"|"-"|"*"|"/"|"%"|"\$"|"<"|">" { return yytext[0]; }
"("|")" { return yytext[0]; }

{L}({L}|{D})* { yylval->string = strdup(yytext); return IDENT; }

[0-9] { yylval->integer = atol(yytext); return INTEGER; }

"[" yy_push_state(IN_BRACKET);
<IN_BRACKET>{
  "]" {
    yy_pop_state(yyscanner);
    if (yy_top_state(yyscanner) == INITIAL) {
      yylval->string = strdup(yytext);
      return BRACED_CODE;
    }
  }
  [^\].A-Za-z0-9_/ \f\n\t\v]+|. {
    // TODO
  }
  <<EOF>> unexpected_eof(yylval, "]");
  "\n" unexpected_newline(yylval, "]");
}

"{" yy_push_state(IN_BRACE);
<IN_BRACE>{
  "'" { yy_push_state(IN_Q_STRING, yyscanner); }
  "\"" { yy_push_state(IN_QQ_STRING, yyscanner); }
  "[" { yy_push_state(IN_BRACKET, yyscanner); }
  "{" { yy_push_state(IN_BRACE, yyscanner); }
  "}" {
    yy_pop_state(yyscanner);
    if (yy_top_state(yyscanner) == INITIAL) {
      yylval->string = strdup(yytext);
      return BRACED_CODE;
    }
  }
  <<EOF>> unexpected_eof(yylval, "}");
  "\n" unexpected_newline(yylval, "}");
}

"'" yy_push_state(IN_Q_STRING);
<IN_Q_STRING>{
  "'" {
    yy_pop_state(yyscanner);
    if (yy_top_state(yyscanner) == INITIAL) {
      yylval->string = strdup(tmp_str.c_str());
      return STRING_LITERAL;
    }
  }
  <<EOF>> unexpected_eof(yylval, "'");
  "\n" unexpected_newline(yylval, "'");
}

"\"" yy_push_state(IN_QQ_STRING);
<IN_QQ_STRING>{
  "\"" {
    yy_pop_state(yyscanner);
    if (yy_top_state(yyscanner) == INITIAL) {
      yylval->string = strdup(tmp_str.c_str());
      return STRING_LITERAL;
    }
  }
  <<EOF>> unexpected_eof(yylval, "\"");
  "\n" unexpected_newline(yylval, "\"");
}

<IN_Q_STRING,IN_QQ_STRING>{
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
    tmp_str.push_back();
  }
  \\a tmp_str += '\a';
  \\b tmp_str += '\b';
  \\f tmp_str += '\f';
  \\n tmp_str += '\n';
  \\r tmp_str += '\r';
  \\t tmp_str += '\t';
  \\v tmp_str += '\v';
  \\[\"\'?\\] tmp_str += yytext[1];
}

[ \n\t]+ {}
. { return INVALID_CHARACTER; }

%%

static void unexpected_eof(YYSTATE* yylval, const char* token_end)
{
  yylval->errmsg = aprintf("Missing %s at end of file", token_end);
}

static void unexpected_newline(YYSTATE* yylval, const char* token_end)
{
  yylval->errmsg = aprintf("Missing %s at end of line", token_end);
}
