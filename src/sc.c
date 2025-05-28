#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>

#include "sc.h"
#include "sc_priv.h"
#include "config.h"

const char *sc_err = NULL;
static const char *buf = NULL;

static struct sc_priv_fns priv[] = {
    { false, "+", plus },
    { false, "-", minus },
    { false, "*", mult },
    { false, "/", divide },
    { false, "len", len },
    { false, "list", list },
    { false, "cons", cons },
    { false, "car", car },
    { false, "cdr", cdr },
    { false, "begin", begin },
    { true, "eval", eval },
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

        if (isdigit(c)) {
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
    ast_ctx.tok_limit = toks_len - 1;
    ctx->_ctx = &ast_ctx;

    if (ctx->tokens[0] != '(') {
        sc_err = "Expected '('!";
        return nul;
    }

    parse_expr(ctx);
    ast_ctx.eval_offset = 0;
    sc_value res = eval_ast(ctx);
    ctx->_ctx = NULL;
    return res;
}

static bool isspecial(char c) {
    return c == '(' || c == ')';
}

static sc_value eval_ast(struct sc_ctx *ctx) {
    struct sc_ast_expr *expr = (void*) (ctx->heap + ctx->_ctx->eval_offset);
    ctx->_ctx->eval_offset += sizeof(*expr);
    uint8_t fn_index = sizeof(priv) / sizeof(priv[0]) - 1;
    uint16_t len = 0;
    const char *it = buf + expr->ident;
    while (!isspace(*it) && !isspecial(*it)) { it++; len++; }

    it = buf + expr->ident;
    for (uint8_t i = 0; priv[i].name != NULL; i++) {
        if (strncmp(it, priv[i].name, len) == 0) {
            fn_index = i; break;
        }
    }
    if (priv[fn_index].name == NULL) return (sc_value) {0};
    sc_value *args = sc_alloc(ctx, expr->arg_count * sizeof(*args));

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
        }
    }

    return priv[fn_index].run(ctx, args, expr->arg_count);
}

static sc_value get_val(struct sc_ctx *ctx, uint8_t type) {
    sc_value res = { 0 };
    struct sc_ast_val *val = (void*) (ctx->heap + ctx->_ctx->eval_offset);
    ctx->_ctx->eval_offset += sizeof(*val);
    if (type == SC_AST_NUM) {
        res.type = SC_NUM_VAL;
        res.number = strtol(buf + val->value, NULL, 10);
    } else if (type == SC_AST_REAL) {
        res.type = SC_REAL_VAL;
        res.real = strtod(buf + val->value, NULL);
    } else if (type == SC_AST_BOOL) {
        res.type = SC_BOOL_VAL;
        res.boolean = buf[val->value] == 't' ? true : false;
    } else if (type == SC_AST_STRING) {
        size_t len = strcspn(buf + val->value, "\"");
        res.type = SC_STRING_VAL;
        res.str = sc_alloc(ctx, len + 1);
        memcpy(res.str, buf + val->value, len);
        res.str[len] = 0;
    }

    return res;
}

static void parse_expr(struct sc_ctx *ctx) {
    uint16_t start = ctx->_ctx->arena_index;
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
    expr->jump_by = ctx->_ctx->arena_index - start;
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

void sc_init(struct sc_ctx *ctx) {
    /* */
}

void *sc_alloc(struct sc_ctx *ctx, uint16_t size) {
    void *ptr = ctx->heap + ctx->_ctx->arena_index;
    ctx->_ctx->arena_index += size;
    return ptr;
}

/* helper fns */
static bool has_real(sc_value *args, uint16_t nargs) {
    for (uint16_t i = 0; i < nargs; i++) { if (args[i].type == SC_REAL_VAL) return true; }
    return false;
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

static sc_value minus(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    sc_value res = { 0 };
    bool real = has_real(args, nargs);
    res.type = real ? SC_REAL_VAL : SC_NUM_VAL;

    if (nargs == 0) return res;

    if (real) res.real = sc_get_number(args[0]); else res.number = sc_get_number(args[0]);

    for (uint16_t i = 1; i < nargs; i++)
        if (real) res.real -= sc_get_number(args[i]); else res.number -= sc_get_number(args[i]);

    return res;
}

static sc_value mult(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    sc_value res = { 0 };
    bool real = has_real(args, nargs);
    res.type = real ? SC_REAL_VAL : SC_NUM_VAL;

    if (nargs == 0) return res;

    if (real) res.real = sc_get_number(args[0]); else res.number = sc_get_number(args[0]);

    for (uint16_t i = 1; i < nargs; i++)
        if (real) res.real *= sc_get_number(args[i]); else res.number *= sc_get_number(args[i]);

    return res;
}

static sc_value divide(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    sc_value res = { 0 };
    bool real = has_real(args, nargs);
    res.type = real ? SC_REAL_VAL : SC_NUM_VAL;

    if (nargs == 0) return res;

    if (real) res.real = sc_get_number(args[0]); else res.number = sc_get_number(args[0]);

    for (uint16_t i = 1; i < nargs; i++)
        if (real) res.real /= sc_get_number(args[i]); else res.number /= sc_get_number(args[i]);

    return res;
}

static sc_value len(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    sc_value res = { 0 };
    if (nargs != 1) return res;
    if (args[0].type != SC_STRING_VAL) return res;
    res.type = SC_NUM_VAL;
    res.number = strlen(args[0].str);
    return res;
}

static sc_value list(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    sc_value res = { 0 };

    sc_value *iter = &res;
    for (uint16_t i = 0; i < nargs; i++) {
        iter->type = SC_LIST_VAL;
        iter->list.current = args + i; /* there is no GC, can do this */
        iter->list.next = sc_alloc(ctx, sizeof(res));
        iter = iter->list.next;
    }

    return res;
}

static sc_value cons(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    sc_value res = { 0 };
    if (nargs != 2) return res;

    return list(ctx, args, nargs);
}

static sc_value car(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    sc_value res = { 0 };
    if (nargs != 1) return res;
    if (args[0].type != SC_LIST_VAL) return res;

    return *args[0].list.current;
}

static sc_value cdr(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    sc_value res = { 0 };
    if (nargs != 1) return res;
    if (args[0].type != SC_LIST_VAL) return res;

    return *args[0].list.next;
}

static sc_value begin(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) { return args[nargs - 1]; }

/* TEMP */
static sc_value eval(struct sc_ctx *ctx, sc_value *args, uint16_t nargs) {
    uint16_t old = ctx->_ctx->eval_offset;
    ctx->_ctx->eval_offset = args[0].lazy_addr;
    sc_value res = eval_ast(ctx);
    ctx->_ctx->eval_offset = old;    
    return res;
}
