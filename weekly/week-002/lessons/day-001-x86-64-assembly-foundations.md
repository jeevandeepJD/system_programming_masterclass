# Day 1 — x86-64 Assembly Foundations

**Curriculum alignment:** tracker topic 8, *x86-64 Assembly Foundations*

**Target time:** 2–3 hours: 70 minutes concept and tracing · 60 minutes
assembly/GDB lab · 30 minutes compiler annotation · 20 minutes mastery work

> Last week ended with instruction bytes changing architectural state. Today
> we stop looking at “the CPU” from a distance. If `RIP` points at one x86-64
> instruction, which state can that instruction read, what can it change, and
> how do we prove it one instruction at a time?

## Tracker contract

By the end you should be able to:

- read basic x86-64 assembly and track register values;
- explain memory operands and effective addresses;
- use GDB for instruction stepping;
- recognize general-purpose registers, moves, arithmetic, logic, flags, and
  basic branches;
- write three small routines, step through them, and annotate
  compiler-generated assembly.

The mastery checkpoint is exact:

> **Translate a simple C loop into a plausible assembly sequence.**

“Plausible” matters. A C statement has no unique assembly translation.

---

## 1. Why assembly exists between bytes and source

Machine-code bytes are excellent for a decoder and hostile to human memory.
Early programmers entered numeric operation codes directly, then created
*assemblers*: programs that map names such as `add`, labels such as `.loop`,
and register spellings such as `%rax` into encodings.

```text
human intent → C → compiler → assembly text → assembler → machine bytes
                                  ↑
                         today's working level
```

Assembly is not what the CPU reads. It is a textual notation for a particular
ISA plus assembler features. Labels disappear into addresses or relocations;
comments disappear; mnemonics become bytes.

Why descend to this level?

- A debugger stops at an instruction address even when source is unavailable.
- The kernel's entry paths and context switches must manipulate registers
  explicitly.
- A compiler bug, ABI violation, crash dump, or performance mystery is often
  visible only after source has been lowered.
- Virtualization saves and restores architectural state—the same state named
  here.

The goal is not to memorize the x86 manual. It is to learn to execute a small
sequence on paper and verify the model on Linux.

## 2. Architectural state: the visible machine

### General-purpose registers

x86-64 exposes sixteen 64-bit general-purpose registers:

```text
rax rbx rcx rdx   rsi rdi rbp rsp   r8 r9 r10 r11 r12 r13 r14 r15
```

They are not physically guaranteed to be sixteen simple storage boxes inside
a modern core. Register renaming may map them onto a larger internal file.
They are *architectural names*: software-visible state the implementation
must preserve.

Conventional roles come from instructions and the ABI:

- `rsp` is the architectural stack pointer used implicitly by `push`, `pop`,
  `call`, and `ret`;
- `rbp` can be a frame pointer, but is otherwise a general register;
- `rax` often carries integer return values;
- `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9` carry the first six integer/pointer
  arguments under the System V AMD64 ABI.

Only `rsp`'s special instruction behavior belongs to the ISA. Argument and
return roles are ABI agreements, studied tomorrow.

### Subregisters and the zero-extension rule

Parts of a register have historical names:

```text
rax  64 bits
eax  low 32 bits
 ax  low 16 bits
 al  low  8 bits
 ah  bits 8–15 (legacy; avoid in new 64-bit examples)
```

Writing a 32-bit register zeros its upper half:

```asm
movl $1, %eax       # RAX becomes 0x0000000000000001
```

Writing `ax` or `al` preserves the other bits. Predicting this distinction is
part of register tracking.

### `RIP`: where fetch continues

`RIP` names the next architectural instruction location. Ordinary execution
advances it by the current variable-length instruction's size. A branch,
call, return, exception, interrupt, or system call replaces that ordinary
next value.

You normally cannot use `mov %rax, %rip`. Control-transfer instructions update
it according to defined rules. x86-64 also supports RIP-relative memory
addressing, vital for position-independent code:

```asm
mov value(%rip), %eax
```

### `RFLAGS`: recorded conditions

Arithmetic and logical instructions update selected status bits. The most
useful today are:

- `ZF`: result was zero;
- `SF`: high result bit was one;
- `CF`: unsigned carry/borrow condition;
- `OF`: signed overflow;
- `PF` and `AF`: historically useful conditions that still exist.

Flags are state, not a hidden Boolean attached permanently to a value. Another
flag-writing instruction replaces them.

## 3. Reading AT&T syntax without guessing

GNU tools often display AT&T syntax:

```asm
mnemonic source, destination
movq     %rsi, %rax
addq     $7, %rax
```

Conventions:

- `%` introduces a register;
- `$` introduces an immediate constant;
- suffix `b/w/l/q` means 1/2/4/8-byte operation;
- source precedes destination;
- parentheses denote memory addressed through registers.

Intel syntax commonly puts destination first and omits `%`/`$`. Both are
textual views of the same possible bytes. Always state which one you use.
This week uses AT&T in source and requests Intel flavor when helpful in GDB.

## 4. Data movement and memory operands

### `mov` copies bits; it does not “move” an object

```asm
movq %rsi, %rax       # copy 64 bits RSI → RAX
movl (%rdi), %eax     # read 4 bytes at address RDI; zero upper RAX
movq %rax, 8(%rdi)    # write 8 bytes at address RDI+8
```

The source remains unchanged. A load is memory → register; a store is
register → memory. Ordinary x86 `mov` does not permit both operands to be
arbitrary memory locations.

### Effective addresses

The general integer address form is:

```text
displacement(base, index, scale)
effective address = displacement + base + index × scale
scale ∈ {1,2,4,8}
```

Example:

```asm
movl 12(%rdi,%rsi,4), %eax
```

If `rdi=0x1000`, `rsi=3`, the effective address is
`0x1000 + 3×4 + 12 = 0x1018`. Four bytes are loaded from that address.

Do not conflate calculation with access. `lea 12(%rdi,%rsi,4), %rax`
calculates `0x1018` and performs **no load**. Translation and permissions are
consulted only when an actual memory access occurs.

### Draw the path

```text
instruction fields + base/index register values
                    ↓ address-generation logic
             virtual effective address
                    ↓ translation + permission + hierarchy
                 bytes or fault
```

That last arrow connects assembly to Linux: a user load can trigger a page
fault; the kernel may map a page and restart the same instruction.

## 5. Arithmetic and logic

```asm
addq %rsi, %rax       # rax = rax + rsi
subq $1, %rcx         # rcx = rcx - 1
imulq %rdx, %rax      # low 64-bit signed product into rax
andq $0xff, %rax      # preserve low byte
orq  %rsi, %rax
xorl %eax, %eax       # eax=0, therefore rax=0
shlq $3, %rax         # shift left three positions
```

Most have a read-modify-write destination and update flags. The width is part
of the operation. A 32-bit `addl` wraps modulo \(2^{32}\) and zeros the upper
half of its destination register; `addq` works modulo \(2^{64}\).

Signedness is not stored in a register. `add` produces the same bit sum for
signed and unsigned interpretations; later code chooses `OF`-based or
`CF`-based conditions.

## 6. Compare, test, and branch

`cmpq %rsi, %rdi` computes `rdi-rsi` for flags and discards the result.

```asm
cmpq %rsi, %rdi
je   .equal            # ZF=1
jl   .signed_less      # SF != OF
jb   .unsigned_below   # CF=1
```

`testq %rax, %rax` computes an AND for flags without storing it. It is a
common zero/sign test.

Trace this loop with `n=3` in `rdi`:

```asm
xorl %eax, %eax        # sum = 0
xorl %ecx, %ecx        # i = 0
.loop:
cmpq %rdi, %rcx        # i - n
jge  .done             # signed i >= n?
addq %rcx, %rax        # sum += i
incq %rcx              # i++
jmp  .loop
.done:
ret
```

Create a table with columns `RIP`, `RAX`, `RCX`, `RDI`, `ZF/SF/OF`, and “next
instruction.” A loop is not one event. It is repeated state transitions and a
backward next-`RIP` choice.

## 7. Lab — write, predict, step

Use:

```text
weekly/week-002/challenges/day-001-routines.s
weekly/week-002/challenges/day-001-driver.c
```

Build safely on Fedora x86-64:

```bash
cd /home/jd/Desktop/masterclass
gcc -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g \
  -fno-omit-frame-pointer \
  weekly/week-002/challenges/day-001-driver.c \
  weekly/week-002/challenges/day-001-routines.s \
  -o /tmp/week2-day1
/tmp/week2-day1
```

Before running, hand-trace:

1. `asm_sum_to(4)`;
2. `asm_scale_add(10, 3, -2)`;
3. `asm_count_byte(bytes, count, 0x2a)`.

The files contain TODO prompts, not solutions to the extension tasks.

### GDB instruction stepping

```bash
gdb -q /tmp/week2-day1
```

```gdb
set disassembly-flavor intel
break asm_sum_to
run
display/i $pc
display/x $rax
display/x $rcx
display/x $rdi
si
info registers rip rax rcx rdi eflags
x/8i $pc
```

Use `si` for one machine instruction; `ni` steps over calls. Stop immediately
before and after `cmp`, then before and after its conditional branch. Record
the relevant values and whether `RIP` became the target or fall-through.

For the byte-count routine:

```gdb
break asm_count_byte
run
x/12bx $rdi
si
```

When a load executes, compute its address before asking GDB:

```gdb
p/x $rdi+$rcx
x/1bx $rdi+$rcx
```

### Annotate compiler-generated assembly

The driver contains `c_sum_to`. Generate assembly at two optimization levels:

```bash
gcc -std=c17 -Wall -Wextra -O0 -S \
  weekly/week-002/challenges/day-001-driver.c -o /tmp/day1-O0.s
gcc -std=c17 -Wall -Wextra -O2 -S \
  weekly/week-002/challenges/day-001-driver.c -o /tmp/day1-O2.s
gcc -std=c17 -Wall -Wextra -O2 -g \
  weekly/week-002/challenges/day-001-driver.c \
  weekly/week-002/challenges/day-001-routines.s -o /tmp/day1-O2
objdump -d -Mintel --disassemble=c_sum_to /tmp/day1-O2
```

Annotate input register, loop state, compare, backward edge, and return
register. If `-O2` transforms the loop, explain equivalence rather than
searching for a one-to-one source line.

## 8. Kernel and virtualization connection

On exception or syscall entry, Linux assembly saves architectural registers
into a kernel-defined frame. On return it restores enough state for user
execution. A scheduler context switch preserves task state; KVM's vCPU state
similarly models guest registers such as `RIP`, `RSP`, and `RFLAGS`.

This does **not** mean every internal CPU register is saved. Rename maps,
reorder-buffer entries, and speculative work are microarchitectural. The
architectural boundary is exactly why an OS or hypervisor can reason about
the machine.

## 9. Mastery — Explain · Draw · Observe · Build

### Explain

Without notes:

- register versus subregister, including a 32-bit write;
- `RIP`, `RSP`, `RBP`, and `RFLAGS`;
- load versus store versus `lea`;
- effective address calculation;
- why signed and unsigned branches differ;
- why assembly text is not machine code.

### Draw

Draw one loop iteration:

```text
RIP → fetch/decode → register reads → optional address/memory
    → ALU result/flags → register write → sequential or branch next RIP
```

### Observe

Capture one GDB trace with:

- instruction bytes/text;
- before/after registers;
- effective address of one load;
- flags after `cmp`;
- branch's next `RIP`.

### Build

Without looking at compiler output first:

1. write a plausible assembly sequence for:

   ```c
   long sum_positive(const int *a, size_t n);
   ```

2. state its register/width assumptions;
3. assemble and call it from C;
4. test empty, mixed, and all-negative arrays;
5. only then compare with `gcc -O2 -S`.

That completes the tracker's “three routines,” debugger stepping, compiler
annotation, and loop-translation evidence.

### Why? notebook

1. Why does writing `eax` clear the upper half of `rax`?
2. Why is `rbp` sometimes a frame pointer and sometimes ordinary storage?
3. Why does `lea` use memory-address syntax without accessing memory?
4. Why can a valid load still enter the kernel?
5. Why does `cmp a,b` become `b-a` in AT&T syntax?
6. Why can two compilers emit different correct loops?

## End model

```text
assembly names an ISA operation
  → instruction reads architectural registers/immediates
  → optional effective address identifies memory
  → arithmetic/logical work changes result and flags
  → ordinary sequencing or a branch chooses next RIP
  → GDB lets us freeze and inspect each boundary
```

## References used selectively

Local primary companion:

- Bryant and O'Hallaron, *Computer Systems: A Programmer's Perspective*,
  3rd ed. global edition, Chapter 3: §§3.2–3.6, book pages 205–273
  (encodings, registers/operands, data movement, arithmetic, condition codes,
  jumps, loops). Local PDF:
  `source-materials/library/books/01-foundations-and-architecture/computer-systems-programmers-perspective-bryant-ohallaron-3e-global.pdf`

Current official references:

- Intel, *Intel® 64 and IA-32 Architectures Software Developer's Manual*,
  Vol. 1 Chapters 3 and 7, and Vol. 2 instruction reference:
  <https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html>
- GDB current manual, “Registers,” “Machine Code,” and “Stopping and
  Continuing”: <https://sourceware.org/gdb/current/onlinedocs/gdb/>
- Linux x86 entry documentation, connecting architectural registers to
  kernel entry: <https://docs.kernel.org/arch/x86/entry_64.html>

**Next bridge:** instructions define what `call` and `ret` do, but not where
arguments arrive or which registers survive. Those promises belong to an ABI.
