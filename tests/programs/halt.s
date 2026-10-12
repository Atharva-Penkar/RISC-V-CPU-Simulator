# ebreak stops the program; nothing after it runs.
        addi x1, x0, 3
loop:   addi x1, x1, -1
        sw   x1, 0[x0]
        bne  x1, x0, loop
        addi x2, x0, 7
        ebreak
        addi x3, x0, 1          # not executed
        sw   x2, 4[x0]          # not executed
