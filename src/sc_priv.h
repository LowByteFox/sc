#ifndef __SC_PRIV_H__
#define __SC_PRIV_H__

#include "sc.h"
#include <stdbool.h>
#include <stdint.h>

#if HEAP_SIZE > UINT16_MAX
#error "Heap size cannot be more than 65535 (UINT16_MAX) bytes"
#endif

#define sc_bool(val) ((sc_value) { .type = SC_BOOL_VAL, .boolean = val })

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

struct sc_stack_kv {
    char *ident;
    struct sc_stack_kv *next;
    sc_value value;
};

struct sc_stack_node {
    struct sc_stack_kv *first_value;
    struct sc_stack_kv *last_value;
    struct sc_stack_node *next_frame;
};

struct sc_stack {
    struct sc_stack_node *head;
    struct sc_stack_node *tail;
};

static bool isspecial(char c);
static sc_value eval_ast(struct sc_ctx *ctx);
static sc_value get_val(struct sc_ctx *ctx, uint8_t type);
static sc_value eval_lambda(struct sc_ctx *ctx, sc_value *lambda, sc_value *args, uint16_t nargs);
static void parse_expr(struct sc_ctx *ctx);
static void parse_val(struct sc_ctx *ctx);
static void append_tok(struct sc_ctx *ctx, uint16_t *len, uint16_t *sz, sc_tok tk);
static void append_loc(struct sc_ctx *ctx, uint16_t *len, uint16_t *sz, sc_loc loc);

static void push_frame(struct sc_ctx *ctx);
static void pop_frame(struct sc_stack *stack);
static struct sc_stack_kv *stack_node_find(struct sc_stack_node *node, const char *ident);
static struct sc_stack_kv *stack_find(struct sc_stack *stack, const char *ident);
static struct sc_stack_kv *global_add(struct sc_ctx *ctx);
static struct sc_stack_kv *frame_add(struct sc_ctx *ctx);

/* helper fns */
static sc_value eval_at(struct sc_ctx *ctx, uint16_t addr);
static bool has_real(sc_value *args, uint16_t nargs);
static char *get_ident(struct sc_ctx *ctx, struct sc_ast_val *val);
static char *alloc_ident(struct sc_ctx *ctx, uint16_t addr);

/* builtin routines */
static sc_value plus(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value minus(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value mult(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value divide(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value lt(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value lte(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value eql(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value lte(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value gt(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value gte(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value len(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value list(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value cons(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value car(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value cdr(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value begin(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value define(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value define_scope(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value lambda(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value eq(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value equal(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);

#endif
