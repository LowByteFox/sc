#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/sc.h"

void print_value(sc_value *val);

int main()
{
    struct sc_ctx ctx = { 0 };
    const char *prog = "(len \"uwu owo\")";

    sc_value res = sc_eval(&ctx, prog, strlen(prog));

    if (sc_err != NULL) {
        fprintf(stderr, "sc error: %s\n", sc_err);
        abort();
    }

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
