# Register-register and register-immediate arithmetic, including overflow and shifts.
addi x1, x0, 0x00000004
addi x2, x0, 0x0000000B
addi x3, x0, 0xA
addi x4, x0, 255
add  x5, x1, x2         # 15
sub  x6, x1, x2         # -7
lui  x7, 0x80000
addi x7, x7, -1         # 0x7fffffff
addi x8, x7, 1          # wraps to 0x80000000
addi x9, x0, -1
srai x10, x9, 4         # -1
srli x11, x9, 28        # 15
slli x12, x9, 31        # 0x80000000
slt  x13, x9, x0        # -1 < 0 signed
sltu x14, x9, x0        # 0xffffffff < 0 unsigned is false
xori x15, x4, 0x0F      # 0xf0
andi x16, x4, 0x3C      # 0x3c
ori  x17, x1, 0x100     # 0x104
sra  x18, x12, x1       # 0xf8000000
srl  x19, x12, x1       # 0x08000000
sll  x20, x2, x1        # 176
