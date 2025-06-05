#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

#include "src/sc.h"

static void usage(void);
static void print_value(sc_value *val);
static sc_value display(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value f_open(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);
static sc_value f_read(struct sc_ctx *ctx, sc_value *args, uint16_t nargs);

struct sc_fns funs[] = {
    { false, "my_display", display },
    { false, "fopen", f_open },
    { false, "fread", f_read },
    { false, NULL, NULL }
};

int main(int argc, char **argv)
{
    char *eval, *path;
    eval = path = NULL;
    int c;

    while ((c = getopt(argc, argv, "hf:e:")) != -1) {
        switch (c) {
        case 'f':
            path = optarg;
            break;
        case 'e':
            eval = optarg;
            break;
        case 'h':
        default:
            usage();
        }
    }

    argc -= optind;
    argv += optind;

    srand(time(NULL));
    struct sc_ctx ctx = { 0 };
    ctx.user_fns = funs;
    sc_value res = sc_nil;

    if (eval != NULL && path != NULL) {
        fprintf(stderr, "sc: specify either -f or -e!");
        return 1;
    }

    if (eval != NULL) res = sc_eval(&ctx, eval, strlen(eval));
    else if (path != NULL) {
        char *buf = NULL;
        int written = 0;
        FILE *f = fopen(path, "r");
        for (int i = 1; !feof(f); i++) {
            buf = realloc(buf, BUFSIZ * i);
            written += fread(buf + written, 1, BUFSIZ, f);
        }
        *strrchr(buf, '\n') = 0;
        fclose(f);
        res = sc_eval(&ctx, buf, strlen(buf));
    } else {
        for (;;) {
            char *in = NULL;
            size_t size = 0;
            printf(">> ");
            fflush(stdout);
            getline(&in, &size, stdin);
            *strrchr(in, '\n') = 0;
            if (strcmp(".q", in) == 0 || strcmp(".exit", in) == 0) {
                free(in);
                exit(0);
            } else if (strcmp(".stats", in) == 0) {
                printf("Peak memory usage: %dB\n", sc_heap_usage(&ctx));
                free(in);
                continue;
            }

            res = sc_eval(&ctx, in, size);
            if (res.type == SC_ERROR_VAL) fprintf(stderr, "sc error: %s\n", res.err);
            else if (res.type != SC_NOTHING_VAL) {
                sc_display(&ctx, &res, 1);
                putchar('\n');
            }

            free(in);
        }
        return 0;
    }

    if (res.type == SC_ERROR_VAL) {
        fprintf(stderr, "sc error: %s\n", res.err);
        abort();
    }

    if (res.type == SC_NOTHING_VAL)
        return 0;

    sc_display(&ctx, &res, 1);
    putchar('\n');

    return 0;
}

static void usage(void)
{
    fprintf(stderr, "usage: sc [-h] [-f file|-e str]\n");
    exit(1);
}

static void print_value(sc_value *val)
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
