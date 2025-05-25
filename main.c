#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/sx.h"

int main()
{
    struct sx_ctx ctx = { 0 };
    const char *prog = "(+ 1 (+ 1 (+ 2 3)))";

    struct sx_rt_val res = sx_eval(&ctx, prog, strlen(prog));

    if (sx_err != NULL) {
        fprintf(stderr, "sx error: %s\n", sx_err);
        abort();
    }

    if (res.type == RT_NUM)
        printf("%ld\n", res.number);

    return 0;
}
