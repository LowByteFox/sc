#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>

#include "sc.h"

#if HEAP_SIZE > UINT16_MAX
#error "Heap size cannot be more than 65535 (UINT16_MAX) bytes"
#endif

enum sc_node_types {
    AST_EXPR = 1,
    AST_IDENT,
    AST_NUM,
    AST_REAL,
    AST_BOOL,
    AST_STRING,
};

struct sc_ast_val {
    uint8_t type;
    uint16_t value; /* index of the value in the buffer/index of expression node in the heap */
};

struct sc_ast_expr {
    uint8_t type;
    uint16_t ident; /* index of the ident in the buffer */
    uint16_t arg_count; /* number of args the expression has */
};

struct sc_priv_fns {
    const char *name;
    sc_fn run;
};

struct sc_ast_ctx { uint16_t arena_index; union { uint16_t tok_index; uint16_t eval_offset; }; uint16_t tok_limit; };

static bool isspecial(char c);
static struct sc_rt_val eval_ast(struct sc_ctx *ctx);
static struct sc_rt_val get_val(struct sc_ctx *ctx, uint8_t type);
static void parse_expr(struct sc_ctx *ctx);
static void parse_val(struct sc_ctx *ctx);
static void append_tok(struct sc_ctx *ctx, uint16_t *len, uint16_t *sz, sc_tok tk);
static void append_loc(struct sc_ctx *ctx, uint16_t *len, uint16_t *sz, sc_loc loc);

/* helper fns */
static bool has_real(struct sc_rt_val *args, uint16_t nargs);

/* builtin routines */
static struct sc_rt_val plus(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs);
static struct sc_rt_val minus(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs);
static struct sc_rt_val mult(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs);
static struct sc_rt_val divide(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs);
static struct sc_rt_val len(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs);
static struct sc_rt_val list(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs);
static struct sc_rt_val cons(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs);
static struct sc_rt_val car(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs);
static struct sc_rt_val cdr(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs);

const char *sc_err = NULL;
static const char *buf = NULL;

static struct sc_priv_fns priv[] = {
    { "+", plus },
    { "-", minus },
    { "*", mult },
    { "/", divide },
    { "len", len },
    { "list", list },
    { "cons", cons },
    { "car", car },
    { "cdr", cdr },
    { NULL, NULL },
};

struct sc_rt_val sc_eval(struct sc_ctx *ctx, const char *buffer, uint16_t buflen) {
    if (ctx->heap) free(ctx->heap);
    if (ctx->tokens) free(ctx->tokens);
    if (ctx->locs) free(ctx->locs);

    buf = buffer;

    uint16_t toks_len, toks_size, locs_len, locs_size;
    toks_len = toks_size = locs_len = locs_size = 0;
    struct sc_rt_val nul = { 0 };

    for (uint16_t i = 0; i < buflen; i++, buffer++) {
        char c = *buffer;
        if (isspace(c))
            continue;

        if (isspecial(c)) {
            append_tok(ctx, &toks_len, &toks_size, c);
            append_loc(ctx, &locs_len, &locs_size, i);
            continue;
        }

        if (isdigit(c)) {
            bool parsed_float = false;
            append_tok(ctx, &toks_len, &toks_size, NUM);
            append_loc(ctx, &locs_len, &locs_size, i);
flt:
            do {
                i++; buffer++;
                c = *buffer;
            } while (isdigit(c) && !isspecial(c));
            if (c == '.' && !parsed_float && isdigit(buffer[1])) {
                ctx->tokens[toks_len - 1] = REAL;
                parsed_float = true;
                goto flt;
            }
            if (isspecial(c)) { i--; buffer--; }
        } else {
            if (c == '#') { /* possibly bool */
                if (buffer[1] == 't' || buffer[1] == 'f') {
                    i++; buffer++;
                    append_tok(ctx, &toks_len, &toks_size, BOOL);
                    append_loc(ctx, &locs_len, &locs_size, i);
                } else {
                    sc_err = "Exprected #t or #f!";
                    return nul;
                }
            } else if (c == '"') { /* possibly string */
                i++; buffer++;
                append_tok(ctx, &toks_len, &toks_size, STRING);
                append_loc(ctx, &locs_len, &locs_size, i); /* start of the string */
                do {
                    i++; buffer++;
                    c = *buffer;
                } while (c != '"');
            } else {
                append_tok(ctx, &toks_len, &toks_size, IDENT);
                append_loc(ctx, &locs_len, &locs_size, i);

                do {
                    i++; buffer++;
                    c = *buffer;
                } while (!isspace(c) && !isspecial(c));
                if (isspecial(c)) { i--; buffer--; }
            }
        }
    }
    append_tok(ctx, &toks_len, &toks_size, END);
    ctx->heap = calloc(HEAP_SIZE, sizeof(uint8_t));

    struct sc_ast_ctx ast_ctx = { 0 };
    ast_ctx.tok_limit = toks_len - 1;
    ctx->_ctx = &ast_ctx;

    if (ctx->tokens[0] != '(') {
        sc_err = "Expected '('!";
        return nul;
    }

    parse_expr(ctx);
    ast_ctx.eval_offset = 0;
    struct sc_rt_val res = eval_ast(ctx);
    ctx->_ctx = NULL;
    return res;
}

static bool isspecial(char c) {
    return c == '(' || c == ')';
}

static struct sc_rt_val eval_ast(struct sc_ctx *ctx) {
    struct sc_rt_val res = { 0 };
    struct sc_ast_expr *expr = (void*) (ctx->heap + ctx->_ctx->eval_offset);
    ctx->_ctx->eval_offset += sizeof(*expr);
    struct sc_rt_val *args = sc_alloc(ctx, expr->arg_count * sizeof(*args));

    for (uint16_t i = 0; i < expr->arg_count; i++) {
        uint8_t *type = (void*) (ctx->heap + ctx->_ctx->eval_offset);
        if (*type == AST_EXPR) args[i] = eval_ast(ctx);
        else args[i] = get_val(ctx, *type);
    }

    uint16_t len = 0;
    const char *it = buf + expr->ident;
    while (!isspace(*it) && !isspecial(*it)) { it++; len++; }

    it = buf + expr->ident;
    for (uint8_t i = 0; priv[i].name != NULL; i++) {
        if (strncmp(it, priv[i].name, len) == 0) {
            return priv[i].run(ctx, args, expr->arg_count);
        }
    }

    return res;
}
static struct sc_rt_val get_val(struct sc_ctx *ctx, uint8_t type)
{
    struct sc_rt_val res = { 0 };
    struct sc_ast_val *val = (void*) (ctx->heap + ctx->_ctx->eval_offset);
    ctx->_ctx->eval_offset += sizeof(*val);
    if (type == AST_NUM) {
        res.type = RT_NUM;
        res.number = strtol(buf + val->value, NULL, 10);
    } else if (type == AST_REAL) {
        res.type = RT_REAL;
        res.real = strtod(buf + val->value, NULL);
    } else if (type == AST_BOOL) {
        res.type = RT_BOOL;
        res.boolean = buf[val->value] == 't' ? true : false;
    } else if (type == AST_STRING) {
        size_t len = strcspn(buf + val->value, "\"");
        res.type = RT_STRING;
        res.str = sc_alloc(ctx, len + 1);
        memcpy(res.str, buf + val->value, len);
        res.str[len] = 0;
    }

    return res;
}

static void parse_expr(struct sc_ctx *ctx) {
    struct sc_ast_expr *expr = sc_alloc(ctx, sizeof(*expr));
    expr->type = AST_EXPR;
    ctx->_ctx->tok_index++; /* skip ( */
    if (ctx->tokens[ctx->_ctx->tok_index] != IDENT) {
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
        if (current == LPAREN) parse_expr(ctx);
        else parse_val(ctx);

        current = ctx->tokens[ctx->_ctx->tok_index];
    }
    expr->arg_count = arg_count;
    ctx->_ctx->tok_index++; /* skip ) */

    return;
}

static void parse_val(struct sc_ctx *ctx) {
    struct sc_ast_val *val = sc_alloc(ctx, sizeof(*val));
    sc_tok current = ctx->tokens[ctx->_ctx->tok_index];
    if (current == IDENT) val->type = AST_IDENT;
    else if (current == NUM) val->type = AST_NUM;
    else if (current == REAL) val->type = AST_REAL;
    else if (current == BOOL) val->type = AST_BOOL;
    else if (current == STRING) val->type = AST_STRING;
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

void *sc_alloc(struct sc_ctx *ctx, uint16_t size) {
    void *ptr = ctx->heap + ctx->_ctx->arena_index;
    ctx->_ctx->arena_index += size;
    return ptr;
}

/* helper fns */
static bool has_real(struct sc_rt_val *args, uint16_t nargs) {
    for (uint16_t i = 0; i < nargs; i++) { if (args[i].type == RT_REAL) return true; }
    return false;
}

/* builtin routines */
static struct sc_rt_val plus(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs) {
    struct sc_rt_val res = { 0 };
    bool real = has_real(args, nargs);
    res.type = real ? RT_REAL : RT_NUM;

    for (uint16_t i = 0; i < nargs; i++)
        if (real) res.real += sc_get_number(args[i]); else res.number += sc_get_number(args[i]);

    return res;
}

static struct sc_rt_val minus(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs) {
    struct sc_rt_val res = { 0 };
    bool real = has_real(args, nargs);
    res.type = real ? RT_REAL : RT_NUM;

    if (nargs == 0) return res;

    if (real) res.real = sc_get_number(args[0]); else res.number = sc_get_number(args[0]);

    for (uint16_t i = 1; i < nargs; i++)
        if (real) res.real -= sc_get_number(args[i]); else res.number -= sc_get_number(args[i]);

    return res;
}

static struct sc_rt_val mult(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs) {
    struct sc_rt_val res = { 0 };
    bool real = has_real(args, nargs);
    res.type = real ? RT_REAL : RT_NUM;

    if (nargs == 0) return res;

    if (real) res.real = sc_get_number(args[0]); else res.number = sc_get_number(args[0]);

    for (uint16_t i = 1; i < nargs; i++)
        if (real) res.real *= sc_get_number(args[i]); else res.number *= sc_get_number(args[i]);

    return res;
}

static struct sc_rt_val divide(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs) {
    struct sc_rt_val res = { 0 };
    bool real = has_real(args, nargs);
    res.type = real ? RT_REAL : RT_NUM;

    if (nargs == 0) return res;

    if (real) res.real = sc_get_number(args[0]); else res.number = sc_get_number(args[0]);

    for (uint16_t i = 1; i < nargs; i++)
        if (real) res.real /= sc_get_number(args[i]); else res.number /= sc_get_number(args[i]);

    return res;
}

static struct sc_rt_val len(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs) {
    struct sc_rt_val res = { 0 };
    if (nargs != 1) return res;
    if (args[0].type != RT_STRING) return res;
    res.type = RT_NUM;
    res.number = strlen(args[0].str);
    return res;
}

static struct sc_rt_val list(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs) {
    struct sc_rt_val res = { 0 };

    struct sc_rt_val *iter = &res;
    for (uint16_t i = 0; i < nargs; i++) {
        iter->type = RT_LIST;
        iter->list.current = args + i; /* there is no GC, can do this */
        iter->list.next = sc_alloc(ctx, sizeof(res));
        iter = iter->list.next;
    }

    return res;
}

static struct sc_rt_val cons(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs) {
    struct sc_rt_val res = { 0 };
    if (nargs != 2) return res;

    return list(ctx, args, nargs);
}

static struct sc_rt_val car(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs) {
    struct sc_rt_val res = { 0 };
    if (nargs != 1) return res;
    if (args[0].type != RT_LIST) return res;

    return *args[0].list.current;
}

static struct sc_rt_val cdr(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs) {
    struct sc_rt_val res = { 0 };
    if (nargs != 1) return res;
    if (args[0].type != RT_LIST) return res;

    return *args[0].list.next;
}
