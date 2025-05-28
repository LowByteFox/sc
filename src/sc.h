#ifndef __SC_H__
#define __SC_H__

#include <stdint.h>
#include <stdbool.h>

#define HEAP_SIZE UINT16_MAX
#define ARR_GROW 64

#define sc_get_number(val) (val.type == SC_NUM_VAL ? val.number : val.real)

struct sc_ast_ctx;

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

enum sc_val_type {
    SC_NOTHING_VAL = 0,
    SC_NUM_VAL,
    SC_REAL_VAL,
    SC_BOOL_VAL,
    SC_STRING_VAL,
    SC_LIST_VAL,
};

struct sc_ctx;
struct sc_val;

typedef uint8_t sc_tok;
typedef uint16_t sc_loc;
typedef struct sc_val sc_value;
typedef sc_value (*sc_fn)(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);

struct sc_kv {
    sc_tok val_type; /* type of value */
    uint16_t name; /* offset in heap */
    uint16_t next; /* index to next global */
};

struct sc_ctx {
    struct sc_kv glbs;
    uint8_t *heap; /* < UINT16_MAX */
    sc_tok *tokens;
    sc_loc *locs; /* u16 offsets */
    struct sc_ast_ctx *_ctx;
};

struct sc_val {
    uint8_t type;
    union {
        bool boolean;
        uint64_t number;
        double real;
        char *str;
        struct {
            struct sc_val *current;
            struct sc_val *next;
        } list;
    };
};

extern const char *sc_err;

void *sc_alloc(struct sc_ctx *ctx, uint16_t size);
sc_value sc_eval(struct sc_ctx *ctx, const char *buffer, uint16_t buflen);

#endif
