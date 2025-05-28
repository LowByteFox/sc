#ifndef __SC_H__
#define __SC_H__

#include <stdint.h>
#include <stdbool.h>

#define HEAP_SIZE INT16_MAX
#define ARR_GROW 64

#define sc_get_number(val) (val.type == RT_NUM ? val.number : val.real)

struct sc_ast_ctx;

enum sc_tokens {
    END = 1,
    LPAREN = '(',
    RPAREN = ')',

    IDENT = 'I',
    NUM = 'N',
    REAL = 'R',
    BOOL = 'B',
    STRING = 'S',
    LIST = 'L',
};

enum sc_rt_type {
    RT_NOTHING,
    RT_NUM,
    RT_REAL,
    RT_BOOL,
    RT_STRING,
    RT_LIST,
};

struct sc_ctx;

typedef uint8_t sc_tok;
typedef uint16_t sc_loc;
typedef struct sc_rt_val (*sc_fn)(struct sc_ctx *ctx, struct sc_rt_val *args, uint16_t nargs);

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

struct sc_rt_val {
    uint8_t type;
    union {
        bool boolean;
        uint64_t number;
        double real;
        char *str;
        struct {
            struct sc_rt_val *current;
            struct sc_rt_val *next;
        } list;
    };
};

extern const char *sc_err;

void *sc_alloc(struct sc_ctx *ctx, uint16_t size);
struct sc_rt_val sc_eval(struct sc_ctx *ctx, const char *buffer, uint16_t buflen);

#endif
