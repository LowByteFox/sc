#ifndef __SX_H__
#define __SX_H__

#include <stdint.h>
#include <stdbool.h>

#define HEAP_SIZE INT16_MAX
#define ARR_GROW 64

#define sx_get_number(val) (val.type == RT_NUM ? val.number : val.real)

struct sx_ast_ctx;

enum sx_tokens {
    END = 1,
    LPAREN = '(',
    RPAREN = ')',

    IDENT = 'I',
    NUM = 'N',
    REAL = 'R',
    BOOL = 'B',
    STRING = 'S',
};

enum sx_rt_type {
    RT_NOTHING,
    RT_NUM,
    RT_REAL,
    RT_BOOL,
    RT_STRING,
};

typedef uint8_t sx_tok;
typedef uint16_t sx_loc;
typedef struct sx_rt_val (*sx_fn)(struct sx_rt_val *args, uint16_t nargs);

struct sx_kv {
    sx_tok val_type; /* type of value */
    uint16_t name; /* offset in heap */
    uint16_t next; /* index to next global */
};

struct sx_ctx {
    struct sx_kv glbs;
    uint8_t *heap; /* < UINT16_MAX */
    sx_tok *tokens;
    sx_loc *locs; /* u16 offsets */
    struct sx_ast_ctx *_ctx;
};

struct sx_rt_val {
    uint8_t type;
    union { uint64_t number; double real; bool boolean; char *str; };
};

extern const char *sx_err;

void *sx_alloc(struct sx_ctx *ctx, uint16_t size);
struct sx_rt_val sx_eval(struct sx_ctx *ctx, const char *buffer, uint16_t buflen);

#endif
