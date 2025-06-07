#!/bin/sh

CC="${CC:-cc}"
OPT="${OPT:--O0}"

NOWARNS="-Wno-dangling-pointer -Wno-unused-parameter"

"$CC" -std=c11 -static -Wall -Wextra $NOWARNS -g "$OPT" ./main.c ./src/sc.c -lm -o sc
