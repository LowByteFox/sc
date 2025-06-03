#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>

#include "sc.h"
#include "sc_priv.h"
#include "config.h"

const char *sc_err = NULL;
static const char *buf = NULL;

static struct sc_fns priv[] = {
    { false, "+", plus },
    { false, "-", minus },
    { false, "*", mult },
    { false, "/", divide },
    { false, "=", eql, },
    { false, "<", lt, },
    { false, "<=", lte, },
    { false, ">", gt, },
    { false, ">=", gte, },
    { false, "and", and },
    { false, "or", or },
    { false, "not", not },
    { false, "car", car },
    { false, "cdr", cdr },
    { false, "cons", cons },
    { false, "list", list },
    { false, "length", len },
    /* { false, "append", append },
     * { false, "map", map },
     * { false, "filter", filter },
     * { false, "reverse", reverse },
     * { false, "fold", fold },
     * { false, "member", member },
     * { false, "take", take },
     * { false, "drop", drop },
     */
    { false, "string-length", len },
    /* { false, "string-ref", string_ref },
     * { false, "substring", substring },
     * { false, "string-upcase", upcase },
     * { false, "string-downcase", downcase },
     * { false, "string-titlecase", titlecase },
     * { false, "string-contains?", str_contains },
     */
    { true, "if", cond },
    { true, "cond", cond },
    { true, "define", define },
    { false, "begin", begin },
    { true, "lambda", lambda },
    { true, "λ", lambda },
    { true, "let", let },
    { false, "call", call },
    { false, "random", rnd },
    { false, "abs", sc_abs },
    { false, "sqrt", sc_sqrt },
    { false, "expt", sc_expt },
    { false, "mean", mean },
    /* { true, "while", while_loop },
     * { true, "until", until_loop },
     * { false, "display", display },
     * { false, "newline", newline },
     * { false, "read", read },
     * { false, "median", median },
     * { false, "max", sc_max },
     * { false, "min", sc_min },
     */
    { false, "eq?", eq },
    { false, "equal?", eq },
    { false, "error", error },
    { false, NULL, NULL },
};

sc_value sc_eval(struct sc_ctx *ctx, const char *buffer, uint16_t buflen) {
    if (ctx->heap) free(ctx->heap);
    if (ctx->tokens) free(ctx->tokens);
    if (ctx->locs) free(ctx->locs);

    buf = buffer;

    uint16_t toks_len, toks_size, locs_len, locs_size;
    toks_len = toks_size = locs_len = locs_size = 0;
    sc_value nul = { 0 };

    for (uint16_t i = 0; i < buflen; i++, buffer++) {
        char c = *buffer;
        if (isspace(c))
            continue;

        if (isspecial(c)) {
            append_tok(ctx, &toks_len, &toks_size, c);
            append_loc(ctx, &locs_len, &locs_size, i);
            continue;
        }

        if (isdigit(c) || c == '-') {
            bool parsed_float = false;
            append_tok(ctx, &toks_len, &toks_size, SC_NUM_TOK);
            append_loc(ctx, &locs_len, &locs_size, i);
flt:
            do {
                i++; buffer++;
                c = *buffer;
            } while (isdigit(c) && !isspecial(c));
            if (c == '.' && !parsed_float && isdigit(buffer[1])) {
                ctx->tokens[toks_len - 1] = SC_REAL_TOK;
                parsed_float = true;
                goto flt;
            }
            if (isspecial(c)) { i--; buffer--; }
        } else {
            if (c == '#') { /* possibly bool */
                if (buffer[1] == 't' || buffer[1] == 'f') {
                    i++; buffer++;
                    append_tok(ctx, &toks_len, &toks_size, SC_BOOL_TOK);
                    append_loc(ctx, &locs_len, &locs_size, i);
                } else {
                    sc_err = "Exprected #t or #f!";
                    return nul;
                }
            } else if (c == '"') { /* possibly string */
                i++; buffer++;
                append_tok(ctx, &toks_len, &toks_size, SC_STRING_TOK);
                append_loc(ctx, &locs_len, &locs_size, i); /* start of the string */
                do {
                    i++; buffer++;
                    c = *buffer;
                } while (c != '"');
            } else {
                append_tok(ctx, &toks_len, &toks_size, SC_IDENT_TOK);
                append_loc(ctx, &locs_len, &locs_size, i);

                do {
                    i++; buffer++;
                    c = *buffer;
                } while (!isspace(c) && !isspecial(c));
                if (isspecial(c)) { i--; buffer--; }
            }
        }
    }

    append_tok(ctx, &toks_len, &toks_size, SC_END_TOK);
    ctx->heap = calloc(HEAP_SIZE, sizeof(uint8_t));

    struct sc_ast_ctx ast_ctx = { 0 };
    struct sc_stack stack = { 0 };
    ast_ctx.tok_limit = toks_len - 1;
    ctx->_ctx = &ast_ctx;

    if (ctx->tokens[0] != '(') {
        sc_err = "Expected '('!";
        return nul;
    }

    ctx->_ctx->gc.memory_limit = HEAP_SIZE;
    parse_expr(ctx);
    ctx->_stack = &stack;
    ctx->_ctx->gc.memory_begin = ctx->_ctx->gc.arena_index;
    push_frame(ctx);
    ast_ctx.eval_offset = 0;
    sc_value res = eval_ast(ctx);
    ctx->_ctx = NULL;
    free(ctx->tokens);
    free(ctx->locs);
    return res;
}

static bool isspecial(char c) { return c == '(' || c == ')'; }

static sc_value eval_ast(struct sc_ctx *ctx) {
    struct sc_ast_expr *expr = (void*) (ctx->heap + ctx->_ctx->eval_offset);
    ctx->_ctx->eval_offset += sizeof(*expr);
    uint8_t fn_index = sizeof(priv) / sizeof(priv[0]) - 1;
    size_t len = strcspn(buf + expr->ident, " ()");;
    bool user_fn = false;

    char *it = alloc_ident(ctx, expr->ident);
    for (uint8_t i = 0; priv[i].name != NULL; i++) {
        if (strncmp(it, priv[i].name, len) == 0) {
            fn_index = i; break;
        }
    }
    struct sc_stack_kv *maybe = NULL;

    if (priv[fn_index].name == NULL) {
        for (uint8_t i = 0; ctx->user_fns[i].name != NULL; i++) {
            if (strncmp(it, ctx->user_fns[i].name, len) == 0) {
                fn_index = i; user_fn = true; break;
            }
        }
        if (ctx->user_fns[fn_index].name == NULL) {
            char buffer[len + 1]; memcpy(buffer, it, len); buffer[len] = 0;
            maybe = stack_find(ctx->_stack, buffer);
            if (maybe == NULL)
                return (sc_value) {0};
        }
    }
    sc_free(ctx, it);
    sc_value args[expr->arg_count];
    memset(args, 0, sizeof(sc_value) * expr->arg_count);

    if (priv[fn_index].lazy) {
        for (uint16_t i = 0; i < expr->arg_count; i++) {
            args[i].type = SC_LAZY_EXPR_VAL;
            args[i].lazy_addr = ctx->_ctx->eval_offset;

            uint8_t *type = (void*) (ctx->heap + ctx->_ctx->eval_offset);
            if (*type == SC_AST_EXPR) {
                struct sc_ast_expr *_expr = (void*) (ctx->heap + ctx->_ctx->eval_offset);
                ctx->_ctx->eval_offset += _expr->jump_by;
            } else ctx->_ctx->eval_offset += sizeof(struct sc_ast_val);
        }
    } else {
        for (uint16_t i = 0; i < expr->arg_count; i++) {
            uint8_t *type = (void*) (ctx->heap + ctx->_ctx->eval_offset);
            if (*type == SC_AST_EXPR) args[i] = eval_ast(ctx);
            else args[i] = get_val(ctx, *type);
            if (args[i].type == SC_ERROR_VAL) {
                free_args(ctx, args, expr->arg_count); return args[i];
            }
        }
    }
    sc_value res = { 0 };
    if (maybe != NULL)
        res = eval_lambda(ctx, &maybe->value, args, expr->arg_count);
    else if (!user_fn)
        res = priv[fn_index].run(ctx, args, expr->arg_count);
    else
        res = ctx->user_fns[fn_index].run(ctx, args, expr->arg_count);
    free_args(ctx, args, expr->arg_count);
    return res;
}

static sc_value get_val(struct sc_ctx *ctx, uint8_t type) {
    sc_value res = { 0 };
    struct sc_ast_val *val = (void*) (ctx->heap + ctx->_ctx->eval_offset);
    ctx->_ctx->eval_offset += sizeof(*val);
    if (type == SC_AST_NUM) return sc_num(strtol(buf + val->value, NULL, 10));
    else if (type == SC_AST_REAL) return sc_real(strtod(buf + val->value, NULL));
    else if (type == SC_AST_BOOL) return sc_bool(buf[val->value] == 't' ? true : false);
    else if (type == SC_AST_STRING) {
        size_t len = strcspn(buf + val->value, "\"");
        res.type = SC_STRING_VAL;
        res.str = sc_alloc(ctx, len + 1);
        memcpy(res.str, buf + val->value, len);
        res.str[len] = 0;
    } else if (type == SC_AST_IDENT) {
        size_t len = strcspn(buf + val->value, " ()");
        char buffer[len + 1]; memcpy(buffer, buf + val->value, len); buffer[len] = 0;
        struct sc_stack_kv *maybe = stack_find(ctx->_stack, buffer);
        if (maybe != NULL)
            res = maybe->value;
    }

    return res;
}

static sc_value eval_lambda(struct sc_ctx *ctx, sc_value *lambda, sc_value *args, uint16_t nargs) {
    if (lambda->lambda.arg_count != nargs) return (sc_value) {0};
    push_frame(ctx);
    struct sc_ast_expr *l_args = (void*) ctx->heap + lambda->lambda.args;

    if (lambda->lambda.arg_count > 0) {
        struct sc_stack_kv *val = frame_add(ctx);
        val->ident = alloc_ident(ctx, l_args->ident);
        val->value = args[0];
        uint8_t *type = ctx->heap + lambda->lambda.args + sizeof(*l_args);

        for (uint16_t i = 1; i < lambda->lambda.arg_count; i++) {
            struct sc_ast_val *v = (void*) type;
            val = frame_add(ctx);
            val->ident = alloc_ident(ctx, v->value);
            val->value = args[i];
            type += sizeof(*v);
        }
    }

    sc_value res = eval_at(ctx, lambda->lambda.body);

    pop_frame(ctx, ctx->_stack);
    return res;
}

static void parse_expr(struct sc_ctx *ctx) {
    uint16_t start = ctx->_ctx->gc.arena_index;
    struct sc_ast_expr *expr = sc_alloc(ctx, sizeof(*expr));
    expr->type = SC_AST_EXPR;
    ctx->_ctx->tok_index++; /* skip ( */
    if (ctx->tokens[ctx->_ctx->tok_index] != SC_IDENT_TOK) {
        sc_err = "Expected identifier!";
        return;
    }

    expr->ident = ctx->locs[ctx->_ctx->tok_index++];
    sc_tok current = ctx->tokens[ctx->_ctx->tok_index];
    uint16_t arg_count = 0;

    while (current != ')') {
        if (ctx->_ctx->tok_index >= ctx->_ctx->tok_limit) {
            sc_err = "Expected )";
            return;
        }

        arg_count++;
        if (current == SC_LPAREN_TOK) parse_expr(ctx);
        else parse_val(ctx);

        current = ctx->tokens[ctx->_ctx->tok_index];
    }
    expr->arg_count = arg_count;
    expr->jump_by = ctx->_ctx->gc.arena_index - start;
    ctx->_ctx->tok_index++; /* skip ) */

    return;
}

static void parse_val(struct sc_ctx *ctx) {
    struct sc_ast_val *val = sc_alloc(ctx, sizeof(*val));
    sc_tok current = ctx->tokens[ctx->_ctx->tok_index];
    if (current == SC_IDENT_TOK) val->type = SC_AST_IDENT;
    else if (current == SC_NUM_TOK) val->type = SC_AST_NUM;
    else if (current == SC_REAL_TOK) val->type = SC_AST_REAL;
    else if (current == SC_BOOL_TOK) val->type = SC_AST_BOOL;
    else if (current == SC_STRING_TOK) val->type = SC_AST_STRING;
    val->value = ctx->locs[ctx->_ctx->tok_index];
    ctx->_ctx->tok_index++; /* skip over */
}

static void append_tok(struct sc_ctx *ctx, uint16_t *len, uint16_t *sz, sc_tok tk) {
    if (*len * sizeof(tk) == *sz)
        ctx->tokens = realloc(ctx->tokens, (*sz += ARR_GROW * sizeof(tk)));
    ctx->tokens[(*len)++] = tk;
}

static void append_loc(struct sc_ctx *ctx, uint16_t *len, uint16_t *sz, sc_loc loc) {
    if (*len * sizeof(loc) == *sz)
        ctx->locs = realloc(ctx->locs, (*sz += ARR_GROW * sizeof(loc)));
    ctx->locs[(*len)++] = loc;
}

static char *get_ident(struct sc_ctx *ctx, struct sc_ast_val *val) {
    const char *ident_s = buf + val->value;
    size_t len = strcspn(ident_s, " ()");
    char *ident = sc_alloc(ctx, len + 1);
    memcpy(ident, ident_s, len);
    ident[len] = 0;
    return ident;
}

static void push_frame(struct sc_ctx *ctx) {
    struct sc_stack_node *n = sc_alloc(ctx, sizeof(*n));

    if (ctx->_stack->head == NULL) {
        ctx->_stack->head = n;
        ctx->_stack->tail = n;
    } else {
        ctx->_stack->tail->next_frame = n;
        ctx->_stack->tail = n;
    }
}

static void pop_frame(struct sc_ctx *ctx, struct sc_stack *stack) {
    struct sc_stack_node *iter = stack->head;
    while (iter->next_frame != NULL && iter->next_frame->next_frame != NULL)
        iter = iter->next_frame;
    struct sc_stack_kv *i = iter->next_frame->first_value;
    while (i != NULL) {
        struct sc_stack_kv *prev = i;
        sc_free(ctx, i->ident);
        i = i->next;
        sc_free(ctx, prev);
    }
    sc_free(ctx, iter->next_frame);
    iter->next_frame = NULL;
    stack->tail = iter;
}

static struct sc_stack_kv *stack_node_find(struct sc_stack_node *node, const char *ident) {
    struct sc_stack_kv *res = NULL;
    if (node->next_frame) {
        res = stack_node_find(node->next_frame, ident);
        if (res != NULL)
            return res;
    }

    struct sc_stack_kv *iter = node->first_value;
    while (iter != NULL) {
        if (strcmp(iter->ident, ident) == 0) return iter;
        iter = iter->next;
    }
    return res;
}

static struct sc_stack_kv *stack_find(struct sc_stack *stack, const char *ident) {
    return stack_node_find(stack->head, ident);
}

static struct sc_stack_kv *global_add(struct sc_ctx *ctx) {
    struct sc_stack_kv *kv = sc_alloc(ctx, sizeof(*kv));
    if (ctx->_stack->head->first_value == NULL) {
        ctx->_stack->head->first_value = kv;
        ctx->_stack->head->last_value = kv;
    } else {
        ctx->_stack->head->last_value->next = kv;
        ctx->_stack->head->last_value = kv;
    }
    return kv;
}

static struct sc_stack_kv *frame_add(struct sc_ctx *ctx) {
    struct sc_stack_kv *kv = sc_alloc(ctx, sizeof(*kv));
    if (ctx->_stack->tail->first_value == NULL) {
        ctx->_stack->tail->first_value = kv;
        ctx->_stack->tail->last_value = kv;
    } else {
        ctx->_stack->tail->last_value->next = kv;
        ctx->_stack->tail->last_value = kv;
    }
    return kv;
}

void *sc_alloc(struct sc_ctx *ctx, uint16_t size) {
    if ((int) ctx->_ctx->gc.arena_index + size >= ctx->_ctx->gc.memory_limit) {
        fprintf(stderr, "sc: heap exhausted!\n");
        abort();
    }

    /* will run when eval */
    if (ctx->_ctx->gc.memory_begin != 0) {
        uint16_t *free_list = (void*) ctx->heap + ctx->_ctx->gc.memory_limit;
        while (((uintptr_t) free_list) < ((uintptr_t) ctx->heap + HEAP_SIZE)) {
            if (*free_list > 0) {
                struct sc_gc_obj *fnd = (void*) ctx->heap + (*free_list);
                if (fnd->size >= size) {
                    memset(fnd->data, 0, fnd->size); fnd->count++; *free_list = 0;
                    return fnd->data;
                }
            }
            free_list++;
        }
        struct sc_gc_obj *ptr = (void*) ctx->heap + ctx->_ctx->gc.arena_index;
        ptr->size = size;
        ptr->count = 1;
        ctx->_ctx->gc.arena_index += size + sizeof(struct sc_gc_obj);
        ctx->_ctx->gc.memory_limit -= sizeof(uint16_t);
        return ptr->data;
    }

    void *ptr = (void*) ctx->heap + ctx->_ctx->gc.arena_index;
    ctx->_ctx->gc.arena_index += size;
    ctx->_ctx->gc.memory_limit -= sizeof(uint16_t);

    return ptr;
}

void sc_free(struct sc_ctx *ctx, void *ptr) {
    struct sc_gc_obj *obj = (void*)((uint8_t*) ptr) - sizeof(*obj);
    if (obj->count > 0) obj->count--;
    if (obj->count == 0) {
        uintptr_t address = ((uintptr_t) obj) - ((uintptr_t) ctx->heap);
        uint16_t *free_list = (void*) ctx->heap + ctx->_ctx->gc.memory_limit;
        while (((uintptr_t) free_list) < ((uintptr_t) ctx->heap + HEAP_SIZE)) {
            if (*free_list == 0) {
                *free_list = (uint16_t) address;
                break;
            }
            free_list++;
        }
    }
}

void sc_dup(void *ptr) {
    struct sc_gc_obj *obj = (void*)((uint8_t*) ptr) - sizeof(*obj);
    obj->count++;
}

/* helper fns */
static void free_val(struct sc_ctx *ctx, sc_value val) {
    if (val.type == SC_STRING_VAL) sc_free(ctx, val.str);
    else if (val.type == SC_LIST_VAL) {
        free_val(ctx, *val.list.current);
        sc_free(ctx, val.list.current);
        free_val(ctx, *val.list.next);
        sc_free(ctx, val.list.next);
    }
}

static void free_args(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    for (uint16_t i = 0; i < nargs; i++) free_val(ctx, args[i]);
}

static sc_value eval_at(struct sc_ctx *ctx, uint16_t addr) {
    uint16_t old = ctx->_ctx->eval_offset;
    sc_value res = { 0 };
    ctx->_ctx->eval_offset = addr;
    if (ctx->heap[ctx->_ctx->eval_offset] != SC_AST_EXPR)
        res = get_val(ctx, ctx->heap[ctx->_ctx->eval_offset]);
    else
        res = eval_ast(ctx);
    ctx->_ctx->eval_offset = old;
    return res;
}

static bool has_real(sc_value *args, uint16_t nargs) {
    for (uint16_t i = 0; i < nargs; i++) { if (args[i].type == SC_REAL_VAL) return true; }
    return false;
}

static char *alloc_ident(struct sc_ctx *ctx, uint16_t addr) {
    size_t len = strcspn(buf + addr, " ()");
    char *buffer = sc_alloc(ctx, len + 1);
    memcpy(buffer, buf + addr, len);
    buffer[len] = 0;
    return buffer;
}

static sc_value dup_val(sc_value val) {
    if (val.type == SC_STRING_VAL) sc_dup(val.str);
    else if (val.type == SC_LIST_VAL) {
        sc_dup(val.list.current);
        *val.list.current = dup_val(*val.list.current);
        sc_dup(val.list.next);
        *val.list.next = dup_val(*val.list.next);
    }
    return val;
}

/* builtin routines */
static sc_value plus(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    sc_value res = { 0 };
    bool real = has_real(args, nargs);
    res.type = real ? SC_REAL_VAL : SC_NUM_VAL;

    for (uint16_t i = 0; i < nargs; i++)
        if (real) res.real += sc_get_number(args[i]); else res.number += sc_get_number(args[i]);

    return res;
}

#define gen_math_fns(name, op) static sc_value name(struct sc_ctx *ctx,\
    sc_value *args, uint16_t nargs) {\
    sc_value res = { 0 }; bool real = has_real(args, nargs);\
    res.type = real ? SC_REAL_VAL : SC_NUM_VAL;\
    if (nargs == 0) return res;\
    if (real) res.real = sc_get_number(args[0]); else res.number = sc_get_number(args[0]);\
    for (uint16_t i = 1; i < nargs; i++)\
        if (real) res.real op sc_get_number(args[i]); else res.number op sc_get_number(args[i]);\
    return res;\
}

#define gen_comp_fns(name, op) static sc_value name(struct sc_ctx *ctx,\
    sc_value *args, uint16_t nargs) {\
    if (nargs == 0) return sc_bool(true);\
    for (uint16_t i = 0; i < nargs - 1; i++)\
        if (!(sc_get_number(args[i]) op sc_get_number(args[i + 1])))\
             return sc_bool(false);\
    return sc_bool(true);\
}

#define gen_log_fns(name, op) static sc_value name(struct sc_ctx *ctx,\
    sc_value *args, uint16_t nargs) {\
    if (nargs == 0) return sc_bool(true);\
    for (uint16_t i = 0; i < nargs - 1; i++) {\
        if (args[i].type != SC_BOOL_VAL) return sc_bool(false);\
        if (!(args[i].boolean op args[i + 1].boolean))\
             return sc_bool(false);}\
    return sc_bool(true);\
}

static sc_value len(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs != 1) return sc_error("len: incorrect amount of arguments!");
    else if (args[0].type == SC_STRING_VAL) return sc_num(strlen(args[0].str));
    else if (args[0].type == SC_LIST_VAL) {
        int64_t len = 0;
        sc_value *iter = args + 0;
        while (iter != NULL && iter->type != SC_NOTHING_VAL) { len++; iter = iter->list.next; }
        return sc_num(len);
    }
    return sc_nil;
}

static sc_value list(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    sc_value res = { 0 };

    sc_value *iter = &res;
    for (uint16_t i = 0; i < nargs; i++) {
        iter->type = SC_LIST_VAL;
        iter->list.current = sc_alloc(ctx, sizeof(res));
        *iter->list.current = dup_val(args[i]);
        iter->list.next = sc_alloc(ctx, sizeof(res));
        iter = iter->list.next;
    }

    return res;
}

static sc_value cons(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs != 2) return sc_error("cons: incorrect amount of arguments!");;
    return list(ctx, args, nargs);
}

static sc_value car(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs != 1) return sc_error("car: incorrect amount of arguments!");;
    if (args[0].type != SC_LIST_VAL) return sc_error("car: expected a list!");

    return dup_val(*args[0].list.current);
}

static sc_value cdr(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs != 1) return sc_error("cdr: incorrect amount of arguments!");;
    if (args[0].type != SC_LIST_VAL) return sc_error("cdr: expected a list!");

    return dup_val(*args[0].list.next);
}

static sc_value begin(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) { return dup_val(args[nargs - 1]); }

static bool val_eql(sc_value a, sc_value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
    case SC_NUM_VAL:
        return a.number == b.number;
    case SC_REAL_VAL:
        return a.real == b.real;
    case SC_BOOL_TOK:
        return a.boolean == b.boolean;
    case SC_STRING_TOK:
        return strcmp(a.str, b.str) == 0;
    case SC_LIST_VAL:
        {
            sc_value *iter_a = &a; sc_value *iter_b = &b;
            while (iter_a->list.current != NULL && iter_b->list.current != NULL) {
                if (!val_eql(*iter_a->list.current, *iter_b->list.current))
                    return false;
                iter_a = iter_a->list.next; iter_b = iter_b->list.next;
            }
            if (iter_a->list.current == NULL && iter_b->list.current == NULL) return true;
        }
    }
    return false;
}

static sc_value eq(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs == 0) return sc_bool(true);
    for (uint16_t i = 0; i < nargs - 1; i++)
        if (!val_eql(args[i], args[i + 1]))
            return sc_bool(false);
    return sc_bool(true);
}

static sc_value define(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs != 2) return sc_error("define: incorrect amount of arguments!");
    char *ident = get_ident(ctx, (void*) ctx->heap + args[0].lazy_addr);
    struct sc_stack_kv *kv = global_add(ctx);
    kv->ident = ident;
    kv->value = eval_at(ctx, args[1].lazy_addr);
    return sc_bool(true);
}

static sc_value let(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs != 2) return sc_error("let: incorrect amount of arguments!");
    char *ident = get_ident(ctx, (void*) ctx->heap + args[0].lazy_addr);
    struct sc_stack_kv *maybe = stack_find(ctx->_stack, ident);
    if (maybe != NULL) {
        sc_free(ctx, ident);
        maybe->value = eval_at(ctx, args[1].lazy_addr);
        return sc_bool(true);
    }
    struct sc_stack_kv *kv = frame_add(ctx);
    kv->ident = ident;
    kv->value = eval_at(ctx, args[1].lazy_addr);
    return sc_bool(true);
}

static sc_value lambda(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    sc_value res = { 0 };
    if (nargs != 2) return res;
    res.type = SC_LAMBDA_VAL;
    struct sc_ast_expr *l_args = (void*) ctx->heap + args[0].lazy_addr;
    res.lambda.arg_count = l_args->arg_count + 1;
    res.lambda.args = args[0].lazy_addr;
    res.lambda.body = args[1].lazy_addr;
    return res;
}

static sc_value cond(struct sc_ctx *ctx, sc_value *args, uint16_t nargs)
{
    uint16_t i = 0;
    if (nargs < 2) return sc_nil;
    for (; i < nargs; i += 2) {
        sc_value cond = eval_at(ctx, args[i].lazy_addr);
        if (cond.type != SC_BOOL_VAL) goto skip;
        if (cond.boolean == true) return eval_at(ctx, args[i + 1].lazy_addr);
        continue;
skip:
        while(i + 2 < nargs) { i += 2; }
    }
    if (i - nargs == 1) /* else */
        return eval_at(ctx, args[nargs - 1].lazy_addr);
    return sc_nil;
}

static sc_value call(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs < 2) return sc_error("call: incorrect amount of arguments!");
    if (args[0].type != SC_LAMBDA_VAL) return sc_error("call: expected first argument to be a function!");
    return eval_lambda(ctx, (args + 0), (args + 1), nargs - 1);
}

static sc_value not(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs != 1) return sc_bool(false);
    else if (args[0].type != SC_BOOL_VAL) return sc_bool(false);
    return sc_bool(!args[0].boolean);
}

static sc_value rnd(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs == 1 && args[0].type == SC_NUM_VAL)
        return sc_num(random() % args[0].number);
    else if (nargs == 1 && args[0].type == SC_REAL_VAL)
        return sc_real(((float) random()/(float) RAND_MAX) * args[0].real);
    return sc_num(random() % UINT64_MAX);
}

static sc_value sc_abs(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs != 1) return sc_error("abs: incorrect amount of arguments!");
    double res = fabs(sc_get_number(args[0]));
    double dec = modf(res, &res);
    return dec == 0.0 ? sc_num((uint64_t) res) : sc_real(res + dec);
}

static sc_value sc_sqrt(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs != 1) return sc_error("sqrt: incorrect amount of arguments!");
    double res = sqrt(sc_get_number(args[0]));
    double dec = modf(res, &res);
    return dec == 0.0 ? sc_num((uint64_t) res) : sc_real(res + dec);
}

static sc_value sc_expt(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs != 2) return sc_error("expt: incorrect amount of arguments");
    double res = pow(sc_get_number(args[0]), sc_get_number(args[1]));
    double dec = modf(res, &res);
    return dec == 0.0 ? sc_num((uint64_t) res) : sc_real(res + dec);
}

static sc_value mean(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs == 0) return sc_error("mean: incorrect amount of arguments!");
    double res = sc_get_number(plus(ctx, args, nargs)) / (double) nargs;
    double dec = modf(res, &res);
    return dec == 0.0 ? sc_num((uint64_t) res) : sc_real(res + dec);
}

static sc_value error(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    if (nargs != 1) return sc_error("error: incorrect amount of arguments!");
    return sc_error(dup_val(args[0]).str);
}

/* generated functions */
gen_math_fns(minus, -=);
gen_math_fns(mult, *=);
gen_math_fns(divide, /=);
gen_comp_fns(eql, ==);
gen_comp_fns(lt, <);
gen_comp_fns(lte, <=);
gen_comp_fns(gt, >);
gen_comp_fns(gte, >=);
gen_log_fns(and, &&);
gen_log_fns(or, ||);
