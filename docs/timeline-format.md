# Timeline JSON format (version 1)

`rvsim --json` and the backend's `POST /api/simulate` return the whole pipeline run as one JSON document. The
simulator runs the program once and records every cycle; a viewer shows cycle N by reading `cycles[N - 1]`, without
running anything again. The document holds data only: ids, registers, values and enum names. The viewer writes any
explanation sentences ("x2 is forwarded from the load in MEM/WB") from these fields.

`rvasm --json` and `POST /api/assemble` return the [program document](#program-document), which uses the same
instruction objects.

All numbers are plain JSON numbers. Register and memory values are unsigned 32-bit (0 to 4294967295); convert to
signed or hex in the viewer. Cycles are 1-based, instance ids and instruction ids are 0-based.

## Top level

```jsonc
{
  "format": "rv-timeline",
  "version": 1,
  "ok": true,
  "model": "pipeline",
  "program": { "instructions": [ /* Instruction */ ] },
  "instances": [ /* Instance */ ],
  "cycles": [ /* Cycle, cycles[i].cycle == i + 1 */ ],
  "outcome": { /* Outcome */ },
  "stats": { /* Stats */ },
  "final": { "registers": [ /* 32 values, x0..x31 */ ], "memory": { "bytes": 256, "words": [ /* 64 words */ ] } }
}
```

If the program does not assemble, the document is instead:

```json
{ "format": "rv-timeline", "version": 1, "ok": false,
  "error": { "stage": "assemble", "diagnostics": [ { "line": 2, "column": 14, "message": "immediate '99999' does not fit in 12 bits" } ] } }
```

## Instruction (static, once per program)

One entry per word of the program, at address `pc = 4 * id`.

| Field | Meaning |
| --- | --- |
| `id` | Index in the program. Instances refer to it. |
| `pc` | Byte address. |
| `word` | Encoding as a hex string, e.g. `"0x00500093"`. |
| `source` | `{ "line", "column" }` of the mnemonic in the source (1-based), or `null` for machine-code input. The editor highlights this line while the instruction is in a stage. |
| `valid` | `false` for a word that is not an RV32I instruction. Then only `id`, `pc`, `word`, `source` are present. |
| `text` | Disassembly, e.g. `"LW x5, -4[x1]"`. |
| `mnemonic`, `kind`, `format` | e.g. `"LW"`, `"load"`, `"I"`. Kinds: `alu`, `alu-immediate`, `shift-immediate`, `load`, `store`, `branch`, `lui`, `auipc`, `jal`, `jalr`, `system`. |
| `rd`, `rs1`, `rs2` | Register numbers, or `null` when the instruction does not use that field. |
| `imm` | Sign-extended immediate (`lui`/`auipc`: already shifted left by 12), or `null`. |
| `aluOp` | `add`, `sub`, `and`, `or`, `xor`, `sll`, `srl`, `sra`, `slt`, `sltu`, the branch comparisons `eq`, `ne`, `lt`, `ge`, `ltu`, `geu`, and `pass-b` (LUI). |
| `control` | Control signals generated in ID: `regRead1`, `regRead2`, `aluSrc`, `regWrite`, `memRead`, `memWrite`, `memToReg`, `branch`, `jump`, `halt`. |

## Instance (dynamic, once per fetch)

Every fetch creates an instance. A loop body run three times is three instances, so the same source line can sit
in two stages at once with different ids. Stage slots, forwarding, stalls and flushes refer to instances.

| Field | Meaning |
| --- | --- |
| `id` | Instance id. |
| `instruction` | Static instruction id, or `null` for a fetch outside the program (it faults). |
| `pc` | Fetch address. |
| `occurrence` | 1 for the first fetch of this instruction, 2 for the second... ("3rd execution of line 5"). Squashed fetches count. |
| `fetchCycle` | Cycle in which IF first held it. |
| `fate` | `retired`, `flushed` (squashed by a taken branch/jump), `discarded` (younger than a halt or fault), `faulted`, `halted`, or `in-flight` (the cycle limit stopped the run). |
| `endCycle` | Cycle of that fate; 0 for `in-flight`. |

## Cycle

What each stage worked on during the cycle, what moved between stages, and what was written.

```jsonc
{
  "cycle": 6,
  "pc": 16,                                   // fetch address at the start of the cycle
  "stages": { "IF": Slot, "ID": Slot, "EX": Slot, "MEM": Slot, "WB": Slot },
  "forwarding": [ Forward ],
  "stall": Stall | null,
  "flush": Flush | null,
  "execute": Execute | null,
  "memory": Memory | null,
  "registerWrite": RegisterWrite | null,
  "nextPc": { "source": "pc+4", "value": 20 }
}
```

### Slot

| Value | Meaning |
| --- | --- |
| `null` | Nothing there: the pipeline is filling or draining. |
| `{ "instance": 3 }` | Instance 3 is in this stage. |
| `{ "instance": 3, "stalled": true }` | Held in place by a load-use stall (only IF and ID). |
| `{ "bubble": "stall" }` | A bubble inserted by a load-use stall. It starts in EX and moves down. |
| `{ "bubble": "flush" }` | The hole left by an instruction squashed after a taken branch or jump. |

### Forward

A value sent into EX from a later pipeline register instead of the stale value read from the register file in ID.

```json
{ "operand": "rs1", "from": "EX/MEM", "register": 1, "value": 5, "producer": 0, "consumer": 1 }
```

`from` is `EX/MEM` (the instruction one ahead) or `MEM/WB` (two ahead). `producer` computed the value, and
`consumer` is the instance in EX.

### Stall

```json
{ "waiting": 2, "load": 1, "register": 2 }
```

The instance in ID (`waiting`) needs `register`, which the load in EX (`load`) has not read from memory yet. ID and
IF keep their instructions, the PC is not written, and a `stall` bubble enters EX in the next cycle.

### Flush

```json
{ "by": 4, "squashed": [5, 6], "target": 8 }
```

The branch or jump `by` resolved in EX and redirected fetch to `target`. The instances fetched (IF) and decoded (ID)
in this cycle are squashed; `squashed` can be empty when nothing was fetched behind it. The next cycle shows
`flush` bubbles in ID and EX.

### Execute

The EX datapath for the instance in EX.

```json
{ "instance": 2, "op": "add", "a": 0, "aSource": "rs1", "b": 5, "bSource": "rs2", "result": 5,
  "branch": null, "target": null }
```

| Field | Meaning |
| --- | --- |
| `a`, `b`, `result` | ALU inputs (after forwarding) and output. For a branch, `result` is 1 when the condition holds. For loads and stores, `result` is the address. |
| `aSource` | `rs1`, or `pc` for AUIPC and JAL (the A-input mux). |
| `bSource` | `rs2`, or `imm` when ALUSrc selects the immediate. |
| `branch` | `{ "taken": true/false }` for conditional branches, otherwise `null`. |
| `target` | Next PC when a branch is taken or for a jump, otherwise `null`. |

### Memory

```json
{ "instance": 7, "access": "store", "address": 8, "width": 4, "value": 4294967294, "previous": 0 }
```

`width` is 1, 2 or 4 bytes. For a store, `value` is the stored bytes and `previous` the bytes it overwrote. For a
load, `value` is the result after sign or zero extension, and there is no `previous`.

### RegisterWrite

The register file write in WB; drives "changed this cycle" highlighting. Writes to x0 are not recorded.

```json
{ "instance": 0, "register": 1, "value": 5, "previous": 0, "source": "alu" }
```

`source` is the MemToReg mux: `alu`, `memory` (loads) or `link` (the return address of JAL/JALR).

### nextPc

The PC mux at the end of the cycle: `pc+4`, `branch-target`, `jump-target`, `hold` (load-use stall: the PC is
not written) or `stopped` (fetching has run past the program). `value` is the PC for the next cycle.

## Outcome

```json
{ "status": "faulted", "cycle": 6, "instance": 1, "haltPc": null,
  "fault": { "kind": "load-out-of-range", "pc": 4, "address": 256 } }
```

| `status` | Meaning |
| --- | --- |
| `finished` | Ran past the last instruction and drained. |
| `halted` | An ECALL/EBREAK (`instance`, at `haltPc`) reached MEM after every older instruction completed. |
| `faulted` | `instance` faulted in MEM; older instructions completed, younger ones were discarded. |
| `cycle-limit` | Stopped after `maxCycles`; instances still in the pipeline are `in-flight`. |

Fault kinds: `illegal-instruction` (`address` is the instruction word), `fetch-out-of-range`, `fetch-misaligned`
(`pc` and `address` are the bad fetch address), `load-out-of-range`, `load-misaligned`, `store-out-of-range`,
`store-misaligned`. Data memory is `final.memory.bytes` bytes, and accesses must be naturally aligned.

## Stats

```json
{ "cycles": 19, "instructions": 11, "cpi": 1.7273, "stalls": 2, "flushes": 2, "redirects": 1 }
```

`stalls` counts load-use stall cycles, `flushes` squashed instructions, and `redirects` taken branches and jumps.

## State at cycle N

Registers and memory start at zero. The state at the end of cycle N is the initial state with every
`registerWrite` and every store `memory` record of cycles 1 to N applied, in order. Each record also carries
`previous`, so a viewer stepping backwards can undo one cycle without replaying from the start. After the last
cycle, the result equals `final`.

## Program document

```json
{ "format": "rv-program", "version": 1, "ok": true, "instructions": [ /* Instruction */ ] }
```

On assembly errors: `{ "format": "rv-program", "version": 1, "ok": false, "error": { "stage": "assemble", "diagnostics": [...] } }`.
