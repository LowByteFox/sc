# sc
Tiny, embeddable Scheme-like language implemented in C
> Project is highly inspired by [fe](https://github.com/rxi/fe)

> [!WARNING]  
> the C version is experiencing UB due to how data are allocated, use with caution

```scm
(let reverse
  (lambda (lst)
    (begin
      (let res (list))
      (while (< 0 (length lst))
        (begin
          (set! res (append (list (car lst)) res))
          (set! lst (cdr lst))))
      (begin res))))

(display "rare => common")
(newline)

(display (reverse (list "cat" "dog" "fox" "shark"))) ; ("shark" "fox" "dog" "cat")
(newline)
```

## Overview
- Supports numbers, decimal numbers, booleans, strings, pairs, lists & lambdas
- Static scoping - (the GC needs to be better to allow something like currying)
- Minimal amount of allocations, configurable with `HEAP_SIZE`
- Simple ref counted GC
- Easy and simple C API

## Documentation
- [Language](doc/lang.md)
- [C API](doc/api.md)
- [Examples](examples)
