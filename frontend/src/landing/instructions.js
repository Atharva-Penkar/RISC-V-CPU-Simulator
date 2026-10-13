// The instruction reference on the landing page: every instruction the assembler accepts, grouped as in
// the RISC-V specification, with the operand syntax this assembler uses.

/**
 * @typedef {object} InstructionGroup
 * @property {string} name
 * @property {string} syntax      operand pattern for the whole group
 * @property {string[]} mnemonics
 * @property {string} description
 * @property {string} example
 */

/** @type {InstructionGroup[]} */
export const INSTRUCTION_GROUPS = [
  {
    name: "Register arithmetic and logic",
    syntax: "op rd, rs1, rs2",
    mnemonics: ["add", "sub", "and", "or", "xor", "sll", "srl", "sra", "slt", "sltu"],
    description: "Two registers in, one out. Shifts use the low 5 bits of rs2; slt/sltu write 1 or 0.",
    example: "add x3, x1, x2",
  },
  {
    name: "Immediate arithmetic and logic",
    syntax: "op rd, rs1, imm",
    mnemonics: ["addi", "andi", "ori", "xori", "slti", "sltiu"],
    description: "A register and a 12-bit signed immediate (−2048 to 2047).",
    example: "addi x1, x0, -5",
  },
  {
    name: "Immediate shifts",
    syntax: "op rd, rs1, shamt",
    mnemonics: ["slli", "srli", "srai"],
    description: "Shift by a constant from 0 to 31.",
    example: "slli x2, x1, 4",
  },
  {
    name: "Multiply and divide (M extension)",
    syntax: "op rd, rs1, rs2",
    mnemonics: ["mul", "mulh", "mulhsu", "mulhu", "div", "divu", "rem", "remu"],
    description:
      "mul gives the low 32 bits of the product, mulh* the high 32 bits. Division rounds toward zero; dividing by zero does not trap (quotient −1, remainder = dividend).",
    example: "mul x3, x1, x2",
  },
  {
    name: "Loads",
    syntax: "op rd, imm[rs1]",
    mnemonics: ["lb", "lh", "lw", "lbu", "lhu"],
    description: "Read 1, 2 or 4 bytes at rs1 + imm. lb/lh sign-extend, lbu/lhu zero-extend.",
    example: "lw x2, 8[x1]",
  },
  {
    name: "Stores",
    syntax: "op rs2, imm[rs1]",
    mnemonics: ["sb", "sh", "sw"],
    description: "Write the low 1, 2 or 4 bytes of rs2 at rs1 + imm.",
    example: "sw x2, 0[x0]",
  },
  {
    name: "Branches",
    syntax: "op rs1, rs2, label",
    mnemonics: ["beq", "bne", "blt", "bge", "bltu", "bgeu"],
    description: "Compare two registers and jump to the label when the condition holds. Resolved in EX.",
    example: "bne x1, x0, loop",
  },
  {
    name: "Jumps",
    syntax: "jal rd, label · jalr rd, rs1, imm",
    mnemonics: ["jal", "jalr"],
    description: "Jump and write the return address (pc + 4) to rd. Use x0 as rd to jump without linking.",
    example: "jal x1, function",
  },
  {
    name: "Upper immediates",
    syntax: "op rd, imm20",
    mnemonics: ["lui", "auipc"],
    description: "lui loads a 20-bit value into the upper bits; auipc adds it to the pc.",
    example: "lui x1, 0x12345",
  },
  {
    name: "System",
    syntax: "op",
    mnemonics: ["ecall", "ebreak"],
    description: "Both stop the program once every older instruction has completed.",
    example: "ebreak",
  },
];

/** Keyboard shortcuts of the simulator, as the transport bar implements them. */
export const SHORTCUTS = [
  { keys: ["Space", "K"], action: "Play or pause" },
  { keys: ["→", "L"], action: "Next cycle" },
  { keys: ["←", "J"], action: "Previous cycle" },
  { keys: ["Shift + → / ←"], action: "Ten cycles forward or back" },
  { keys: ["Home", "End"], action: "First or last cycle" },
];
