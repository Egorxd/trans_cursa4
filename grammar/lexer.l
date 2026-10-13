%option noyywrap nounput noinput nodefault never-interactive
%x COMMENT
%{
#include "ast.h"
#include "parser.h"
#include <stddef.h>
static int current_line = 1, current_column = 1;
static Position comment_start;
static size_t token_count;
static void locate(const char *text, int length) {
    int i;
    yylloc.first_line = current_line; yylloc.first_column = current_column;
    yylloc.last_line = current_line; yylloc.last_column = current_column;
    for (i = 0; i < length; ++i) {
        yylloc.last_line = current_line; yylloc.last_column = current_column;
        if (text[i] == '\n') { ++current_line; current_column = 1; }
        else if (((unsigned char)text[i] & 0xC0) != 0x80) ++current_column;
    }
}
#define YY_USER_ACTION locate(yytext, yyleng);
#define RETURN_TOKEN(t) do { if (++token_count > 100000) { diagnostic((Position){yylloc.first_line, yylloc.first_column}, "ошибка", "превышен предел 100000 токенов"); return INVALID; } return (t); } while (0)
%}
DIGIT [0-9]
EXP [eE][+-]?{DIGIT}+
REAL ({DIGIT}+(\.{DIGIT}*)?|\.{DIGIT}+)({EXP})?
%%
\xEF\xBB\xBF { if (yylloc.first_line != 1 || yylloc.first_column != 1) { diagnostic((Position){yylloc.first_line,yylloc.first_column}, "лексическая ошибка", "BOM допустим только в начале файла"); return INVALID; } current_column = 1; }
[ \t\r\n]+ ;
"//"[^\r\n]* ;
"#"[^\r\n]* ;
"/*" { comment_start = (Position){yylloc.first_line, yylloc.first_column}; BEGIN(COMMENT); }
<COMMENT>"*/" { BEGIN(INITIAL); }
<COMMENT>[^*\n]+ ;
<COMMENT>"*" ;
<COMMENT>\n ;
<COMMENT><<EOF>> { diagnostic(comment_start, "лексическая ошибка", "незавершённый комментарий /* ... */"); BEGIN(INITIAL); return INVALID; }
"matrix" { RETURN_TOKEN(MATRIX); }
"number" { RETURN_TOKEN(NUMBER); }
"print" { RETURN_TOKEN(PRINT); }
{REAL} { yylval.text = arena_string(yytext); RETURN_TOKEN(NUM); }
[A-Za-z_][A-Za-z_0-9]* { yylval.text = arena_string(yytext); RETURN_TOKEN(IDENT); }
[+*()\[\],;=-] { RETURN_TOKEN((unsigned char)yytext[0]); }
\x00 { diagnostic((Position){yylloc.first_line,yylloc.first_column}, "лексическая ошибка", "нулевой байт во входном тексте"); return INVALID; }
. { diagnostic((Position){yylloc.first_line,yylloc.first_column}, "лексическая ошибка", "недопустимый символ (байт 0x%02X)", (unsigned char)yytext[0]); return INVALID; }
<<EOF>> { yylloc.first_line = yylloc.last_line = current_line; yylloc.first_column = yylloc.last_column = current_column; return 0; }
%%
