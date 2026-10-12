# Counts to 5 with a backward branch, then jumps over an instruction.
        addi x1, x0, 0
        addi x2, x0, 5
loop:   addi x1, x1, 1
        bne  x1, x2, loop
        jal  x3, end            # x3 = return address 20
        addi x4, x0, 99         # skipped
end:
