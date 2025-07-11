#ifndef __SC_H__
#define __SC_H__

#include <stdint.h>
#include <stdbool.h>

#define sc_get_number(val) (val.type == SC_NUM_VAL ? val.number : val.real)
#define sc_nil ((sc_value) { 0 })
#define sc_num(val) ((sc_value) { .type = SC_NUM_VAL, .number = val })
#define sc_real(val) ((sc_value) { .type = SC_REAL_VAL, .real = val })
#define sc_bool(val) ((sc_value) { .type = SC_BOOL_VAL, .boolean = val })
#define sc_error(msg) ((sc_value) { .type = SC_ERROR_VAL, .err = msg })

enum sc_val_type {
    SC_NOTHING_VAL = 0,
    SC_NUM_VAL,
    SC_REAL_VAL,
    SC_BOOL_VAL,
    SC_STRING_VAL,
    SC_LIST_VAL,
    SC_LAMBDA_VAL,
    SC_ERROR_VAL,

    SC_LAZY_EXPR_VAL = INT8_MAX,
    SC_USERDATA_VAL,
};

struct sc_ast_ctx;
struct sc_stack;
struct sc_ctx;
struct sc_val;

typedef uint8_t sc_tok;
typedef uint16_t sc_loc;
typedef struct sc_val sc_value;
typedef sc_value (*sc_fn)(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);

struct sc_fns {
    bool lazy;
    const char *name;
    sc_fn run;
};

struct sc_ctx {
    uint8_t *heap; /* <= UINT16_MAX */
    sc_tok *tokens;
    sc_loc *locs; /* u16 offsets */
    struct sc_ast_ctx *_ctx;
    struct sc_stack *_stack;
    struct sc_fns *user_fns;
};

struct sc_val {
    uint8_t type;
    union {
        bool boolean;
        uint16_t lazy_addr;
        int64_t number;
        double real;
        char *str;
        const char *err;
        struct {
            struct sc_val *current;
            struct sc_val *next;
        } list;
        struct {
            uint16_t arg_count;
            uint16_t args;
            uint16_t body;
        } lambda;
        struct {
            void *data;
            void (*on_gc)(struct sc_ctx *ctx, void *data);
        } userdata;
    };
};

sc_value sc_eval(struct sc_ctx *ctx, const char *buffer, uint16_t buflen);
sc_value sc_eval_lambda(struct sc_ctx *ctx, sc_value *lambda, sc_value *args, uint16_t nargs);

void *sc_alloc(struct sc_ctx *ctx, uint16_t size);
void sc_free(struct sc_ctx *ctx, void *ptr);
void sc_dup(void *ptr);
sc_value sc_dup_value(sc_value val);
void sc_free_value(struct sc_ctx *ctx, sc_value val);

sc_value sc_string(struct sc_ctx *ctx, const char *cstr);
sc_value sc_userdata(struct sc_ctx *ctx, uint16_t size, void (*on_gc)(struct sc_ctx *ctx, void *data));

bool sc_value_eq(sc_value a, sc_value b);
sc_value sc_display(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
uint16_t sc_heap_usage(struct sc_ctx *ctx);

#endif
