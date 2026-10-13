%code requires {
#include "ast.h"
}
%{
#include "ast.h"
int yylex(void);
void yyerror(const char *message);
#define POS(loc) ((Position){(loc).first_line, (loc).first_column})
%}
%define parse.error detailed
%define parse.lac full
%locations
%union {
    char *text;
    Expr *expr;
    Stmt *stmt;
    List *list;
    ValueType type;
}
%token <text> IDENT "идентификатор" NUM "число"
%token MATRIX "matrix" NUMBER "number" PRINT "print" INVALID "недопустимый символ"
%type <type> type
%type <expr> expr matrix signed_number
%type <stmt> statement
%type <list> statements rows row opt_numbers numbers opt_args args
%left '+' '-'
%left '*'
%precedence UNARY
%start input
%%
input: statements { program = $1; };
statements:
    %empty { $$ = list_new(); }
  | statements statement { $$ = list_add($1, $2); }
;
type: MATRIX { $$ = TYPE_MATRIX; } | NUMBER { $$ = TYPE_NUMBER; };
statement:
    type IDENT '=' expr ';' { $$ = stmt_new(ST_DECLARE, $1, $2, $4, POS(@2)); }
  | IDENT '=' expr ';' { $$ = stmt_new(ST_ASSIGN, TYPE_UNKNOWN, $1, $3, POS(@1)); }
  | PRINT '(' expr ')' ';' { $$ = stmt_new(ST_PRINT, TYPE_UNKNOWN, NULL, $3, POS(@1)); }
;
expr:
    NUM { $$ = expr_text(EX_NUMBER, $1, POS(@1)); }
  | IDENT { $$ = expr_text(EX_VARIABLE, $1, POS(@1)); }
  | matrix { $$ = $1; }
  | '(' expr ')' { $$ = $2; }
  | expr '+' expr { $$ = expr_binary('+', $1, $3, POS(@2)); }
  | expr '-' expr { $$ = expr_binary('-', $1, $3, POS(@2)); }
  | expr '*' expr { $$ = expr_binary('*', $1, $3, POS(@2)); }
  | '-' expr %prec UNARY { $$ = expr_unary('-', $2, POS(@1)); }
  | '+' expr %prec UNARY { $$ = expr_unary('+', $2, POS(@1)); }
  | IDENT '(' opt_args ')' { $$ = expr_list(EX_CALL, $1, $3, POS(@1)); }
;
opt_args: %empty { $$ = list_new(); } | args { $$ = $1; };
args: expr { $$ = list_add(list_new(), $1); } | args ',' expr { $$ = list_add($1, $3); };
matrix:
    '[' rows ']' { $$ = expr_list(EX_MATRIX, NULL, $2, POS(@1)); }
  | '[' ']' { $$ = expr_list(EX_MATRIX, NULL, list_new(), POS(@1)); }
;
rows: row { $$ = list_add(list_new(), $1); } | rows ',' row { $$ = list_add($1, $3); };
row: '[' opt_numbers ']' { $$ = $2; };
opt_numbers: %empty { $$ = list_new(); } | numbers { $$ = $1; };
numbers:
    signed_number { $$ = list_add(list_new(), $1); }
  | numbers ',' signed_number { $$ = list_add($1, $3); }
;
signed_number:
    NUM { $$ = expr_text(EX_NUMBER, $1, POS(@1)); }
  | '-' NUM { $$ = expr_unary('-', expr_text(EX_NUMBER, $2, POS(@2)), POS(@1)); }
  | '+' NUM { $$ = expr_unary('+', expr_text(EX_NUMBER, $2, POS(@2)), POS(@1)); }
;
%%
void yyerror(const char *message) {
    diagnostic(POS(yylloc), "синтаксическая ошибка", "%s", message);
}
