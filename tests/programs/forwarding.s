# Every value is consumed by the very next instruction: EX/MEM, MEM/WB and load-use paths.
addi x1, x0, 10
addi x2, x1, 5          # x1 from EX/MEM: 15
add  x3, x1, x2         # x1 from MEM/WB, x2 from EX/MEM: 25
sw   x3, 0[x0]          # store data forwarded: mem[0] = 25
lw   x4, 0[x0]          # 25
addi x5, x4, 1          # load-use: one stall, then MEM/WB: 26
lw   x6, 0[x0]
sw   x6, 4[x0]          # load then store of the loaded value: mem[4] = 25
addi x7, x0, 44
jalr x8, x7, 0          # jump target from EX/MEM: address 44, x8 = 40
addi x9, x0, 1          # skipped
addi x10, x8, 0         # x10 = 40
