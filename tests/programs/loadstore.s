# Byte, half and word accesses, little-endian, with sign and zero extension.
addi x1, x0, 8
addi x4, x0, -2
sw   x4, 0x0[x1]        # mem[8] = 0xfffffffe
lw   x5, 0x0[x1]        # -2
lbu  x6, 0x0[x1]        # 254
lb   x7, 0[x1]          # -2
lh   x8, 0(x1)          # -2
lhu  x9, 2[x1]          # 0xffff
sh   x4, 6[x1]          # mem[12] = 0xfffe0000
lw   x10, 4[x1]
addi x11, x0, 0x7F
sb   x11, 1[x0]         # mem[0] = 0x00007f00
sb   x4, 3[x0]          # mem[0] = 0xfe007f00
lw   x12, 0[x0]
