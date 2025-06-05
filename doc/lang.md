# The Language
`sc` is mostly based on [Scheme](https://en.wikipedia.org/wiki/Scheme_(programming_language)), however doesn't implement much of sugar syntax and is different in behavior in some cases.

## Functions
### Special Functions

```scm
(define sym val)
```
Defines a new global `sym` with value `val`.

```scm
(let sym val)
```
Creates a new variable `sym` on current scope with value `val`.

```scm
(set! sym val) ;
```
Overrides a variable `sym` with value `val`. It is just an alias to `let`.

```scm
(if cond1 expr1 cond2 expr2 ... else)
(cond cond1 expr1 cond2 expr2 ... else)
```
Evaluate `cond`, if true, returns value of `expr1` otherwise continues until `else`, which you can omit and if/cond will return `nil`.

```scm
(lambda args expr)
(λ args expr)
```
Creates a new function, due to `sc` limitations (as of now), functions need to have at least 1 argument.

```scm
(while cond expr)
```
Keeps evaluating `expr` until `cond` returns `#f`.

### Builtin Functions
