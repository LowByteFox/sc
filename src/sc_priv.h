#ifndef __SC_PRIV_H__
#define __SC_PRIV_H__

#include "sc.h"
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
    uint16_t ident; /* index of the ident in the buffer */
    uint16_t arg_count; /* number of args the expression has */
};

struct sc_priv_fns {
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

#endif
