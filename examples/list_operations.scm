(let list-sum
  (lambda (lst)
    (if (< 0 (length lst))
      (+ (car lst) (list-sum (cdr lst)))
      0)))

(let reverse
  (lambda (lst)
    (begin
      (let res nil)
      (while (< 0 (length lst))
        (begin
          (set! res (append (list (car lst)) res))
          (set! lst (cdr lst))))
      (begin res))))

(let remove
  (lambda (pred lst)
    (begin
      (let res nil)
      (while (< 0 (length lst))
        (if (pred (car lst))
          (set! lst (cdr lst))
          (begin
            (set! res (cons res (car lst)))
            (set! lst (cdr lst))
            )))
      (begin (cdr res)))))

(display (remove (lambda (x) (< x 2)) (list 1 2 3 4)))
(newline)

(display (reverse (list "cat" "dog" "fox" "shark")))
(newline)

(display (list-sum (list 1 2 3 4 5 6 7 8 9)))
(newline)
