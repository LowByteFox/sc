#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/sc.h"

static void print_value(struct sc_rt_val *val);

int main()
{
    struct sc_ctx ctx = { 0 };
    const char *prog = "(cons 1 3)";

    struct sc_rt_val res = sc_eval(&ctx, prog, strlen(prog));

    if (sc_err != NULL) {
        fprintf(stderr, "sc error: %s\n", sc_err);
        abort();
    }

    print_value(&res);
    putchar('\n');

    return 0;
}

static void print_value(struct sc_rt_val *val)
{
    if (val == NULL || val->type == RT_NOTHING)
        printf("nil");
    else if (val->type == RT_NUM)
        printf("%ld", val->number);
    else if (val->type == RT_REAL)
        printf("%f", val->real);
    else if (val->type == RT_BOOL)
        printf("%s", val->boolean ? "#t" : "#f");
    else if (val->type == RT_STRING)
        printf("\"%s\"", val->str);
    else if (val->type == RT_LIST) {
        struct sc_rt_val *iter = val;
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
