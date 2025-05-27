#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/sx.h"

int main()
{
    struct sx_ctx ctx = { 0 };
    const char *prog = "(len \"Hello World!\")";

    struct sx_rt_val res = sx_eval(&ctx, prog, strlen(prog));

    if (sx_err != NULL) {
        fprintf(stderr, "sx error: %s\n", sx_err);
        abort();
    }

    if (res.type == RT_NUM)
        printf("%ld\n", res.number);
    else if (res.type == RT_REAL)
        printf("%f\n", res.real);

    return 0;
}
