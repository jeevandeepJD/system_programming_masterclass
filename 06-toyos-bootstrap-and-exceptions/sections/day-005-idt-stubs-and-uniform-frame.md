# Day 5 — IDT, Exception Stubs, and a Uniform Saved Frame

**Target time:** approximately 3 hours

> x86 exceptions do not all push the same fields. How can one C handler receive
> one predictable structure without losing the hardware's meaning?

ToyOS combines architecture-created state with assembly-created state. The
`struct interrupt_frame` is correct only while its fields match the exact
stack layout produced by both.

---

## 1. From vector to handler

The runtime IDT has 256 entries. Week 6 installs gates for vectors 0–31 and
also prepares vector 32 for Week 7's timer. Each entry includes:

- low/middle/high pieces of the handler address;
- kernel code selector `0x08`;
- an IST index;
- attributes `0x8e`;
- a reserved zero field.

`0x8e` describes a present ring-0 interrupt gate. Gate encoding is x86-64
architecture. The chosen handlers, selector, and IST assignment are ToyOS.

`lidt` installs a descriptor containing the IDT base and byte limit. Building
an array is insufficient; the architecture consults the table selected by
`IDTR`.

## 2. Hardware entry is not the C ABI

On exception entry without a privilege-level change, the processor supplies
at least saved `RIP`, `CS`, and `RFLAGS`. Some vectors also push an error code.
A privilege or stack transition changes the frame further. Consult current
architecture manuals for exact vector-specific behavior.

C compilers do not automatically understand that raw frame. An ordinary C
function expects its platform ABI. Therefore each assembly stub normalizes
the stack and `isr_common` saves general registers before calling C.

## 3. Normalizing error-code differences

For a vector without a CPU error code, `ISR_NOERR` pushes:

```text
synthetic error = 0
vector number
```

For a vector where the CPU already pushed an error code, `ISR_ERR` pushes only
the vector number. In both cases, the top of the pre-register frame becomes:

```text
vector, error, rip, cs, rflags
```

This is a normalization layer. The synthetic zero must never be mistaken for
proof that hardware supplied an error code of zero.

Maintaining the correct vector list is critical. If a stub wrongly adds or
omits a word, every later field shifts and `iretq` consumes corrupt state.

## 4. Push order versus structure order

Stacks grow downward. `isr_common` pushes `RAX`, `RBX`, …, ending with `R15`.
After the final push, `RSP` points at `R15`. The C structure therefore begins:

```c
uint64_t r15, r14, r13, ... , rbx, rax;
uint64_t vector, error, rip, cs, rflags;
```

The apparent reverse order is deliberate. `movq %rsp, %rdi` passes a pointer
to this frame as the first System V AMD64 argument.

`cld` clears the direction flag before C. The ABI expects forward string
direction. This is an ABI hygiene step performed at an architecture entry
boundary.

## 5. Return and register ownership

After `interrupt_handler` returns, assembly pops registers in reverse,
discards vector and error with `addq $16, %rsp`, then executes `iretq`.

The breakpoint vector is intentionally recoverable: the handler prints a
self-test marker and returns. Divide and page expected-fault profiles exit
from the C handler instead. Unexpected exceptions panic.

This kernel always saves its listed general registers, rather than relying
on ordinary caller-saved/callee-saved rules across asynchronous entry.
Floating-point/vector state is outside this milestone, reinforced by
`-mgeneral-regs-only`.

## 6. Precise exceptions and saved RIP

A useful exception frame identifies the interrupted architectural context.
The meaning of saved `RIP` depends on the exception class: for a fault it
commonly identifies the instruction to retry; trap semantics can identify a
subsequent point. Do not state one universal rule for every vector.

ToyOS's divide-error and page-fault tests do not resume, so they verify entry,
decode, and diagnostics rather than restart correctness. The `int3`
breakpoint does return and provides bounded evidence for `iretq`.

## 7. Practical challenge

Use `challenges/day-005-exception-frame.sh` from the repository root.

**Purpose:** verify the implementation's frame agreement using static layout
inspection plus dynamic breakpoint and divide-error paths.

**Predict:** calculate byte offsets for `r15`, `rax`, `vector`, `error`, and
`rip`. Predict why vector 0 needs a synthetic error word and what markers the
normal and divide profiles should emit.

**Run:** the wrapper prints the C structure and assembly common entry, checks
the offsets with a temporary hosted helper that includes the canonical
header, then runs normal and divide expected-fault tests.

**Evidence:** preserve measured offsets, `SELFTEST: IDT breakpoint frame PASS`,
and `EXPECTED-FAULT: divide PASS`. The hosted `offsetof` check proves the
compiler's C layout under those include/type rules; the dynamic tests connect
that layout to selected guest paths.

## 8. Architecture lens at exception entry

The need to bridge raw architectural state to a software frame is common; the
shape is not.

- x86-64 selects an IDT gate and may push an error code; software often adds
  a vector and register save area before `iretq`.
- AArch64 vectors branch through `VBAR_ELx`; exception state is held in
  registers such as `ELR_ELx` and `SPSR_ELx`, which entry code saves into a
  software frame before `eret`.
- RISC-V records cause, return PC, and status in CSRs such as `scause`,
  `sepc`, and `sstatus`; a trap vector saves general registers before `sret`.

Do not project x86's hardware stack layout or error-code convention onto the
other architectures.

---

## Mastery: Explain · Draw · Observe · Build

### Explain

1. Why do no-error and error-code vectors use different stub macros?
2. Why does the C structure list registers opposite the push sequence?
3. Why must `cld` run before calling C?
4. Why does the breakpoint test exercise more than a log call?

### Draw

Draw the stack from high to low addresses at C-handler entry for vector 0 and
vector 14. Color hardware-pushed, stub-pushed, and common-entry-pushed words.
Both drawings must match one structure from `RSP`.

### Observe

Correlate `isr_stub_table[0]`, IDT vector zero, `isr0`, `isr_common`, and
`interrupt_handler`. At each edge, identify the address or calling contract.

### Build

Write three invariants that an automated frame test should check. Include
one offset, one vector/error relationship, and one successful return path.

## Evidence limits

Passing breakpoint and divide tests exercises two vectors. It does not prove
all 32 stubs, privilege transitions, nested exceptions, NMI behavior, or
floating-point state preservation.
