# Back-to-back dependencies, a load-use pair, repeated writes and writes to x0.
addi x1, x0, 1
addi x1, x1, 1
addi x1, x1, 1          # x1 = 3
add  x2, x1, x1         # 6
sw   x2, 16[x0]
lw   x3, 16[x0]         # 6
add  x4, x3, x3         # 12: uses the loaded value immediately
addi x0, x0, 5          # x0 stays 0
add  x5, x0, x4         # 12
addi x6, x0, 1
addi x6, x0, 2          # last writer wins
add  x7, x6, x6         # 4
