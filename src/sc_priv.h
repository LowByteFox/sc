#ifndef __SC_PRIV_H__
#define __SC_PRIV_H__

#include "sc.h"
#include <stdbool.h>
#include <stdint.h>

#if HEAP_SIZE > UINT16_MAX
#error "Heap size cannot be more than 65535 (UINT16_MAX) bytes"
#endif

#define stack_find(stack, ident) (stack_node_find(stack->head, ident))

enum sc_tokens {
    SC_END_TOK = 1,
    SC_LPAREN_TOK = '(',
    SC_RPAREN_TOK = ')',

    SC_IDENT_TOK = 'I',
    SC_NUM_TOK = 'N',
    SC_REAL_TOK = 'R',
    SC_BOOL_TOK = 'B',
    SC_STRING_TOK = 'S',
    SC_LIST_TOK = 'L',
};

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

struct sc_gc {
    uint16_t arena_index;
    uint16_t memory_begin;
    uint16_t memory_limit;
};

struct sc_ast_ctx {
    uint16_t tok_limit;
    union {
        uint16_t tok_index;
        uint16_t eval_offset;
    };
    struct sc_gc gc;
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

struct sc_gc_obj {
    uint16_t size;
    uint16_t count;
    uint8_t data[];
};

static bool isspecial(char c);
static sc_value eval_ast(struct sc_ctx *ctx);
static sc_value get_val(struct sc_ctx *ctx, uint8_t type);
static sc_value parse_expr(struct sc_ctx *ctx);
static void parse_val(struct sc_ctx *ctx);
static void append_tok(struct sc_ctx *ctx, uint16_t *len, uint16_t *sz, sc_tok tk);
static void append_loc(struct sc_ctx *ctx, uint16_t *len, uint16_t *sz, sc_loc loc);

static void push_frame(struct sc_ctx *ctx);
static void pop_frame(struct sc_ctx *ctx, struct sc_stack *stack);
static struct sc_stack_kv *stack_node_find(struct sc_stack_node *node, const char *ident);
static struct sc_stack_kv *global_add(struct sc_ctx *ctx);
static struct sc_stack_kv *frame_add(struct sc_ctx *ctx);

/* helper fns */
static void free_args(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value eval_at(struct sc_ctx *ctx, uint16_t addr);
static bool has_real(sc_value *args, uint16_t nargs);
static char *get_ident(struct sc_ctx *ctx, struct sc_ast_val *val);
static char *alloc_ident(struct sc_ctx *ctx, uint16_t addr);

/* builtin routines */
static sc_value plus(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value minus(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value mult(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value divide(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value sc_mod(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value lt(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value lte(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value and(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value or(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value not(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value eql(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value lte(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value gt(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value gte(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value len(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value list(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value append(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value cons(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value car(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value cdr(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value begin(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value define(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value let(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value lambda(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value cond(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value sc_while(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value call(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value eq(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value rnd(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value sc_abs(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value sc_sqrt(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value sc_expt(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value mean(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value error(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value newline(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value upcase(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value downcase(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value str_contains(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value at(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value tonum(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value toreal(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value tostring(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value map(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value filter(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value find(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);

#endif
