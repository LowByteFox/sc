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
(set! sym val)
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

```scm
(call (lambda (x) (* x x)) 8)
```
`sc` requires an identifier after `(`, this function was made to avoid such purpose

### Builtin Functions

#### Arithmetic operations
```scm
(+ 3 4)
(- 14 7)
(* 1 7)
(/ 7 1)
(% 15 8) ; or (modulo 15 8)
```

#### Logical operations
```scm
(= 1 1)
(< 1 2)
(<= 1 1)
(> 2 1)
(>= 2 2)
(and #t #t)
(or #f #t)
(not #t)
(eq? (list 1 2) (list 1 2)) ; or (equal? (list 1 2) (list 1 2))
```

#### List operations
```scm
(list 1 2 3 4)
(car (list 1 2 3))
(cdr (list 1 2 3))
(cons 1 2)
(length (list 1 2 3 4))
(append (list 1 2) (list 3 4) (list 5 6))
(map (lambda (x) (* x x)) (list 1 2 3 4))
(filter (lambda (x) (< 1 x)) (list 1 2 3))
(find (lambda (x) (= x 2)) (list 1 2 3))
(at (list 1 2 3 4) 2)
```

#### String operations
```scm
(string-length "Hello, World!")
(string-append "Hello" " , " "World!")
(string-upcase "hey")
(string-downcase "NICE")
(string-contains? "Haystack" "ay")
(at "Hey" 0)
```

#### Type casting
```scm
(number "77")
(real "3.14")
(string 3.14)
```

#### Miscellaneous operations
```scm
(error "Something went wrong")
(random) ; or (random 100) or (random 100.50)
(abs -2)
(sqrt 64)
(expt 2 3)
(mean 1 7 2 9)
(display "Hello, World!")
(newline)
```
