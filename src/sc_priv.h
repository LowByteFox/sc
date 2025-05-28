#ifndef __SC_PRIV_H__
#define __SC_PRIV_H__

#include "sc.h"
#include <stdbool.h>
#include <stdint.h>

#if HEAP_SIZE > UINT16_MAX
#error "Heap size cannot be more than 65535 (UINT16_MAX) bytes"
#endif

enum sc_node_types {
    SC_AST_EXPR = 1,
    SC_AST_IDENT,
    SC_AST_NUM,
    SC_AST_REAL,
    SC_AST_BOOL,
    SC_AST_STRING,
};

struct sc_ast_val {
    uint8_t type;
    uint16_t value; /* index of the value in the buffer/index of expression node in the heap */
};

struct sc_ast_expr {
    uint8_t type;
    uint16_t jump_by; /* when function is lazy, to just skip N bytes over the args */
    uint16_t ident; /* index of the ident in the buffer */
    uint16_t arg_count; /* number of args the expression has */
};

struct sc_priv_fns {
    bool lazy;
    const char *name;
    sc_fn run;
};

struct sc_ast_ctx {
    uint16_t arena_index;
    uint16_t tok_limit;
    union {
        uint16_t tok_index;
        uint16_t eval_offset;
    };
};

static bool isspecial(char c);
static sc_value eval_ast(struct sc_ctx *ctx);
static sc_value get_val(struct sc_ctx *ctx, uint8_t type);
static void parse_expr(struct sc_ctx *ctx);
static void parse_val(struct sc_ctx *ctx);
static void append_tok(struct sc_ctx *ctx, uint16_t *len, uint16_t *sz, sc_tok tk);
static void append_loc(struct sc_ctx *ctx, uint16_t *len, uint16_t *sz, sc_loc loc);

/* helper fns */
static bool has_real(sc_value *args, uint16_t nargs);

/* builtin routines */
static sc_value plus(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value minus(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value mult(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value divide(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value len(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value list(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value cons(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value car(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value cdr(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value begin(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value eval(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);

#endif
