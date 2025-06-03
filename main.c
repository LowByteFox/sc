#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "src/sc.h"

void print_value(sc_value *val);

static sc_value display(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value f_open(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value f_read(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);

struct sc_fns funs[] = {
    { false, "my_display", display },
    { false, "fopen", f_open },
    { false, "fread", f_read },
    { false, NULL, NULL }
};

int main()
{
    srand(time(NULL));
    struct sc_ctx ctx = { 0 };
    ctx.user_fns = funs;
    const char *prog = "(fread (fopen \"/etc/passwd\"))";

    sc_value res = sc_eval(&ctx, prog, strlen(prog));

    if (sc_err != NULL) {
        fprintf(stderr, "sc error: %s\n", sc_err);
        abort();
    }

    if (res.type == SC_ERROR_VAL) {
        fprintf(stderr, "sc runtime error: %s\n", res.err);
        abort();
    }

    if (res.type == SC_NOTHING_VAL)
        return 0;

    print_value(&res);
    putchar('\n');

    return 0;
}

void print_value(sc_value *val)
{
    if (val == NULL || val->type == SC_NOTHING_VAL)
        printf("nil");
    else if (val->type == SC_NUM_VAL)
        printf("%ld", val->number);
    else if (val->type == SC_REAL_VAL)
        printf("%f", val->real);
    else if (val->type == SC_BOOL_VAL)
        printf("%s", val->boolean ? "#t" : "#f");
    else if (val->type == SC_STRING_VAL)
        printf("\"%s\"", val->str);
    else if (val->type == SC_LIST_VAL) {
        sc_value *iter = val;
        printf("(");
        while (iter != NULL) {
            print_value(iter->list.current);
            if (iter->list.current != NULL)
                printf(" ");
            iter = iter->list.next;
        }
        printf(")");
    }
}

static sc_value display(struct sc_ctx *ctx, sc_value *args, uint16_t nargs)
{
    if (nargs != 1) return sc_error("display: only 1 argument!");
    print_value(args + 0);
    putchar('\n');
    return sc_nil;
}

static void f_close(struct sc_ctx *ctx, void *data) {
    FILE *f = *(FILE**) data;
    fclose(f);
    printf("Closing file!\n");
}

static sc_value f_open(struct sc_ctx *ctx, sc_value *args, uint16_t nargs)
{
    if (nargs != 1) return sc_error("fopen: only 1 argument!");
    FILE *f = fopen(args[0].str, "r");
    sc_value data = sc_userdata(ctx, sizeof(FILE*), f_close);
    *(FILE**) data.userdata.data = f;
    return data;
}

static sc_value f_read(struct sc_ctx *ctx, sc_value *args, uint16_t nargs)
{
    if (nargs != 1) return sc_error("fread: only 1 argument!");
    char buffer[256] = { 0 };
    FILE *f = *(FILE**) args[0].userdata.data;
    fgets(buffer, 256, f);
    *strchr(buffer, '\n') = 0;

    return sc_string(ctx, buffer);
}
