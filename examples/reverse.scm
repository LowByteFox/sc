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
