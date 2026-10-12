# Stores the first 12 Fibonacci numbers at addresses 0, 4, 8, ...
        addi x1, x0, 0          # fib(n)
        addi x2, x0, 1          # fib(n+1)
        addi x3, x0, 0          # address
        addi x4, x0, 48         # end address
next:   sw   x1, 0[x3]
        add  x5, x1, x2
        addi x1, x2, 0
        addi x2, x5, 0
        addi x3, x3, 4
        blt  x3, x4, next
