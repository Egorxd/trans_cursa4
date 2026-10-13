#ifndef MAT_AST_H
#define MAT_AST_H
#include <stddef.h>
#include <stdio.h>

#define TRANSLATOR_VERSION "1.0.0"
typedef struct { int line, column; } Position;
typedef enum { TYPE_UNKNOWN, TYPE_NUMBER, TYPE_MATRIX } ValueType;
typedef enum { EX_NUMBER, EX_VARIABLE, EX_MATRIX, EX_BINARY, EX_UNARY, EX_CALL } ExprKind;
typedef struct Expr Expr;
typedef struct Link { void *item; struct Link *next; } Link;
typedef struct List { Link *head, *tail; size_t count; } List;
struct Expr {
    ExprKind kind;
    Position pos;
    ValueType type;
    size_t rows, cols, symbol;
    char *text;
    int op;
    Expr *left, *right;
    List *items;
};
typedef enum { ST_DECLARE, ST_ASSIGN, ST_PRINT } StmtKind;
typedef struct {
    StmtKind kind;
    Position pos;
    ValueType declared;
    char *name;
    Expr *value;
    size_t symbol;
} Stmt;

extern List *program;
extern const char *source_name;
extern int had_error;
void *arena_alloc(size_t size);
char *arena_string(const char *text);
void arena_free(void);
List *list_new(void);
List *list_add(List *list, void *item);
Expr *expr_new(ExprKind kind, Position pos);
Expr *expr_text(ExprKind kind, char *text, Position pos);
Expr *expr_binary(int op, Expr *left, Expr *right, Position pos);
Expr *expr_unary(int op, Expr *value, Position pos);
Expr *expr_list(ExprKind kind, char *name, List *items, Position pos);
Stmt *stmt_new(StmtKind kind, ValueType type, char *name, Expr *value, Position pos);
void diagnostic(Position pos, const char *stage, const char *format, ...);
int analyze(List *statements);
int generate(FILE *output, List *statements);
#endif
