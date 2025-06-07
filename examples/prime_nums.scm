(define divides? 
  (lambda (a b) (= (modulo b a) 0)))

(define has-divisor?
  (lambda (n k)
    (if (> (* k k) n)
      #f
      (if (divides? k n)
        #t
        (has-divisor? n (+ k 1))))))

(define prime?
  (lambda (x)
    (if (< x 2)
      #f
      (not (has-divisor? x 2)))))

(display (prime? 7))
(newline)

(display (prime? 17)) 
(newline)

(display (prime? 15)) 
(newline)
