#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

#include "src/sc.h"

static void usage(void);

int main(int argc, char **argv)
{
    bool stats = false;
    char *eval, *path;
    eval = path = NULL;
    int c;

    while ((c = getopt(argc, argv, "hse:f:")) != -1) {
        switch (c) {
        case 'f':
            path = optarg;
            break;
        case 'e':
            eval = optarg;
            break;
        case 's':
            stats = true;
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

    if (eval != NULL) {
        sc_display(&ctx, &res, 1);
        putchar('\n');
    }

    if (stats) {
        printf("Peak memory usage: %dB\n", sc_heap_usage(&ctx));
    }

    return 0;
}

static void usage(void)
{
    fprintf(stderr, "usage: sc [-hs] [-e str|-f file]\n");
    exit(1);
}
