#include "ast.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation { void *pointer; struct Allocation *next; } Allocation;
static Allocation *allocations;
List *program;
const char *source_name;
int had_error;

void *arena_alloc(size_t size) {
    void *p = calloc(1, size);
    Allocation *a = malloc(sizeof(*a));
    if (!p || !a) {
        free(p); free(a);
        fputs("Ошибка: недостаточно памяти.\n", stderr);
        arena_free();
        exit(3);
    }
    a->pointer = p; a->next = allocations; allocations = a;
    return p;
}
char *arena_string(const char *text) {
    size_t size = strlen(text) + 1;
    char *result = arena_alloc(size);
    memcpy(result, text, size);
    return result;
}
void arena_free(void) {
    while (allocations) {
        Allocation *next = allocations->next;
        free(allocations->pointer); free(allocations); allocations = next;
    }
}
List *list_new(void) { return arena_alloc(sizeof(List)); }
List *list_add(List *list, void *item) {
    Link *link = arena_alloc(sizeof(*link)); link->item = item;
    if (list->tail) list->tail->next = link; else list->head = link;
    list->tail = link; ++list->count; return list;
}
Expr *expr_new(ExprKind kind, Position pos) {
    Expr *e = arena_alloc(sizeof(*e)); e->kind = kind; e->pos = pos; return e;
}
Expr *expr_text(ExprKind kind, char *text, Position pos) {
    Expr *e = expr_new(kind, pos); e->text = text; return e;
}
Expr *expr_binary(int op, Expr *left, Expr *right, Position pos) {
    Expr *e = expr_new(EX_BINARY, pos); e->op = op; e->left = left; e->right = right; return e;
}
Expr *expr_unary(int op, Expr *value, Position pos) {
    Expr *e = expr_new(EX_UNARY, pos); e->op = op; e->left = value; return e;
}
Expr *expr_list(ExprKind kind, char *name, List *items, Position pos) {
    Expr *e = expr_new(kind, pos); e->text = name; e->items = items; return e;
}
Stmt *stmt_new(StmtKind kind, ValueType type, char *name, Expr *value, Position pos) {
    Stmt *s = arena_alloc(sizeof(*s)); s->kind = kind; s->pos = pos;
    s->declared = type; s->name = name; s->value = value; return s;
}
void diagnostic(Position pos, const char *stage, const char *format, ...) {
    va_list ap;
    if (had_error) return;
    had_error = 1;
    fprintf(stderr, "%s:%d:%d: %s: ", source_name, pos.line, pos.column, stage);
    va_start(ap, format); vfprintf(stderr, format, ap); va_end(ap);
    fputc('\n', stderr);
}
