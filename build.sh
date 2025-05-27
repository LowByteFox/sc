#!/bin/sh

CC="${CC:-cc}"
OPT="${OPT:--O0}"

NOWARNS="-Wno-dangling-pointer"

"$CC" -static -Wall -Wextra "$NOWARNS" -g "$OPT" ./main.c ./src/sx.c
