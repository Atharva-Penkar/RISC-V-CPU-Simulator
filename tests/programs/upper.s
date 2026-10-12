# LUI and AUIPC build 32-bit constants and PC-relative addresses.
lui   x1, 0x12345
addi  x1, x1, 0x678     # 0x12345678
auipc x2, 1             # pc 8 + 0x1000
lui   x3, 0xFFFFF       # 0xfffff000
addi  x3, x3, -1        # 0xffffefff = -4097
auipc x4, 0             # pc 20
