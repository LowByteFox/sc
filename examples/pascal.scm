(let pascal
  (lambda (n)
    (begin
      (let row 1)
      (while (<= row n)
        (begin
          (let c 1)
          (let i 1)
          (while (<= i row)
            (begin
              (display c)
              (display " ")
              (set! c (/ (* c (- row i)) i))
              (set! i (+ i 1))))
          (newline)
          (set! row (+ row 1)))))))

(pascal 7)
