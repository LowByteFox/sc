#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>

#include "sx.h"

#if HEAP_SIZE > UINT16_MAX
#error "Heap size cannot be more than 65535 (UINT16_MAX) bytes"
#endif

enum sx_node_types {
    AST_EXPR = 1,
    AST_IDENT,
    AST_NUM,
    AST_REAL,
    AST_BOOL,
    AST_STRING,
};

struct sx_ast_val {
    uint8_t type;
    uint16_t value; /* index of the value in the buffer/index of expression node in the heap */
};

struct sx_ast_expr {
    uint8_t type;
    uint16_t ident; /* index of the ident in the buffer */
    uint16_t arg_count; /* number of args the expression has */
};

struct sx_priv_fns {
    const char *name;
    sx_fn run;
};

struct sx_ast_ctx { uint16_t arena_index; union { uint16_t tok_index; uint16_t eval_offset; }; uint16_t tok_limit; };

static bool isspecial(char c);
static struct sx_rt_val eval_ast(struct sx_ctx *ctx);
static struct sx_rt_val get_val(struct sx_ctx *ctx, uint8_t type);
static void parse_expr(struct sx_ctx *ctx);
static void parse_val(struct sx_ctx *ctx);
static struct sx_rt_val *alloc_args(struct sx_ctx *ctx, uint16_t arg_count);
static void append_tok(struct sx_ctx *ctx, uint16_t *len, uint16_t *sz, sx_tok tk);
static void append_loc(struct sx_ctx *ctx, uint16_t *len, uint16_t *sz, sx_loc loc);

/* helper fns */
static bool has_real(struct sx_rt_val *args, uint16_t nargs);

/* builtin routines */
static struct sx_rt_val plus(struct sx_rt_val *args, uint16_t nargs);
static struct sx_rt_val minus(struct sx_rt_val *args, uint16_t nargs);
static struct sx_rt_val mult(struct sx_rt_val *args, uint16_t nargs);
static struct sx_rt_val divide(struct sx_rt_val *args, uint16_t nargs);
static struct sx_rt_val len(struct sx_rt_val *args, uint16_t nargs);

const char *sx_err = NULL;
static const char *buf = NULL;

static struct sx_priv_fns priv[] = {
    { "+", plus },
    { "-", minus },
    { "*", mult },
    { "/", divide },
    { "len", len },
    { NULL, NULL },
};

struct sx_rt_val sx_eval(struct sx_ctx *ctx, const char *buffer, uint16_t buflen) {
    if (ctx->heap) free(ctx->heap);
    if (ctx->tokens) free(ctx->tokens);
    if (ctx->locs) free(ctx->locs);

    buf = buffer;

    uint16_t toks_len, toks_size, locs_len, locs_size;
    toks_len = toks_size = locs_len = locs_size = 0;
    struct sx_rt_val nul = { 0 };

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
                    sx_err = "Exprected #t or #f!";
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

    struct sx_ast_ctx ast_ctx = { 0 };
    ast_ctx.tok_limit = toks_len - 1;
    ctx->_ctx = &ast_ctx;

    if (ctx->tokens[0] != '(') {
        sx_err = "Expected '('!";
        return nul;
    }

    parse_expr(ctx);
    ast_ctx.eval_offset = 0;
    struct sx_rt_val res = eval_ast(ctx);
    ctx->_ctx = NULL;
    return res;
}

static bool isspecial(char c) {
    return c == '(' || c == ')';
}

static struct sx_rt_val eval_ast(struct sx_ctx *ctx) {
    struct sx_rt_val res = { 0 };
    struct sx_ast_expr *expr = (void*) (ctx->heap + ctx->_ctx->eval_offset);
    ctx->_ctx->eval_offset += sizeof(*expr);
    struct sx_rt_val *args = alloc_args(ctx, expr->arg_count);

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
            return priv[i].run(args, expr->arg_count);
        }
    }

    return res;
}
static struct sx_rt_val get_val(struct sx_ctx *ctx, uint8_t type)
{
    struct sx_rt_val res = { 0 };
    struct sx_ast_val *val = (void*) (ctx->heap + ctx->_ctx->eval_offset);
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
        res.str = sx_alloc(ctx, len + 1);
        memcpy(res.str, buf + val->value, len);
        res.str[len] = 0;
    }

    return res;
}

static void parse_expr(struct sx_ctx *ctx) {
    struct sx_ast_expr *expr = sx_alloc(ctx, sizeof(*expr));
    expr->type = AST_EXPR;
    ctx->_ctx->tok_index++; /* skip ( */
    if (ctx->tokens[ctx->_ctx->tok_index] != IDENT) {
        sx_err = "Expected identifier!";
        return;
    }

    expr->ident = ctx->locs[ctx->_ctx->tok_index++];
    sx_tok current = ctx->tokens[ctx->_ctx->tok_index];
    uint16_t arg_count = 0;

    while (current != ')') {
        if (ctx->_ctx->tok_index >= ctx->_ctx->tok_limit) {
            sx_err = "Expected )";
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

static void parse_val(struct sx_ctx *ctx) {
    struct sx_ast_val *val = sx_alloc(ctx, sizeof(*val));
    sx_tok current = ctx->tokens[ctx->_ctx->tok_index];
    if (current == IDENT) val->type = AST_IDENT;
    else if (current == NUM) val->type = AST_NUM;
    else if (current == REAL) val->type = AST_REAL;
    else if (current == BOOL) val->type = AST_BOOL;
    else if (current == STRING) val->type = AST_STRING;
    val->value = ctx->locs[ctx->_ctx->tok_index];
    ctx->_ctx->tok_index++; /* skip over */
}

static struct sx_rt_val *alloc_args(struct sx_ctx *ctx, uint16_t arg_count) {
    struct sx_rt_val *ptr = (void*) (ctx->heap + ctx->_ctx->arena_index);
    ctx->_ctx->arena_index += sizeof(*ptr) * arg_count;
    return ptr;
}

static void append_tok(struct sx_ctx *ctx, uint16_t *len, uint16_t *sz, sx_tok tk) {
    if (*len * sizeof(tk) == *sz)
        ctx->tokens = realloc(ctx->tokens, (*sz += ARR_GROW * sizeof(tk)));
    ctx->tokens[(*len)++] = tk;
}

static void append_loc(struct sx_ctx *ctx, uint16_t *len, uint16_t *sz, sx_loc loc) {
    if (*len * sizeof(loc) == *sz)
        ctx->locs = realloc(ctx->locs, (*sz += ARR_GROW * sizeof(loc)));
    ctx->locs[(*len)++] = loc;
}

void *sx_alloc(struct sx_ctx *ctx, uint16_t size)
{
    void *ptr = ctx->heap + ctx->_ctx->arena_index;
    ctx->_ctx->arena_index += size;
    return ptr;
}

/* helper fns */
static bool has_real(struct sx_rt_val *args, uint16_t nargs)
{
    for (uint16_t i = 0; i < nargs; i++) { if (args[i].type == RT_REAL) return true; }
    return false;
}

/* builtin routines */
static struct sx_rt_val plus(struct sx_rt_val *args, uint16_t nargs) {
    struct sx_rt_val res = { 0 };
    bool real = has_real(args, nargs);
    res.type = real ? RT_REAL : RT_NUM;

    for (uint16_t i = 0; i < nargs; i++)
        if (real) res.real += sx_get_number(args[i]); else res.number += sx_get_number(args[i]);

    return res;
}

static struct sx_rt_val minus(struct sx_rt_val *args, uint16_t nargs)
{
    struct sx_rt_val res = { 0 };
    bool real = has_real(args, nargs);
    res.type = real ? RT_REAL : RT_NUM;

    if (nargs == 0) return res;

    if (real) res.real = sx_get_number(args[0]); else res.number = sx_get_number(args[0]);

    for (uint16_t i = 1; i < nargs; i++)
        if (real) res.real -= sx_get_number(args[i]); else res.number -= sx_get_number(args[i]);

    return res;
}

static struct sx_rt_val mult(struct sx_rt_val *args, uint16_t nargs)
{
    struct sx_rt_val res = { 0 };
    bool real = has_real(args, nargs);
    res.type = real ? RT_REAL : RT_NUM;

    if (nargs == 0) return res;

    if (real) res.real = sx_get_number(args[0]); else res.number = sx_get_number(args[0]);

    for (uint16_t i = 1; i < nargs; i++)
        if (real) res.real *= sx_get_number(args[i]); else res.number *= sx_get_number(args[i]);

    return res;
}

static struct sx_rt_val divide(struct sx_rt_val *args, uint16_t nargs)
{
    struct sx_rt_val res = { 0 };
    bool real = has_real(args, nargs);
    res.type = real ? RT_REAL : RT_NUM;

    if (nargs == 0) return res;

    if (real) res.real = sx_get_number(args[0]); else res.number = sx_get_number(args[0]);

    for (uint16_t i = 1; i < nargs; i++)
        if (real) res.real /= sx_get_number(args[i]); else res.number /= sx_get_number(args[i]);

    return res;
}

static struct sx_rt_val len(struct sx_rt_val *args, uint16_t nargs)
{
    struct sx_rt_val res = { 0 };
    if (nargs != 1) return res;
    if (args[0].type != RT_STRING) return res;
    res.type = RT_NUM;
    res.number = strlen(args[0].str);
    return res;
}
