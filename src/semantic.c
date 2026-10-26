#include "ast.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct Symbol {
    char *name;
    ValueType type;
    size_t rows, cols, id;
    struct Symbol *next;
} Symbol;
static Symbol *symbols;
static size_t symbol_count;
static const char *type_name(ValueType type) { return type == TYPE_MATRIX ? "матрица" : "число"; }
static Symbol *find_symbol(const char *name) {
    Symbol *s;
    for (s = symbols; s; s = s->next) if (!strcmp(s->name, name)) return s;
    return NULL;
}
static int builtin(const char *name) {
    return !strcmp(name, "transpose") || !strcmp(name, "det") || !strcmp(name, "rank") || !strcmp(name, "inverse");
}
static int check_expr(Expr *e, unsigned depth) {
    Link *link;
    if (depth > 512) {
        diagnostic(e->pos, "семантическая ошибка", "глубина выражения превышает 512"); return 0;
    }
    switch (e->kind) {
    case EX_NUMBER: {
        char *end;
        double value;
        errno = 0; value = strtod(e->text, &end);
        if (*end || !isfinite(value) || (errno == ERANGE && value == 0.0)) {
            diagnostic(e->pos, "семантическая ошибка", "число '%s' вне допустимого диапазона float64", e->text); return 0;
        }
        e->type = TYPE_NUMBER; return 1;
    }
    case EX_VARIABLE: {
        Symbol *s = find_symbol(e->text);
        if (!s) { diagnostic(e->pos, "семантическая ошибка", "переменная '%s' не объявлена", e->text); return 0; }
        e->type = s->type; e->rows = s->rows; e->cols = s->cols; e->symbol = s->id; return 1;
    }
    case EX_MATRIX:
        if (!e->items->count) { diagnostic(e->pos, "семантическая ошибка", "матрица должна быть непустой"); return 0; }
        e->rows = e->items->count;
        e->cols = ((List *)e->items->head->item)->count;
        if (!e->cols) { diagnostic(e->pos, "семантическая ошибка", "строка матрицы должна быть непустой"); return 0; }
        for (link = e->items->head; link; link = link->next) {
            List *row = link->item;
            Link *element;
            if (row->count != e->cols) {
                diagnostic(e->pos, "семантическая ошибка", "все строки матрицы должны иметь одинаковое число элементов (ожидалось %zu, получено %zu)", e->cols, row->count); return 0;
            }
            for (element = row->head; element; element = element->next) if (!check_expr(element->item, depth + 1)) return 0;
        }
        e->type = TYPE_MATRIX; return 1;
    case EX_UNARY:
        if (!check_expr(e->left, depth + 1)) return 0;
        e->type = e->left->type; e->rows = e->left->rows; e->cols = e->left->cols; return 1;
    case EX_BINARY: {
        Expr *a = e->left, *b = e->right;
        if (!check_expr(a, depth + 1) || !check_expr(b, depth + 1)) return 0;
        if (a->type != b->type) {
            diagnostic(e->pos, "семантическая ошибка", "операция '%c' требует два числа или две матрицы, получены %s и %s", e->op, type_name(a->type), type_name(b->type)); return 0;
        }
        e->type = a->type;
        if (e->type == TYPE_NUMBER) return 1;
        if (e->op == '*') {
            if (a->cols != b->rows) {
                diagnostic(e->pos, "семантическая ошибка", "невозможно умножить матрицы размеров %zux%zu и %zux%zu: число столбцов первой матрицы должно совпадать с числом строк второй", a->rows, a->cols, b->rows, b->cols); return 0;
            }
            e->rows = a->rows; e->cols = b->cols;
        } else {
            if (a->rows != b->rows || a->cols != b->cols) {
                diagnostic(e->pos, "семантическая ошибка", "операция '%c' требует одинаковых размеров матриц, получены %zux%zu и %zux%zu", e->op, a->rows, a->cols, b->rows, b->cols); return 0;
            }
            e->rows = a->rows; e->cols = a->cols;
        }
        return 1;
    }
    case EX_CALL: {
        Expr *a;
        if (!builtin(e->text)) { diagnostic(e->pos, "семантическая ошибка", "неизвестная функция '%s'", e->text); return 0; }
        if (e->items->count != 1) { diagnostic(e->pos, "семантическая ошибка", "функция '%s' ожидает один аргумент, получено %zu", e->text, e->items->count); return 0; }
        a = e->items->head->item;
        if (!check_expr(a, depth + 1)) return 0;
        if (a->type != TYPE_MATRIX) { diagnostic(e->pos, "семантическая ошибка", "функция '%s' ожидает матрицу, получено число", e->text); return 0; }
        if ((!strcmp(e->text, "det") || !strcmp(e->text, "inverse")) && a->rows != a->cols) {
            diagnostic(e->pos, "семантическая ошибка", "функция '%s' требует квадратную матрицу, получена %zux%zu", e->text, a->rows, a->cols); return 0;
        }
        if (!strcmp(e->text, "det") || !strcmp(e->text, "rank")) e->type = TYPE_NUMBER;
        else {
            e->type = TYPE_MATRIX;
            e->rows = !strcmp(e->text, "transpose") ? a->cols : a->rows;
            e->cols = !strcmp(e->text, "transpose") ? a->rows : a->cols;
        }
        return 1;
    }
    }
    return 0;
}
int analyze(List *statements) {
    Link *link;
    symbols = NULL; symbol_count = 0;
    for (link = statements->head; link; link = link->next) {
        Stmt *stmt = link->item;
        Symbol *s = stmt->name ? find_symbol(stmt->name) : NULL;
        if (stmt->kind == ST_DECLARE) {
            if (s) { diagnostic(stmt->pos, "семантическая ошибка", "повторное объявление переменной '%s'", stmt->name); return 0; }
            if (builtin(stmt->name)) { diagnostic(stmt->pos, "семантическая ошибка", "имя '%s' зарезервировано для встроенной функции", stmt->name); return 0; }
        } else if (stmt->kind == ST_ASSIGN && !s) {
            diagnostic(stmt->pos, "семантическая ошибка", "переменная '%s' не объявлена", stmt->name); return 0;
        }
        if (!check_expr(stmt->value, 1)) return 0;
        if (stmt->kind == ST_PRINT) continue;
        if ((stmt->kind == ST_DECLARE ? stmt->declared : s->type) != stmt->value->type) {
            diagnostic(stmt->pos, "семантическая ошибка", "нельзя присвоить значение типа '%s' переменной '%s' типа '%s'", type_name(stmt->value->type), stmt->name, type_name(stmt->kind == ST_DECLARE ? stmt->declared : s->type)); return 0;
        }
        if (stmt->kind == ST_DECLARE) {
            s = arena_alloc(sizeof(*s)); s->name = stmt->name; s->type = stmt->declared;
            s->id = symbol_count++; s->next = symbols; symbols = s;
        }
        s->rows = stmt->value->rows; s->cols = stmt->value->cols; stmt->symbol = s->id;
    }
    return 1;
}
