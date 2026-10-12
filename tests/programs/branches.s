# Signed and unsigned branch conditions; x5..x10 stay 0 only where a branch is taken.
        addi x1, x0, -3
        addi x2, x0, 2
        blt  x1, x2, a          # -3 < 2: taken
        addi x5, x0, 1
a:      bltu x1, x2, b          # 0xfffffffd < 2: not taken
        addi x6, x0, 1
b:      bge  x2, x1, c          # taken
        addi x7, x0, 1
c:      bgeu x1, x2, d          # taken
        addi x8, x0, 1
d:      beq  x1, x1, e          # taken
        addi x9, x0, 1
e:      bne  x1, x1, f          # not taken
        addi x10, x0, 1
f:
