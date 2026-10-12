# JAL and JALR: call a subroutine and return through the link register.
        addi x10, x0, 7
        jal  x1, double         # x1 = 8
        addi x11, x10, 0        # x11 = 14 after the return
        jal  x0, done
double: add  x10, x10, x10
        jalr x0, x1, 0          # return
done:   addi x12, x0, 12
        addi x5, x0, 40
        jalr x6, x5, 0          # jump to address 40, x6 = 36
        addi x13, x0, 1         # skipped
        addi x14, x0, 2
