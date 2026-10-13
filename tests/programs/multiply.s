# M extension: 6! with MUL, then the digits of 720 with DIVU and REMU, stored from address 0.
        addi x1, x0, 6          # n
        addi x2, x0, 1          # factorial
fact:   mul  x2, x2, x1         # depends on the previous mul: try a multiply latency above 1
        addi x1, x1, -1
        bne  x1, x0, fact
        addi x3, x0, 10
        addi x4, x0, 0          # address
        add  x5, x2, x0         # 720
digit:  remu x6, x5, x3         # last decimal digit
        divu x5, x5, x3
        sw   x6, 0[x4]
        addi x4, x4, 4
        bne  x5, x0, digit
        mulh x7, x2, x2         # high half of 720 * 720 = 0
        div  x8, x2, x0         # division by zero gives -1, it does not trap
        rem  x9, x2, x0         # and the remainder is the dividend
