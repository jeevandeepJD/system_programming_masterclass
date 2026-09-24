# Day 2 — Frames and Contracts

**Curriculum alignment:** Stage 3 — Assembly, ABI, and Program Representation ·
Stack Frames and Calling Conventions

**Target time:** approximately 3 hours

- The problem that forced a stack to exist: about 25 minutes
- call, ret, and the anatomy of a frame: about 45 minutes
- The System V AMD64 ABI as a contract: about 40 minutes
- Labs: frames, the seventh argument, a broken promise, exhaustion: about 55 minutes
- Unwinding, kernel stacks, and mastery work: about 35 minutes

> Yesterday's routines took arguments in `rdi` and `rsi` and returned in
> `rax`. Nothing in the instruction set said they had to.
>
> `call` pushes a return address and jumps. `ret` pops and jumps. That is the
> entire architectural contribution to function calls. Everything else —
> which register holds argument one, who is allowed to destroy `rbx`, where
> local variables live, how a debugger reconstructs a backtrace — is
> agreement, not hardware.
>
> Today is about that agreement, and about what happens when someone breaks
> it.

---

## 1. Why a subroutine needs somewhere to put the return address

Start where the hardware starts. Tiny-8 had no `call`. If you wanted the same
sequence of instructions used from two places, you copied it. That works, and
it was genuinely how early programs were written, but it wastes storage and
it means fixing a bug twice.

The fix seems obvious: jump to the shared code, and jump back afterwards. The
difficulty is the word *back*. A jump instruction contains a fixed target.
The shared routine does not know which of its callers is running.

The first widely used answer stored the return address *inside* the routine.
On the EDSAC in the late 1940s, David Wheeler's technique — still called the
Wheeler jump — had the caller place a return-jump instruction into a slot at
the end of the subroutine before transferring control. The routine literally
modified itself to know where to go home to. Some later machines gave this
hardware support: a call instruction that stored the return address into a
fixed location associated with the routine.

That design has one fatal property. Consider:

```text
main calls compute
compute calls compute          (directly or through helper)
```

The second call overwrites the return address slot with an address inside
`compute`. When the inner call returns, control comes back correctly. When
the outer call tries to return, the slot no longer holds `main`'s address.
The program returns into itself, forever.

So the requirement is sharper than "store the return address." It is:

> Store a return address **per activation**, not per routine, and reclaim
> that storage automatically when the activation ends.

Activations nest perfectly: the most recently started call is always the
first to finish. A structure whose last item in is the first item out is a
stack. This is not a coincidence or an implementation convenience — the
nesting discipline of function calls *is* stack discipline, and the data
structure follows from it.

Algol 60 made this unavoidable by specifying recursion as a language feature,
and by the mid-1960s a hardware-supported stack with a dedicated stack
pointer register was normal. x86-64 inherits that directly: `rsp`, `push`,
`pop`, `call`, `ret`.

### The same stack solves three problems at once

Once you have per-activation storage, you get more than return addresses:

```text
per-activation return address    →  nested and recursive calls work
per-activation local variables   →  each call gets its own copies
per-activation saved registers   →  a callee can borrow a register and
                                    give it back
```

All three live in the same region, and together they are called a **stack
frame** or **activation record**.

---

## 2. What `call` and `ret` actually do

These two instructions are simpler than their reputation.

```text
call target       rsp ← rsp - 8
                  memory[rsp] ← address of the instruction after the call
                  rip ← target

ret               rip ← memory[rsp]
                  rsp ← rsp + 8
```

That is all. `call` is a push followed by a jump. `ret` is a pop into the
program counter.

Two consequences follow immediately, and both matter.

**First:** `ret` does not verify anything. It jumps to whatever eight bytes
sit at `rsp`. If your code wrote over those bytes, `ret` transfers control
there with complete confidence. This is the mechanism behind stack-smashing
attacks, and it is also why a corrupted stack produces a crash at an address
with no relationship to the bug.

**Second:** the stack pointer is the only thing tying an activation to its
storage. There is no header, no length field, no allocator metadata. The
discipline is entirely in the generated code.

### The stack grows downward

`push` *decreases* `rsp`. New frames occupy lower addresses than their
callers.

```text
higher addresses
    ┌────────────────────────┐
    │  environment, argv     │
    ├────────────────────────┤
    │  main's frame          │  ← started here
    ├────────────────────────┤
    │  descend(5) frame      │
    ├────────────────────────┤
    │  descend(4) frame      │
    ├────────────────────────┤
    │  descend(3) frame      │  ← rsp points into the newest frame
    ├────────────────────────┤
    │                        │
    │  unmapped              │  ← the kernel maps more on demand,
    │                        │     up to RLIMIT_STACK
lower addresses
```

Why downward? Historically, because putting the stack at the top of the
address space and the heap just above the program's data let the two grow
toward each other and share whatever was left, without either needing to know
the other's size in advance. On a machine with megabytes of address space
that mattered. On a 64-bit machine with a 47-bit user address space it no
longer does, but the convention is baked into instruction semantics — `push`
subtracts, and that is not something you can change.

You can confirm the direction in the lab. `descend(5)` prints a frame address
for each recursion level, and every level is 96 bytes *below* the last:

```text
  depth  5  frame 0x7ffd58a01ca0  return 0x000000400ad0  delta    -
  depth  4  frame 0x7ffd58a01c40  return 0x000000400663  delta   96
  depth  3  frame 0x7ffd58a01be0  return 0x000000400663  delta   96
  depth  2  frame 0x7ffd58a01b80  return 0x000000400663  delta   96
  depth  1  frame 0x7ffd58a01b20  return 0x000000400663  delta   96
```

Notice the return addresses, too. The outermost activation was called from
`main` at `0x400ad0`; the other four were all called from the same recursive
call site at `0x400663`. Four activations, one piece of code.

---

## 3. What is inside a frame

Here is `descend`'s prologue exactly as GCC 16 generated it at `-O0` with
frame pointers on this machine:

```text
descend:
  push   rbp                   ; save the caller's frame pointer
  mov    rbp,rsp               ; establish ours
  push   rbx                   ; borrow a callee-saved register
  sub    rsp,0x48              ; carve out 72 bytes of locals
```

Add it up, from one `rbp` to the caller's `rbp`:

```text
8   the return address pushed by call
8   the saved rbp
8   the saved rbx
72  locals and spill space (0x48)
--
96  bytes -- exactly the delta the lab measured
```

The general shape, drawn with higher addresses at the top:

```text
                                 higher addresses
    ┌──────────────────────────┐
    │  argument 8              │  pushed by the caller, if any
    │  argument 7              │
    ├──────────────────────────┤
    │  return address          │  ← pushed by call
    ├──────────────────────────┤
    │  saved rbp               │  ← rbp points here
    ├──────────────────────────┤
    │  saved rbx, r12..r15     │  only the ones this function uses
    ├──────────────────────────┤
    │  local variables         │
    │  spilled temporaries     │
    │  outgoing argument area  │
    └──────────────────────────┘  ← rsp
                                 lower addresses
```

With `rbp` established, every local has a fixed negative offset from `rbp`
and every stack argument has a fixed positive offset, regardless of how `rsp`
moves afterwards. That is the entire point of a frame pointer.

### Reading it in GDB

```bash
gdb -q /tmp/day-002-stack-frames-lab
(gdb) break descend
(gdb) run
(gdb) continue 3
(gdb) backtrace
(gdb) info frame
```

On this machine that produces:

```text
#0  descend (depth=2) at day-002-stack-frames-lab.c:61
#1  0x0000000000400663 in descend (depth=3) at day-002-stack-frames-lab.c:81
#2  0x0000000000400663 in descend (depth=4) at day-002-stack-frames-lab.c:81
#3  0x0000000000400663 in descend (depth=5) at day-002-stack-frames-lab.c:81
#4  0x0000000000400ad0 in main (argc=1, ...) at day-002-stack-frames-lab.c:249

Stack level 0, frame at 0x7fffffffdb70:
 rip = 0x40059c in descend; saved rip = 0x400663
 called by frame at 0x7fffffffdbd0
 Arglist at 0x7fffffffdb60, args: depth=2
 Locals at 0x7fffffffdb60, Previous frame's sp is 0x7fffffffdb70
 Saved registers:
  rbx at 0x7fffffffdb58, rbp at 0x7fffffffdb60, rip at 0x7fffffffdb68
```

Read that last block carefully. GDB is telling you the exact byte addresses
where this activation stored the caller's `rbx`, the caller's `rbp`, and the
return address. Three consecutive eight-byte slots, in the order the prologue
pushed them. This is not GDB being clever — it is GDB reading a table the
compiler emitted, which section 8 comes back to.

---

## 4. The ABI: a contract nobody can enforce

The instruction set says nothing about argument passing. So why does a
function compiled by GCC work when called from code compiled by Clang, or
from a library shipped as a binary five years ago, or from Rust, or from a
hand-written `.s` file?

Because they all follow the same written agreement: the **System V AMD64
ABI**. An Application Binary Interface is the set of conventions that make
separately compiled code interoperable. It is the reason `libc` is a single
file rather than a per-compiler build.

The parts you need today:

### Integer and pointer arguments

```text
argument 1  rdi
argument 2  rsi
argument 3  rdx
argument 4  rcx
argument 5  r8
argument 6  r9
argument 7+ pushed on the stack, rightmost first, so that argument 7
            is at 8(%rsp) on entry to the callee
```

Floating-point arguments go in `xmm0`–`xmm7`, counted separately. A function
can therefore have six integer and eight floating-point arguments all in
registers.

### Return values

```text
integer or pointer      rax  (and rdx for a 128-bit value)
floating point          xmm0
large structs           the caller passes a hidden pointer in rdi to
                        space it allocated; the callee fills it in
```

### Who owns which register

This is the part people get wrong, so state it precisely.

```text
callee-saved ("the callee must give these back unchanged")
    rbx  rbp  r12  r13  r14  r15  rsp

caller-saved ("a call may destroy these; save them yourself if you care")
    rax  rcx  rdx  rsi  rdi  r8  r9  r10  r11
```

The split is an economic compromise. If every register were callee-saved, a
tiny leaf function would pay to preserve registers nobody was using. If every
register were caller-saved, a function with a long-lived value in a register
would pay at every call. Splitting them lets the compiler put short-lived
values in caller-saved registers and long-lived ones in callee-saved
registers, and spill only what is genuinely live across a call.

### Stack alignment

`rsp` must be a multiple of 16 **immediately before** a `call` executes.
Since `call` pushes eight bytes, the callee sees `rsp ≡ 8 (mod 16)` on entry.

The lab confirms this directly with a two-instruction routine:

```text
  rsp inside asm_read_rsp: 0x7ffd58a01ce8
  rsp % 16 = 8
```

Why require alignment at all? Because SSE instructions that load sixteen
bytes at once were historically much faster — and in some forms required —
when the address was sixteen-byte aligned. Making it an ABI rule means a
function can spill a vector register to its own frame without checking
anything at runtime. It is a small tax paid everywhere to remove a check from
the hot path.

This is also the single most common way hand-written assembly breaks
mysteriously. Push an odd number of registers before a `call` and the callee
may fault inside a library function that has nothing to do with your code.
The lab's `asm_probe_rbx` pushes exactly three registers for this reason, and
the comment says so.

### The red zone

A leaf function — one that calls nothing — may use the 128 bytes *below*
`rsp` without adjusting `rsp` at all. The ABI guarantees that signal handlers
and interrupts will not clobber that region.

This saves two instructions in small functions and is a nice example of an
ABI making a promise that costs the kernel something: Linux's signal delivery
code must skip the red zone when it builds a signal frame on the user stack.
Kernel code itself is compiled with `-mno-red-zone`, because an interrupt
arriving in kernel mode would happily scribble on it.

---

## 5. Lab A — frames, arguments, and a promise broken on purpose

Two files:

```text
weekly/week-002/challenges/day-002-abi-probe.s
weekly/week-002/challenges/day-002-stack-frames-lab.c
```

Build with frame pointers so part 4 has a chain to follow:

```bash
cd weekly/week-002/challenges

gcc -std=c17 -Wall -Wextra -O0 -g -fno-omit-frame-pointer \
  day-002-abi-probe.s day-002-stack-frames-lab.c \
  -o /tmp/day-002-stack-frames-lab

/tmp/day-002-stack-frames-lab
```

Predict before running:

1. Will successive `descend` frame addresses increase or decrease?
2. What will `rsp % 16` be inside `asm_read_rsp`?
3. `asm_seven` reads its seventh argument from `8(%rsp)`. What is at
   `0(%rsp)`, and why must the routine not push anything before that read?
4. `asm_sum_squares` and `asm_bad_sum_squares` compute the same sum. Which
   observable difference will the probe find?

### The one that matters

Part 5 is the heart of the lab. Two routines, identical arithmetic, and one
of them omits `push %rbx` / `pop %rbx`. Both return the right answer:

```text
  asm_sum_squares(5)     = 55
  asm_bad_sum_squares(5) = 55
  rbx after the well-behaved callee: 0x5a5a5a5a5a5a5a5a (marker intact)
  rbx after the careless callee:     0x0000000000000006 (CLOBBERED)
```

Sit with that. The broken routine is *correct* by any test that looks at
return values. It produces the right number every time. It breaks its caller,
silently, and only if the caller happened to be keeping something in `rbx`.

This is what ABI violations look like in practice. Not a crash at the point
of the mistake — a wrong value somewhere else, in code that did nothing
wrong, possibly only in an optimized build where the compiler chose to use
`rbx`, possibly only on one compiler version. The whole reason to learn the
callee-saved list is so you never generate this bug.

### Then break it yourself

Add a fifth routine to the `.s` file that uses `r12` as a scratch register
without saving it, and call it through `asm_probe_rbx` after modifying the
probe to plant a marker in `r12` instead. Predict what you will see before
you run it.

---

## 6. Lab B — running out of stack

Run the second mode:

```bash
/tmp/day-002-stack-frames-lab --overflow
```

A child process recurses until the kernel stops it. On this machine:

```text
  RLIMIT_STACK soft limit: 8388608 bytes (8192 KiB)
  child reached depth 27557
  first frame 0x7ffcd5df44d0, last frame 0x7ffcd55f7210
  stack consumed 8377024 bytes, about 303 bytes per frame
  child terminated by signal 11 (Segmentation fault)
```

Check the arithmetic yourself: 27557 × 303 ≈ 8.35 MB, against a limit of
8.39 MB. The numbers close.

### What the kernel actually did

The main thread's stack is not 8 MiB of physical memory sitting there
waiting. It is a single virtual memory area that the kernel extends as needed:

1. The process touches an address just below the current stack VMA.
2. The MMU finds no mapping and raises a page fault — a synchronous
   exception, exactly the mechanism Week 1 Day 7 described.
3. The kernel's fault handler recognizes the address as a plausible stack
   extension and maps another page.
4. Execution resumes at the faulting instruction.

That loop runs tens of thousands of times in this demonstration and you never
see it. Step 3 has a limit, though: `RLIMIT_STACK`. Once the VMA has grown to
that size, the next fault is not a growth request, it is an error, and the
kernel delivers `SIGSEGV`.

Note what did *not* happen: the stack did not silently run into the heap.
Modern kernels keep a guard gap below the stack VMA so that a large stack
allocation cannot jump over the guard and land in unrelated mapped memory —
a real class of vulnerability, patched in 2017 and known as Stack Clash.
This is also why GCC has `-fstack-clash-protection`, which forces large stack
allocations to touch each page as they grow so the guard cannot be skipped.

### Try the variations

```bash
(ulimit -s 1024; /tmp/day-002-stack-frames-lab --overflow)
(ulimit -s unlimited; /tmp/day-002-stack-frames-lab --overflow)
```

Predict the depth each time before you run it. Then check whether the kernel
logged anything:

```bash
dmesg | tail
```

and compare against a `coredumpctl list` entry if core dumps are enabled on
your system.

---

## 7. Recursion is not the only thing that consumes stack

A frame's size is set at compile time by the largest set of simultaneously
live locals, spills, and outgoing arguments. Three things make frames large
enough to matter:

- **Large local arrays.** `char buffer[65536];` inside a function allocates
  64 KiB per activation. Eight nested calls like that exhaust a default
  thread stack.
- **Variable-length arrays and `alloca`.** These move `rsp` by a value the
  compiler does not know. If the length comes from untrusted input, the
  program can move `rsp` past the guard page in one step. This is why the
  Linux kernel removed VLAs from its own source tree.
- **Inlining.** A function that inlines three callees may need a frame as
  large as all four combined.

The kernel side is stricter still. A Linux kernel stack on x86-64 is
`THREAD_SIZE` — 16 KiB, four pages — and it **does not grow**. There is no
fault handler that can extend it, because the fault would arrive with no
stack to handle it on. That single fact explains a family of kernel coding
rules: no recursion, no large stack buffers, no VLAs, and separate stacks for
interrupt and exception handling so a deep call chain interrupted at the
wrong moment cannot overflow. `CONFIG_VMAP_STACK` adds a guard page below
each kernel stack so an overflow faults instead of quietly corrupting a
neighbouring task's data.

When you eventually write kernel code, "how much stack does this path use"
becomes a question you have to be able to answer. Today's lab is where the
instinct starts.

---

## 8. How a backtrace is reconstructed

The lab's part 4 walks the stack by hand, following saved `rbp` values:

```text
  manual walk from walk_level_two:
    level 0  rbp 0x7fffa2bc9640  return 0x000000400728
    level 1  rbp 0x7fffa2bc9650  return 0x000000400734
    level 2  rbp 0x7fffa2bc9660  return 0x000000400bb1
    level 3  rbp 0x7fffa2bc96a0  return 0x7f83884ab681
    level 4  rbp 0x7fffa2bc9750  return 0x7f83884ab798
    level 5  rbp 0x7fffa2bc97b0  return 0x000000400455
    chain ends here
```

Each frame's saved `rbp` points at the caller's `rbp`, and the return address
sits eight bytes above it. That is a linked list through the stack, and
walking it is twelve lines of C.

It only works if every function on the path maintained a frame pointer.
Modern compilers default to `-fomit-frame-pointer` at `-O1` and above,
because `rbp` is then available as a seventeenth general-purpose register —
worth a few percent on register-hungry code. The lab is built with
`-fno-omit-frame-pointer` specifically so the walk succeeds; try removing
that flag and watch it stop after one level.

So how does GDB produce a backtrace for optimized code with no frame
pointers? From a table. The compiler emits **DWARF Call Frame Information**
into the `.eh_frame` section: for every instruction address in the function,
a description of where the return address and each saved register can be
found relative to the current `rsp` or `rbp`. Look at it:

```bash
readelf --debug-dump=frames-interp /tmp/day-002-stack-frames-lab | head -40
objdump --dwarf=frames-interp /tmp/day-002-stack-frames-lab | head -40
```

You will see rows that say, in effect, "from this address onward, the
canonical frame address is `rsp+16`, and `rbx` is at CFA−24." That is the
same information the frame-pointer chain encodes, except it is a table
instead of runtime data, it is accurate at every instruction including in the
middle of a prologue, and it costs zero registers.

The trade-off is that reading it is expensive. This is the live argument
behind Linux's `CONFIG_UNWINDER_FRAME_POINTER` versus
`CONFIG_UNWINDER_ORC`: the kernel wanted reliable stack traces for `perf` and
for live patching without paying the frame-pointer register cost, so it
generates its own compact unwind format (ORC) at build time rather than
parsing DWARF in the kernel. And it is why `perf record --call-graph fp` and
`--call-graph dwarf` give you different results on the same binary.

---

## 9. The return address is data, and that has consequences

One more implication of section 2 worth stating explicitly, because it
connects this lesson to a large part of systems security.

The return address sits on the stack, immediately above the local variables,
at a position the compiler chose. A local array is written forward, toward
higher addresses. If code writes past the end of that array, the next things
it reaches are the saved registers and then the return address.

```text
    ┌──────────────────────┐
    │  return address      │  ← ret jumps here, no questions asked
    ├──────────────────────┤
    │  saved rbp           │
    ├──────────────────────┤
    │  char buffer[64]     │  ← overflows upward, into the two above
    └──────────────────────┘
```

The mitigations you will meet are all attempts to break one link in that
chain:

- **Stack canaries** (`-fstack-protector-strong`) place a random value
  between the locals and the saved registers, and check it before `ret`.
- **Non-executable stack** (the `.note.GNU-stack` marker in the lab's `.s`
  files, and `PT_GNU_STACK` in the ELF header) stops the overwritten address
  from pointing at attacker-supplied instructions on the stack.
- **ASLR** randomizes the stack base so the attacker does not know the
  address to write. You can see this working: run the lab twice and compare
  the frame addresses.
- **Shadow stacks** (Intel CET, and `arch_prctl` support in recent Linux)
  keep a second, protected copy of return addresses and compare on `ret`.

Note that a plain `gcc` invocation on this machine enables none of these by
default:

```bash
gcc -Q --help=common | grep stack-protector
nm /tmp/day-002-stack-frames-lab | grep -i stack_chk
```

Fedora's *package* builds add them through distribution build flags, which is
a useful distinction to understand: hardening is a policy applied by whoever
invokes the compiler, not a property of the compiler.

Rebuild the lab with `-fstack-protector-strong` and diff the disassembly of
`descend`. You will find two new instructions in the prologue and three in
the epilogue, and now `nm` shows `__stack_chk_fail`.

---

## 10. Mastery checkpoint

The tracker checkpoint is:

> **Explain function calls mechanically, identify arguments, return values,
> and preserved registers, draw a stack frame, and explain recursion and
> stack overflow.**

### Explain

Without notes:

- why storing a return address per *routine* fails, and per *activation*
  works;
- exactly what `call` and `ret` do to `rsp` and `rip`;
- why the stack grows toward lower addresses, and whether that is a hardware
  fact or a convention;
- the difference between caller-saved and callee-saved, and why the ABI has
  both instead of one rule;
- what `rsp % 16` is on entry to a callee, and why the ABI demands alignment
  at all;
- how a frame pointer chain differs from DWARF call-frame information, and
  what each costs;
- why a Linux kernel stack cannot grow the way a user stack does.

### Draw

Draw `descend`'s frame with byte offsets, labelling:

- the return address and which instruction placed it;
- the saved `rbp` and which instruction placed it;
- the saved `rbx`;
- the 72 bytes of locals;
- where `rbp` points and where `rsp` points;
- the arrow from this frame's saved `rbp` to the caller's `rbp`.

Then draw the same function's frame as it would look if compiled with
`-fomit-frame-pointer`, and mark what a manual walker loses.

### Observe

1. In GDB, break on `descend`, `continue 3`, and run `backtrace`,
   `info frame`, `frame 2`, `info args`. Confirm that GDB's "Saved registers"
   addresses match what the manual walk printed.
2. `x/6gx $rbp` and identify, by eye, the saved `rbp` and the return address.
3. Run the program twice and compare stack addresses. Explain the difference.
4. Run `--overflow` under three different `ulimit -s` values and show that
   depth scales with the limit.
5. `readelf --debug-dump=frames-interp` on the binary; find the entry for
   `descend` and match one row against the prologue instructions.

### Build

1. Add `asm_clobbers_r12` to the probe file and detect it.
2. Write `asm_seven` yourself, from scratch, without looking at the existing
   version, and get it passing.
3. Write a hand-written assembly function that calls `printf`. Getting the
   alignment right is the exercise; getting it wrong is instructive.
4. Modify `descend` to declare `char pad[4096];` and re-measure the frame
   delta. Predict the new number first.
5. Rebuild with `-fstack-protector-strong` and identify every added
   instruction.

### Why? notebook

1. Why does the Wheeler-jump approach break under recursion but work fine for
   non-recursive programs?
2. Why is `rsp` in the callee-saved list when no function explicitly saves
   it?
3. Why does the seventh argument live at `8(%rsp)` and not `0(%rsp)`?
4. Why is the red zone safe from signal handlers but not from kernel
   interrupts?
5. Why does a broken callee-saved register produce a bug far from its cause?
6. Why does `-O2` usually omit the frame pointer, and what did the kernel do
   instead of accepting that trade-off?
7. Why can a VLA with an untrusted length skip a guard page when a fixed
   array cannot?
8. Why does the lab's overflow demonstration need a child process?
9. If `ret` jumps wherever `rsp` points, why do programs not crash constantly?

---

## Mental model at the end

```text
caller has a value it needs after the call
        ↓ is it in a caller-saved register?
        ↓ yes → caller spills it to its own frame first
caller places arguments: rdi rsi rdx rcx r8 r9, then the stack
        ↓ rsp is a multiple of 16 at this instant
call
        ↓ pushes the return address, rsp now ≡ 8 (mod 16)
callee prologue
        ↓ push rbp; mov rbp,rsp        (if it keeps a frame pointer)
        ↓ push any callee-saved register it intends to use
        ↓ sub rsp, N                   (locals, spills, outgoing args)
callee body
        ↓ locals at negative offsets from rbp
        ↓ stack arguments at positive offsets from rbp
callee epilogue
        ↓ restores rsp, pops the callee-saved registers it pushed
ret
        ↓ pops the return address into rip
caller continues, and every callee-saved register holds what it did before
```

The instruction set contributes two lines of that diagram. Everything else is
a contract, written down in a document, obeyed by every compiler, and
enforced by nothing at all.

That is worth carrying forward. Tomorrow you meet a different instruction set
with a different register file and a different ABI — and you will find that
the *shape* of that diagram is nearly identical, because the problem it
solves has not changed.

---

## References used selectively

- *System V Application Binary Interface, AMD64 Architecture Processor
  Supplement*, sections 3.2 (function calling sequence), 3.2.2 (the stack
  frame), 3.2.3 (parameter passing), and Figure 3.4 (the register-usage
  table). This is the authoritative document for everything in section 4:
  <https://gitlab.com/x86-psABIs/x86-64-ABI>
- Bryant and O'Hallaron, *Computer Systems: A Programmer's Perspective*, 3rd
  ed., section 3.7, "Procedures," for the runtime stack, transferring
  control, argument passing, local storage, and recursion. Local PDF:
  `source-materials/library/books/01-foundations-and-architecture/computer-systems-a-programmers-perspective-3rd-edition.pdf`
- Michael Kerrisk, *The Linux Programming Interface*, Chapter 6, for the
  process memory layout and the relationship between the stack, `RLIMIT_STACK`,
  and virtual memory areas.
- Linux kernel documentation and source for `THREAD_SIZE`,
  `CONFIG_VMAP_STACK`, and the ORC unwinder
  (`Documentation/arch/x86/orc-unwinder.rst`), read for orientation.
- DWARF Debugging Information Format, Version 5, Section 6.4, "Call Frame
  Information," for what `.eh_frame` contains:
  <https://dwarfstd.org/>

Every address, frame size, recursion depth, and GDB transcript quoted above
was captured on this machine with GCC 16.2.1 and GDB 17.2. Stack addresses
change on every run because of ASLR; frame *sizes* will match if you use the
same compiler and flags.
