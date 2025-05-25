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

struct ast_ctx { uint16_t arena_index; union { uint16_t tok_index; uint16_t eval_offset; }; uint16_t tok_limit; };

static bool isspecial(char c);
static struct sx_rt_val eval_ast(struct sx_ctx *ctx, struct ast_ctx *actx);
static struct sx_rt_val get_num(struct sx_ctx *ctx, struct ast_ctx *actx);
static void parse_expr(struct sx_ctx *ctx, struct ast_ctx *actx);
static void parse_val(struct sx_ctx *ctx, struct ast_ctx *actx);
static struct sx_rt_val *alloc_args(struct sx_ctx *ctx, struct ast_ctx *actx, uint16_t arg_count);
static struct sx_ast_val *alloc_val(struct sx_ctx *ctx, struct ast_ctx *actx);
static struct sx_ast_expr *alloc_expr(struct sx_ctx *ctx, struct ast_ctx *actx);
static void append_tok(struct sx_ctx *ctx, uint16_t *len, uint16_t *sz, sx_tok tk);
static void append_loc(struct sx_ctx *ctx, uint16_t *len, uint16_t *sz, sx_loc loc);

/* builtin routines */
static struct sx_rt_val plus(struct sx_rt_val *args, uint16_t nargs);

const char *sx_err = NULL;
static const char *buf = NULL;

static struct sx_priv_fns priv[] = {
    { "+", plus },
    { NULL, NULL },
};

struct sx_rt_val sx_eval(struct sx_ctx *ctx, const char *buffer, uint16_t buflen) {
    if (ctx->heap) free(ctx->heap);
    if (ctx->tokens) free(ctx->tokens);
    if (ctx->locs) free(ctx->locs);

    buf = buffer;

    uint16_t toks_len, toks_size, locs_len, locs_size;
    toks_len = toks_size = locs_len = locs_size = 0;

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
            append_tok(ctx, &toks_len, &toks_size, NUM);
            append_loc(ctx, &locs_len, &locs_size, i);
            do {
                i++;
                buffer++;
                c = *buffer;
            } while (isdigit(c) && !isspecial(c));
            if (isspecial(c)) { i--; buffer--; }
        } else {
            append_tok(ctx, &toks_len, &toks_size, IDENT);
            append_loc(ctx, &locs_len, &locs_size, i);

            do {
                i++;
                buffer++;
                c = *buffer;
            } while (!isspace(c) && !isspecial(c));
            if (isspecial(c)) { i--; buffer--; }
        }
    }
    append_tok(ctx, &toks_len, &toks_size, END);
    ctx->heap = calloc(HEAP_SIZE, sizeof(uint8_t));

    struct ast_ctx ast_ctx = { 0 };
    ast_ctx.tok_limit = toks_len - 1;

    if (ctx->tokens[0] != '(') {
        sx_err = "Expected '('!";
        struct sx_rt_val nul = { 0 };
        return nul;
    }

    parse_expr(ctx, &ast_ctx);
    ast_ctx.eval_offset = 0;
    return eval_ast(ctx, &ast_ctx);
}

static bool isspecial(char c) {
    return c == '(' || c == ')';
}

static struct sx_rt_val eval_ast(struct sx_ctx *ctx, struct ast_ctx *actx) {
    struct sx_rt_val res = { 0 };
    struct sx_ast_expr *expr = (void*) (ctx->heap + actx->eval_offset);
    actx->eval_offset += sizeof(*expr);
    struct sx_rt_val *args = alloc_args(ctx, actx, expr->arg_count);

    for (uint16_t i = 0; i < expr->arg_count; i++) {
        uint8_t *type = (void*) (ctx->heap + actx->eval_offset);
        if (*type == AST_EXPR)
            args[i] = eval_ast(ctx, actx);
        else if (*type == AST_NUM)
            args[i] = get_num(ctx, actx);
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

static struct sx_rt_val get_num(struct sx_ctx *ctx, struct ast_ctx *actx) {
    struct sx_rt_val res = { 0 };
    struct sx_ast_val *val = (void*) (ctx->heap + actx->eval_offset);
    actx->eval_offset += sizeof(*val);
    res.type = RT_NUM;
    res.number = strtol(buf + val->value, NULL, 10);

    return res;
}

static void parse_expr(struct sx_ctx *ctx, struct ast_ctx *actx) {
    struct sx_ast_expr *expr = alloc_expr(ctx, actx);
    expr->type = AST_EXPR;
    actx->tok_index++; /* skip ( */
    if (ctx->tokens[actx->tok_index] != IDENT) {
        sx_err = "Expected identifier!";
        return;
    }

    expr->ident = ctx->locs[actx->tok_index++];
    sx_tok current = ctx->tokens[actx->tok_index];
    uint16_t arg_count = 0;

    while (current != ')') {
        if (actx->tok_index >= actx->tok_limit) {
            sx_err = "Expected )";
            return;
        }

        if (current == LPAREN || current == IDENT || current == NUM) {
            arg_count++;
            if (current == LPAREN) 
                parse_expr(ctx, actx);
            else
                parse_val(ctx, actx);
        }

        current = ctx->tokens[actx->tok_index];
    }
    expr->arg_count = arg_count;
    actx->tok_index++; /* skip ) */

    return;
}

static void parse_val(struct sx_ctx *ctx, struct ast_ctx *actx) {
    struct sx_ast_val *val = alloc_val(ctx, actx);
    sx_tok current = ctx->tokens[actx->tok_index];
    if (current == IDENT)
        val->type = AST_IDENT;
    else if (current == NUM)
        val->type = AST_NUM;
    val->value = ctx->locs[actx->tok_index];
    actx->tok_index++; /* skip over */
}

static struct sx_rt_val *alloc_args(struct sx_ctx *ctx, struct ast_ctx *actx, uint16_t arg_count) {
    struct sx_rt_val *ptr = (void*) (ctx->heap + actx->arena_index);
    actx->arena_index += sizeof(*ptr) * arg_count;
    return ptr;
}

static struct sx_ast_val *alloc_val(struct sx_ctx *ctx, struct ast_ctx *actx) {
    struct sx_ast_val *ptr = (void*) (ctx->heap + actx->arena_index);
    actx->arena_index += sizeof(*ptr);
    return ptr;
}

static struct sx_ast_expr *alloc_expr(struct sx_ctx *ctx, struct ast_ctx *actx) {
    struct sx_ast_expr *ptr = (void*) (ctx->heap + actx->arena_index);
    actx->arena_index += sizeof(*ptr);
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

/* builtin routines */
static struct sx_rt_val plus(struct sx_rt_val *args, uint16_t nargs) {
    struct sx_rt_val res = { 0 };
    res.type = RT_NUM;

    for (uint16_t i = 0; i < nargs; i++) res.number += args[i].number;

    return res;
}
