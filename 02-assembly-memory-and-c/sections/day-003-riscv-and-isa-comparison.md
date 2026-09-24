# Day 3 — RISC-V, Seen Through a Decoder

**Curriculum alignment:** tracker Week 10, *RISC-V Assembly and ISA
Comparison*

**Target time:** 2–3 hours: 65 minutes building the RV64I model · 40 minutes
comparing equivalent sequences · 50 minutes decoder/trace laboratory ·
20 minutes mastery evidence

> Yesterday, x86-64 `call` placed a return address on the stack and `ret`
> removed it. Today a RISC-V function call will place the return address in a
> register, and `ret` will turn out not to be a separate base instruction at
> all.
>
> The software problem did not change. The architectural choices did.

## Tracker contract

By the end, you should be able to:

- read basic RV64I assembly and track integer registers;
- explain the special behavior of `x0`;
- distinguish register arithmetic from explicit loads and stores;
- recognize the common R, I, S, B, U, and J 32-bit forms;
- reconstruct signed immediates, including a branch immediate whose bits are
  not adjacent in the instruction;
- compare RISC-V and x86-64 without reducing either to “simple” or “complex”;
- relate ISA choices to compiler output, kernel entry, and virtual CPU state;
- decode and execute a small RISC-V loop without requiring a cross-toolchain.

The tracker mastery checkpoint says:

> **Explain why RISC-V is easier to reason about while x86 retains
> compatibility advantages.**

That sentence is a useful starting claim, not a verdict. We will make
“easier,” “compatibility,” and the costs on both sides precise.

---

## 1. The same problem, a deliberately different contract

An ISA must let software:

```text
name working state
    → transform values
    → move values between registers and memory
    → choose a next instruction
    → enter and return from functions
```

x86-64 and RISC-V both solve that list. They allocate encoding space,
architectural state, and implementation work differently.

RISC-V began at UC Berkeley in 2010, after decades of experience with earlier
RISC designs. Its designers were not trying to rediscover the smallest
possible CPU. They wanted an open, extensible ISA suitable for research,
teaching, tiny embedded implementations, and high-performance systems. The
base integer ISA is intentionally small; multiplication, atomics,
floating-point, vectors, compressed encodings, and other facilities are
organized as extensions.

“RISC-V” therefore does not name one fixed collection of every possible
instruction. `RV64I` means:

```text
RV       RISC-V
64       64-bit integer register width and address-space model
I        base integer instruction set
```

A real Linux-capable machine supports additional extensions. The lesson
starts with RV64I because it exposes the architectural skeleton clearly.

### Three objects that must stay separate

```text
RISC-V specification   defines encodings and architectural behavior
instruction word       32 bits fetched from one particular program
RISC-V processor       hardware or a software model implementing the contract
```

The Python challenge is in the third category: it is a deliberately partial
software implementation. It can demonstrate decode and architectural state
transitions. It says nothing about how many pipeline stages, execution units,
or cycles a physical processor uses.

---

## 2. Thirty-two integer registers, and one that refuses to change

RV64I has 32 integer registers, each 64 bits wide:

```text
x0 x1 x2 ... x31
```

Five bits can name one of 32 registers because \(2^5=32\). That fact will
become visible in every common 32-bit instruction format.

All 32 names are architectural. A high-performance implementation may use
register renaming and many more physical registers internally, just as an
x86-64 implementation may. The ISA promises the behavior of `x0`–`x31`, not
one literal bank of 32 flip-flop arrays.

### `x0`: read zero, discard writes

`x0` always reads as zero. A write to `x0` has no architectural effect:

```asm
addi x5, x0, 12      # x5 = 0 + 12
add  x0, x5, x6      # addition occurs architecturally; result is discarded
```

The second instruction does **not** permanently make `x0` nonzero. The next
read still yields zero.

Why spend one of 32 encodings on a fixed value?

It lets ordinary instruction forms express several common operations without
dedicated opcodes:

```asm
addi x5, x0, 7       # load small constant 7
sub  x6, x0, x5      # negate x5
addi x7, x5, 0       # copy x5
jal  x0, target      # jump, discarding the link address
jalr x0, 0(x1)       # return through x1, discarding a new link
beq  x5, x0, empty   # compare with zero
```

Assemblers provide pseudoinstructions such as `li`, `mv`, `neg`, `j`, and
`ret` for these patterns. A pseudoinstruction is assembler vocabulary, not
necessarily an extra opcode in the ISA.

There is a cost: only 31 registers can retain arbitrary values. The design
judgment is that a permanently available zero simplifies enough common
sequences and hardware operand selection to justify that cost.

### Two names for the same register

RISC-V assembly normally uses ABI names:

```text
x0  zero              constant zero
x1  ra                return address
x2  sp                stack pointer
x5–x7, x28–x31 t0–t6  caller-saved temporaries
x8–x9, x18–x27 s0–s11 callee-saved values
x10–x17 a0–a7         arguments; a0/a1 also return values
```

`x8` is called `s0` and may also be displayed as `fp` when used as a frame
pointer. The names do not create extra storage:

```text
x10 = a0
x11 = a1
x1  = ra
```

The ISA assigns the number. The ELF psABI assigns the conventional role.
RISC-V hardware does not reject a program for putting an argument in `t3`;
separately compiled functions simply will not interoperate unless they obey
the same ABI.

### Retrieval check

Without looking back:

1. How many bits are needed to name one integer register?
2. What value does `x0` return after an attempted write?
3. Is `a0` a second register in addition to `x10`?
4. Which facts above belong to the ISA, and which belong to the ABI?

---

## 3. Load/store means arithmetic names registers, not memory

RV64I integer arithmetic uses register operands. Memory is reached through
explicit load and store instructions.

```asm
ld   t0, 0(a0)       # read 8 bytes at address a0+0 into t0
addi t0, t0, 7       # t0 = t0 + 7
sd   t0, 0(a0)       # write low 8 bytes of t0 to address a0+0
```

That sequence implements the essential work of:

```c
*p += 7;
```

The address form is:

```text
effective address = integer register rs1 + sign-extended 12-bit immediate
```

The immediate is measured in bytes. `24(sp)` means an address 24 bytes above
the current stack pointer; it does not mean register 24.

### Load width and extension are part of the operation

RV64I has several integer loads:

```text
lb   load 1 byte, sign-extend to 64 bits
lbu  load 1 byte, zero-extend to 64 bits
lh   load 2 bytes, sign-extend
lhu  load 2 bytes, zero-extend
lw   load 4 bytes, sign-extend
lwu  load 4 bytes, zero-extend
ld   load 8 bytes
```

Stores `sb`, `sh`, `sw`, and `sd` write the low 1, 2, 4, or 8 bytes of the
source register. A store does not first sign-extend the source. It selects
low bits and transfers those bytes.

Predict:

```text
memory[0x100] = 0xff
a0 = 0x100
```

After `lb t0, 0(a0)`, `t0` is `0xffffffffffffffff`.
After `lbu t1, 0(a0)`, `t1` is `0x00000000000000ff`.

The memory byte is identical. The instruction tells the machine how to widen
it.

### Address construction is visible

Suppose `a0` is a `uint64_t *base` and `a1` is an array index:

```asm
slli t0, a1, 3       # byte offset = index × 8
add  t0, a0, t0      # address = base + byte offset
ld   a0, 0(t0)       # load the element
```

The load itself has only base-plus-12-bit-displacement addressing. Scaling
and adding the index happen in earlier register operations.

x86-64 can put this calculation in one memory operand:

```asm
movq (%rdi,%rsi,8), %rax
```

The RISC-V sequence makes intermediate values explicit. The x86 instruction
makes a common address calculation compact. Neither observation by itself
tells us the number of cycles. A RISC-V implementation can execute the shift
and add efficiently; an x86 implementation can decode its instruction into
internal work. Cache behavior can dominate both.

### A load is not “just copying bytes”

Architecturally, the load describes an effective virtual address and result.
In a Linux process:

```text
rs1 + immediate
    → virtual address
    → address translation and permission check
    → cache/memory access, or a page/access/alignment fault
    → destination register updated only if the instruction completes
```

This is the same causal path studied on x86-64. The syntax changed. The need
for translation, protection, restartable faults, and precise architectural
state did not.

---

## 4. Six common 32-bit forms

In the base ISA, the ordinary instruction length is 32 bits. The low seven
bits hold the major opcode. Register fields stay in stable locations where a
format uses them:

```text
rd   bits 11:7
rs1  bits 19:15
rs2  bits 24:20
```

That regularity lets a decoder begin extracting register numbers before it
has resolved every operation detail.

The optional compressed `C` extension adds 16-bit encodings. Longer
instruction encodings are also defined by RISC-V's variable-length framework.
Therefore “every RISC-V instruction is 32 bits” is false for RISC-V in
general. It is accurate for the base RV64I forms studied here when compressed
and other variable-length extensions are excluded.

### R-type — three register operands

```text
31       25 24    20 19    15 14  12 11     7 6       0
+----------+--------+--------+------+---------+---------+
| funct7   | rs2    | rs1    |funct3| rd      | opcode  |
+----------+--------+--------+------+---------+---------+
```

Examples:

```asm
add t0, t1, t2       # t0 = t1 + t2
sub a0, a0, a1
and t3, t3, t4
slt a0, a0, a1      # signed less-than produces 0 or 1
```

The seven-bit major opcode identifies an operation family. `funct3` and
`funct7` distinguish operations within that family. “Opcode” is therefore
often used informally for more than one encoded field.

### I-type — one register, one signed immediate

```text
31                20 19    15 14  12 11     7 6       0
+-------------------+--------+------+---------+---------+
| imm[11:0]         | rs1    |funct3| rd      | opcode  |
+-------------------+--------+------+---------+---------+
```

Examples:

```asm
addi sp, sp, -32     # allocate 32 stack bytes
andi a0, a0, 255
ld   ra, 24(sp)      # loads use an I-shaped encoding
jalr x0, 0(ra)       # indirect jump also uses I-type fields
```

For `addi`, the 12-bit immediate is sign-extended to the register width. Its
range is -2048 through +2047. `addi a0, zero, 4096` cannot be encoded as one
base instruction.

Shift-immediate operations reuse some upper immediate bits to distinguish
logical from arithmetic shifts. A field diagram is a family pattern, not a
license to treat every bit as data.

### S-type — store, with a split immediate

```text
31       25 24    20 19    15 14  12 11      7 6       0
+----------+--------+--------+------+----------+---------+
|imm[11:5] | rs2    | rs1    |funct3| imm[4:0] | opcode  |
+----------+--------+--------+------+----------+---------+
```

Example:

```asm
sd ra, 24(sp)
```

A store needs:

- `rs1`: address base;
- `rs2`: value to write;
- immediate: address displacement.

It does not need `rd`, because a store has no integer destination register.
The five bits normally occupied by `rd` become part of the immediate. This is
encoding budgeting made visible.

### B-type — compare two registers and branch

```text
31      30:25 24:20 19:15 14:12 11:8    7 6:0
+---------+-----+-----+-----+------+-------+---+-------+
|imm[12]  |10:5 | rs2 | rs1 |funct3| imm[4:1]|11|opcode|
+---------+-----+-----+-----+------+-------+---+-------+
```

The exact drawing is crowded because the immediate is rearranged:

```text
imm[12]   comes from instruction bit 31
imm[10:5] comes from bits 30:25
imm[4:1]  comes from bits 11:8
imm[11]   comes from bit 7
imm[0]    is implicitly zero
```

Examples:

```asm
beq  t0, zero, done
bne  a0, a1, retry
blt  t0, t1, loop       # signed comparison
bltu t0, t1, loop       # unsigned comparison
```

Why scramble the immediate instead of placing it in one neat range? The
register fields and `funct3` remain in the same positions as related formats.
The immediate bits are arranged so hardware can reconstruct the displacement
while preserving those stable positions. The low target bit is known to be
zero under the base alignment model, so it need not consume an encoded bit.

This is not beautiful for a human decoder. It is a trade: regular register
field placement and encoding overlap versus contiguous immediate notation.

### U-type — twenty upper bits

```text
31                                      12 11     7 6:0
+-----------------------------------------+---------+-------+
| immediate[31:12]                        | rd      |opcode |
+-----------------------------------------+---------+-------+
```

`lui` places the 20 encoded bits into bits 31:12 and zeros the low 12 bits.
On RV64, the 32-bit result is sign-extended to 64 bits.

`auipc` adds that upper immediate to the current PC. Combined with another
instruction, it supports PC-relative address formation over a much larger
range than a 12-bit immediate.

### J-type — jump and link

`jal rd, offset` writes `pc+4` to `rd` and transfers to `pc+offset`. Like the
branch immediate, its encoded displacement is rearranged and has an implicit
zero low bit.

```asm
jal ra, function      # ordinary direct call: save return PC in ra
jal x0, loop          # jump: discard return PC
```

The link destination is explicit. `jal` is not inherently “a call” or “a
jump”; the destination register and software convention give it that role.

### Why fixed 32-bit forms are attractive

For this subset, a front end knows:

```text
next sequential PC = current PC + 4
```

Field positions are regular, and several candidate values can be extracted
in parallel. This can simplify teaching, implementation, verification, and
binary rewriting.

But fixed width does not make design constraints disappear:

- every simple operation still consumes four bytes;
- three 5-bit registers consume 15 of the 32 bits;
- large constants require more than one instruction;
- a large address calculation often takes several instructions;
- extensions need reserved encoding space or longer/alternative forms.

The compressed extension exists partly because code density matters to
instruction-cache capacity, fetch bandwidth, memory footprint, and energy.

---

## 5. Immediates: small constants and displaced control flow

An immediate is a bit field inside the instruction, not a miniature memory
object. The decoder reconstructs it according to that instruction's format.

### Sign extension

For:

```asm
addi a0, a0, -1
```

the immediate field contains:

```text
12-bit representation: 111111111111
sign-extended to 64:    111...111111111111
numeric value:          -1
```

The ALU adds that 64-bit pattern modulo \(2^{64}\). Registers do not carry a
permanent “signed” tag.

### Why one constant may need two instructions

A common conceptual sequence for a larger constant is:

```asm
lui  t0, upper20
addi t0, t0, lower12
```

The exact split must account for `addi` sign extension. If bit 11 of the low
part is set, the assembler may round the upper part and use a negative lower
immediate. This is why `li t0, 0x12345678` is a pseudoinstruction: the
assembler chooses a legal sequence rather than replacing one universally
defined `li` opcode.

For arbitrary 64-bit constants, more instructions may be needed.

### Branch targets

Suppose:

```text
branch PC = 0x0000000000000040
decoded immediate = -12
condition = true
```

Then:

```text
target = branch PC + (-12) = 0x34
```

The immediate is not an absolute address. It is a signed PC-relative byte
displacement. Its encoded low bit is implicit.

Do not silently substitute x86's branch rules. Both architectures commonly
use PC-relative control flow, but their encodings and defined PC bases must
be read from their own specifications.

---

## 6. Branches without an integer flags register

x86-64 often separates comparison from conditional transfer:

```asm
cmpq %rsi, %rdi
jl   .less
```

`cmp` writes condition state into `RFLAGS`; `jl` reads the relevant flags.

RV64I integer branches compare registers as part of the branch:

```asm
blt a0, a1, .less
```

The base integer architecture has no x86-like general condition-code register
for this sequence.

Why might a design choose each model?

**Flags can be useful because:**

- one comparison can feed a following condition;
- arithmetic instructions can naturally record carry, zero, sign, and
  overflow conditions;
- an existing architecture and its software already rely on them.

**Compare-and-branch can be useful because:**

- the data dependencies are explicit in one instruction;
- unrelated arithmetic does not accidentally overwrite an earlier condition;
- hardware and compilers need not track a shared integer flags register for
  ordinary branches.

But do not leap from this to “RISC-V branches are always faster.” A
microarchitecture chooses its own branch units, prediction, fusion, and
scheduling. Modern x86 implementations may macro-fuse common compare/branch
pairs. Architectural style influences implementation; it does not dictate a
single performance result.

### Signed and unsigned are selected by the instruction

Let:

```text
t0 = 0xffffffffffffffff
t1 = 0x0000000000000001
```

Then:

```asm
blt  t0, t1, target    # taken: signed -1 < signed 1
bltu t0, t1, target    # not taken: UINT64_MAX < 1 is false
```

The register bits did not change. The operation selected their
interpretation.

---

## 7. Calls: same nesting problem, different architectural primitive

RISC-V direct calls normally use:

```asm
jal ra, function
```

Architecturally:

```text
ra (x1) ← address of the instruction after jal
pc      ← current pc + signed displacement
```

An indirect call can use:

```asm
jalr ra, 0(t0)
```

A return is conventionally:

```asm
jalr x0, 0(ra)
```

which the assembler displays through the `ret` pseudoinstruction.

Compare yesterday's x86-64 mechanism:

```text
x86 call    pushes return address to memory at the stack
RISC-V jal  writes return address to an explicitly named register
```

The RISC-V choice makes a leaf call possible without touching memory for its
return address. But a non-leaf function will call another function, and that
new call overwrites `ra`. The current function must preserve its incoming
return address, commonly in its stack frame:

```asm
example:
    addi sp, sp, -16
    sd   ra, 8(sp)
    sd   s0, 0(sp)
    # ... body, perhaps another call ...
    ld   s0, 0(sp)
    ld   ra, 8(sp)
    addi sp, sp, 16
    ret
```

The exact frame is compiler- and function-dependent. The point is mechanical:
nesting still requires one preserved return address per live activation.
RISC-V starts that address in `ra`; software spills it when necessary.
x86-64 starts it on the stack.

### Calling-convention overview

Under the RISC-V ELF psABI:

- `a0`–`a7` carry integer/pointer arguments;
- `a0` and `a1` carry integer return values;
- `t0`–`t6`, `a0`–`a7`, and `ra` are not preserved across calls;
- `s0`–`s11` are callee-saved;
- `sp` must be restored and follows ABI alignment rules;
- `s0` may be a frame pointer, but a frame pointer is optional.

This resembles the System V AMD64 convention in purpose, not in exact
register names or counts. Both conventions answer the same economic
question: which side should pay to preserve a live value across a call?

---

## 8. Equivalent sequences, compared carefully

Use AT&T syntax for x86-64 below because Days 1–2 did. RISC-V syntax writes
the destination first. Count instructions, but do not confuse the count with
time.

### A. Return `*p + 7`

```c
long add_seven(const long *p);
```

x86-64 System V:

```asm
movq (%rdi), %rax
addq $7, %rax
ret
```

RV64I ELF psABI:

```asm
ld   a0, 0(a0)
addi a0, a0, 7
ret                       # jalr x0, 0(ra)
```

Both make the load and arithmetic distinct here. The argument and return
roles differ by ABI. The RISC-V `ret` spelling hides a base instruction
pattern; x86 `ret` is an architectural instruction with implicit `rsp`
behavior.

### B. Increment through a pointer

x86-64:

```asm
addq $1, (%rdi)
ret
```

RV64I:

```asm
ld   t0, 0(a0)
addi t0, t0, 1
sd   t0, 0(a0)
ret
```

The one x86 instruction specifies a memory read-modify-write. That does not
make it automatically atomic between cores; x86 has separate locking
semantics for atomic read-modify-write behavior. The three RISC-V
instructions make data movement explicit but likewise do not implement an
atomic C update. RISC-V atomics belong to an extension beyond RV64I.

### C. Return `base[index]`

x86-64:

```asm
movq (%rdi,%rsi,8), %rax
ret
```

RV64I:

```asm
slli a1, a1, 3
add  a0, a0, a1
ld   a0, 0(a0)
ret
```

x86 spends encoding and address-generation capability on a scaled indexed
memory form. RISC-V composes the calculation from register operations. The
compiler may schedule independent work around the explicit operations, while
the x86 form is compact in architectural instruction count and often in
bytes.

### D. A counted sum loop

C intent:

```c
long sum(const long *a, long n)
{
    long total = 0;
    for (long i = 0; i < n; ++i)
        total += a[i];
    return total;
}
```

Plausible RV64I body:

```asm
    addi t0, zero, 0       # i
    addi t1, zero, 0       # total
.loop:
    bge  t0, a1, .done
    slli t2, t0, 3
    add  t3, a0, t2
    ld   t4, 0(t3)
    add  t1, t1, t4
    addi t0, t0, 1
    jal  zero, .loop
.done:
    addi a0, t1, 0
    ret
```

A compiler can choose a pointer-walking loop instead:

```asm
    slli a1, a1, 3
    add  a1, a0, a1       # end pointer
    addi t0, zero, 0
.loop:
    bgeu a0, a1, .done
    ld   t1, 0(a0)
    add  t0, t0, t1
    addi a0, a0, 8
    jal  zero, .loop
.done:
    addi a0, t0, 0
    ret
```

Both are plausible. A real compiler may transform more aggressively when C
overflow, aliasing, vector extensions, and optimization settings permit.
Assembly comparison is about preserved semantics, not matching one canonical
answer.

### Comparison worksheet

For each pair above, record:

1. which instructions can access data memory;
2. how the effective address is formed;
3. where the immediate resides;
4. whether condition state is explicit in registers, implicit in flags, or
   consumed in a combined compare-and-branch;
5. which register role comes from the ABI;
6. which spelling is a pseudoinstruction;
7. what instruction count cannot prove.

---

## 9. RISC and CISC without the cartoon

The historical labels describe clusters of design choices, not two laws of
nature.

RISC-V's base integer ISA visibly favors:

- regular base encodings;
- a load/store operand model;
- many general-purpose registers;
- explicit destination registers;
- a small base with named extensions;
- instructions that compose larger operations.

x86-64 visibly preserves:

- variable-length encodings from one through fifteen bytes;
- arithmetic forms that can use one memory operand;
- rich effective-address calculations;
- implicit operands and architectural flags;
- compatibility with decades of deployed binaries and operating systems.

Now remove the caricatures.

**“RISC means one cycle per instruction” is false.** Loads miss caches;
division and vector operations take varying work; implementations may be
superscalar, out of order, speculative, and internally complex.

**“CISC means one instruction performs an entire program” is false.** Common
x86 compiler output is full of loads, stores, adds, compares, and branches.
Many implementations decode architectural instructions into internal
micro-operations and schedule them aggressively.

**“Modern x86 is really RISC inside” is too imprecise to teach from.**
Internal operations are implementation-specific, not public RISC-V
instructions. Intel and AMD may implement the same x86 instruction
differently while preserving its architectural result.

**“RISC-V is automatically faster or lower power” is false.** ISA regularity
can reduce some front-end and verification costs, especially in small
implementations. Performance and energy also depend on process technology,
cache hierarchy, prediction, execution width, physical design, software, and
workload.

### Why make different choices?

RISC-V had the opportunity to define a modular ISA with no obligation to run
old x86 binaries. Regular fields, explicit operands, and extensions improve
teachability, implementation freedom, and the ability to build anything from
a small core to a large one under an open specification.

x86's compatibility is not an accidental embarrassment. Existing operating
systems, applications, firmware, compilers, expertise, and validated
platforms are enormous economic assets. Preserving old architectural
behavior lets new processors run old binaries. The cost appears in encoding
constraints, front-end complexity, verification burden, and architectural
rules that cannot simply be redesigned.

The responsible comparison is:

```text
RISC-V: freedom from a particular legacy + regular base contract
        ↔ software ecosystem maturity and extension/profile choices

x86-64: extraordinary deployed compatibility + dense expressive encodings
        ↔ decode/verification complexity and inherited constraints
```

This is why RISC-V is often easier to reason about at the base-ISA level
while x86 retains compatibility advantages. “Easier to reason about” does
not mean “easy to build a competitive server CPU,” and “compatibility” does
not mean “no innovation is possible.”

---

## 10. CPU, kernel, and virtualization connections

### What a context switch must preserve

Linux can switch tasks only because each ISA defines architectural state.
For RISC-V that includes the program counter, integer registers, and relevant
privileged/control state; enabled extensions add state such as floating-point
or vector registers. The kernel does not save an implementation's reorder
buffer or branch-predictor table as if those were process registers.

On a trap from userspace, architecture-specific entry code saves a register
frame so C kernel code can inspect or modify the interrupted context. The
RISC-V Linux `pt_regs` layout is not the x86-64 `pt_regs` layout because the
architectural registers and trap mechanisms differ. The kernel's abstract
jobs—preserve state, identify the cause, service it, and return—remain.

### Why `x0` still matters in privileged code

The zero register is not only a convenience for application arithmetic.
Control-transfer forms can discard links by selecting `x0`; comparisons can
use it directly; privileged assembly can clear or initialize values without
first dedicating a register to zero. Its semantics remain architectural at
every privilege level.

### Virtualization

A hypervisor or emulator presents a virtual architectural machine. KVM's
RISC-V support must expose guest-visible registers and virtualize privileged
state according to the relevant specifications and extensions. QEMU can
decode guest RISC-V instruction words in software and update a modeled
machine even when the host CPU is x86-64.

Today's Python decoder uses the same high-level idea at toy scale:

```text
fetch 32-bit word from a PC-indexed map
    → inspect opcode/funct/register/immediate fields
    → read modeled registers or bytes
    → compute the required result
    → update modeled architectural state
```

It is not virtualization: there is no guest operating system, privilege
model, device model, translation cache, or complete ISA. But it reveals why
cross-architecture emulation is possible. The host need not physically
implement the guest ISA if software can reproduce its required visible
behavior.

---

## 11. Laboratory — decode and trace without a cross-toolchain

Use:

```text
02-assembly-memory-and-c/challenges/day-003-riscv-decoder.py
```

The script uses only Python's standard library. It decodes a substantial
teaching subset of RV64I and simulates its architectural effects.

From the repository root:

```bash
python3 02-assembly-memory-and-c/challenges/day-003-riscv-decoder.py demo
python3 02-assembly-memory-and-c/challenges/day-003-riscv-decoder.py selftest
```

The self-test checks representative R/I/S/B/U/J encodings, immediate edges,
the fixed-zero invariant, and the complete sum trace.

### Lab A — expose the fields

Start with:

```bash
python3 02-assembly-memory-and-c/challenges/day-003-riscv-decoder.py \
  fields 0x00b50633
```

Before reading the rendered decode:

1. mark bits 6:0, 11:7, 14:12, 19:15, 24:20, and 31:25;
2. convert each five-bit register field to decimal;
3. identify the format from the major opcode;
4. only then identify the operation.

Decode several words at once:

```bash
python3 02-assembly-memory-and-c/challenges/day-003-riscv-decoder.py decode \
  0xfff50513 0x00b50633 0x00513423
```

Add `--x-registers` to remove ABI aliases. Explain why the instruction has
not changed when `a0` becomes `x10`.

### Lab B — immediate reconstruction

```bash
python3 02-assembly-memory-and-c/challenges/day-003-riscv-decoder.py exercise
```

For exercises A–E, draw the appropriate format and reconstruct the immediate
by hand. For the branch:

```text
instruction bits → imm[12|10:5|4:1|11|0] → sign extension → PC addition
```

Then reveal:

```bash
python3 02-assembly-memory-and-c/challenges/day-003-riscv-decoder.py \
  exercise --answers
```

The answers intentionally show both `xN` and ABI register names.

### Lab C — trace signed versus unsigned branches

```bash
python3 02-assembly-memory-and-c/challenges/day-003-riscv-decoder.py trace branch
```

Before running, predict:

- the 64-bit pattern produced by `addi t0, zero, -1`;
- whether `blt t0,t1` is taken;
- whether `bltu t0,t1` is taken;
- final `a0` and `a1`;
- whether any instruction changes `x0`.

The trace reports PC before and after each instruction and every changed
register. Explain the result using bit interpretation, not “the register
became unsigned.”

### Lab D — trace a load/store loop

Run only the first eight dynamic instructions:

```bash
python3 02-assembly-memory-and-c/challenges/day-003-riscv-decoder.py \
  trace sum --limit 8
```

At the stop, record `t0`–`t4`, `t1`'s sum role, and the next PC. Then trace to
completion:

```bash
python3 02-assembly-memory-and-c/challenges/day-003-riscv-decoder.py trace sum
```

The mapped array is `[7, -2, 13, 5]`, so predict `a0` before running. For each
loop iteration annotate:

```text
index → byte offset → effective address → loaded element → accumulated sum
```

Only `ld` accesses data memory in this trace. Instruction fetch is also a
memory-system activity in a real CPU, but the lesson's phrase “data memory
access” distinguishes the explicit program operand from instruction fetch.

### Lab E — modify one architectural choice

Make one local experimental edit, then restore it:

1. remove the `if number != 0` condition in `write_reg`;
2. run `selftest`;
3. inspect the first failure;
4. explain which later sequences would become unreliable if `x0` could
   retain a value.

Or choose the encoding route:

1. add one new decode-only instruction to an unused pattern in this teaching
   model;
2. state which fields select it;
3. decide whether it needs `rd`, `rs1`, `rs2`, or an immediate;
4. explain what encoding resource you consumed.

The goal is not to invent a superior ISA in ten minutes. It is to discover
that every convenience needs fields, semantics, tool support, tests, and
compatibility rules.

### Optional real toolchain observation

If a RISC-V GCC or LLVM target happens to be installed, verify the examples.
Do not install one merely to complete this lesson.

Possible checks:

```bash
command -v riscv64-linux-gnu-gcc
clang --print-targets | grep -i riscv
```

If supported:

```bash
clang --target=riscv64-linux-gnu -O2 -S example.c -o /tmp/example-rv64.s
```

Sysroot or headers may be absent even when the backend exists. A freestanding
function with no includes is the simplest test. The Python simulator is the
required laboratory precisely because host toolchain availability should not
block architectural reasoning.

---

## 12. Mastery — Explain · Draw · Observe · Build

### Explain

Without notes, answer:

> Why is the RV64I base easier to decode on paper than an arbitrary x86-64
> instruction, and why does that not prove a RISC-V program is faster?

A complete answer should include:

1. common 32-bit base width and stable register-field positions;
2. explicit load/store traffic and three-register arithmetic;
3. fewer implicit integer operands and no general integer flags dependency;
4. x86 variable length, prefix/opcode/ModR/M/SIB/displacement/immediate
   possibilities, and inherited compatibility;
5. the distinction between architectural instruction count and
   microarchitectural execution;
6. caches, prediction, scheduling, code density, extensions, and workload.

Also explain:

- why `x0` is useful even though it cannot store a changing value;
- why an S-type instruction has no `rd`;
- why B-type immediate bits are rearranged;
- why `ret` can be a pseudoinstruction;
- why `ra` must be saved by a non-leaf function;
- why ABI register names are not ISA-mandated argument behavior.

### Draw

Draw one 32-bit word for each format:

```text
R  I  S  B  U  J
```

Color or mark:

- major opcode;
- operation-selecting function bits;
- each register;
- immediate bits;
- implicit low zero for branch/jump displacement.

Then draw:

```text
ELF/text bytes → PC fetch → 32-bit instruction → field extraction
    → register reads → ALU/address/branch → register or memory update
    → next PC
```

Beside it, draw an x86-64 fetch window containing instructions of unequal
length. The diagram should show a tradeoff, not a winner.

### Observe

Save evidence from the simulator:

1. one R-type field decode;
2. one negative I-type immediate;
3. one reconstructed S- or B-type split immediate;
4. the signed/unsigned branch trace;
5. one complete sum-loop iteration with effective address;
6. final `x0`, proving a write was discarded;
7. one invalid or unsupported word and the decoder's error.

For the last item, try a word with major opcode zero:

```bash
python3 02-assembly-memory-and-c/challenges/day-003-riscv-decoder.py decode 0x00000000
```

A decoder must reject unsupported/reserved encodings. It must not invent a
plausible instruction for every 32-bit pattern.

### Build

Complete all three:

1. Change the sum array and predict the trace's result before running it.
2. Add a second small program using `sd` followed by `ld`; prove byte
   addresses and little-endian storage in the simulator.
3. Write equivalent plausible x86-64 and RV64I sequences for:

   ```c
   long max(long a, long b);
   long sum4(const long *p);
   void replace(long *p, long old, long new_value);
   ```

For each, document instruction count, explicit memory instructions,
condition mechanism, and ABI registers. Do **not** rank performance from
source-line count.

This satisfies the tracker's hands-on requirement to implement the same
function conceptually in both ISAs, compare instruction count and clarity,
and document key differences even when a RISC-V assembler is unavailable.

### Why? notebook

1. Why does a fixed zero register save opcodes but cost storage capacity?
2. Why does a load/store ISA expose more address-calculation instructions?
3. Why can `lw` and `lwu` read the same four bytes and produce different
   64-bit values?
4. Why is a 12-bit immediate sign-extended instead of always zero-extended?
5. Why are store and branch immediates split across fields?
6. Why does a branch displacement have an implicit zero bit?
7. Why can RISC-V call a function without immediately touching the stack?
8. Why must a non-leaf function still preserve `ra`?
9. Why are `a0` and `x10` two names for one architectural register?
10. Why can one x86 instruction and three RISC-V instructions perform the
    same architectural task without implying a 3:1 runtime ratio?
11. Why is x86 compatibility an engineering benefit rather than merely
    “legacy baggage”?
12. Why can QEMU execute RISC-V guest instructions on an x86-64 host?

---

## End model

```text
software problem: compute, access memory, choose control flow, call
        ↓
RV64I provides 32 integer registers, with x0 fixed at zero
        ↓
32-bit base forms allocate fields among opcode/function/register/immediate
        ↓
loads and stores alone name data memory
        ↓
branches compare registers and add a signed displacement to PC
        ↓
jal/jalr write an explicit link register; the ABI makes x1 the return address
        ↓
compiler and ABI compose these primitives into functions
        ↓
CPU, kernel, emulator, or hypervisor preserves the specified architectural
behavior while choosing very different internal mechanisms
```

The mastery sentence, made precise:

> RV64I is comparatively easy to reason about because its base formats,
> register roles in each instruction, and load/store data movement are
> regular and explicit. x86-64 preserves a richer, variable-length
> architectural contract so decades of binaries and systems continue to
> work. Those facts describe software-visible contracts and engineering
> costs; they do not determine performance by themselves.

---

## References used selectively

Primary RISC-V specifications:

- RISC-V International, *The RISC-V Instruction Set Manual, Volume I:
  Unprivileged Architecture*, current ratified specification index. See
  “RV32I Base Integer Instruction Set” for register state, instruction
  formats, integer computation, control transfer, and loads/stores; and
  “RV64I Base Integer Instruction Set” for RV64 differences:
  <https://docs.riscv.org/reference/isa/unpriv/unpriv-index.html>
- RISC-V International, *RV32I Base Integer Instruction Set*, especially
  “Programmers' Model for Base Integer ISA,” “Base Instruction Formats,”
  “Immediate Encoding Variants,” “Integer Register-Immediate Instructions,”
  “Control Transfer Instructions,” and “Load and Store Instructions”:
  <https://docs.riscv.org/reference/isa/unpriv/rv32.html>
- RISC-V International, *RV64I Base Integer Instruction Set*, including
  sign-extension conventions and RV64 load forms:
  <https://docs.riscv.org/reference/isa/unpriv/rv64.html>
- RISC-V non-ISA specifications, *RISC-V ABIs Specification*, current
  `riscv-elf-psabi-doc`, especially “Integer Register Convention,” “Frame
  Pointer Convention,” “Procedure Calling Convention,” and stack alignment:
  <https://riscv-non-isa.github.io/riscv-elf-psabi-doc/>

Official x86 comparison sources:

- Intel, *Intel® 64 and IA-32 Architectures Software Developer's Manual*,
  Volume 1 for the programming environment and Volume 2, Chapter 2 for
  instruction format and instruction reference:
  <https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html>
- AMD, *AMD64 Architecture Programmer's Manual, Volume 1: Application
  Programming* and Volume 3: *General-Purpose and System Instructions*
  (publication 24594), for the programmer-visible model, instruction
  encoding, addressing, flags, and control transfer:
  <https://www.amd.com/en/search/documentation/hub.html#sortCriteria=%40amd_release_date%20descending&f-amd_archive_status=Active&f-amd_document_type=Programmer%20References>

Kernel and virtualization connection:

- Linux kernel documentation, RISC-V architecture index:
  <https://docs.kernel.org/arch/riscv/index.html>
- Linux KVM documentation, including architecture-specific API material:
  <https://docs.kernel.org/virt/kvm/index.html>

The official manuals are authoritative for architectural behavior. The
Python challenge intentionally implements only the subset named at the top of
the file and should not be used as a compliance oracle.
