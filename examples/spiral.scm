(let max
  (λ (a b)
    (if (< a b) b a)))

(let spiral
  (λ (n)
    (begin
      (let p (- (* n 2) 1))
      (let i 1)
      (while (<= i p)
        (begin
          (let j 1)
          (while (<= j p)
            (begin
              (display (+ (max (abs (- i n)) (abs (- j n))) 1))
              (display " ")
              (set! j (+ j 1))))
          (newline)
          (set! i (+ i 1)))))))

(spiral 7)
