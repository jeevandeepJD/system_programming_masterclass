# Day 4 — The `int 0x80` Syscall ABI, Saved Frame, and Return

**Target time:** approximately 3 hours

> A syscall is an untrusted register packet crossing a privilege boundary.
> Entry mechanism, register ABI, dispatch policy, and return validation are
> related but distinct.

## 1. Write down ToyOS's ABI before tracing code

The user program places a syscall number in `RAX` and arguments in
`RDI`, `RSI`, and `RDX`, then executes `int $0x80`. The kernel writes the
signed result back to the saved `RAX`. Current numbers are:

```text
1 write(fd, user_buffer, length)
2 getpid()
3 yield()
4 exit(status)
5 pipe(user_fds)
6 read(fd, user_buffer, length)
```

Number 7 exists only for the Week 9 build. Unknown numbers return `-38`.
Pointer failures use `-14`; oversized reads use `-22`; pipe capacity failure
returns `-1`. These resemble Linux errno values in places but are a local
teaching ABI, not compatibility with Linux x86-64 or historical i386
`int 0x80`.

The architecture does not know these meanings. It knows only that vector 128
selects an allowed gate.

## 2. Follow gate installation

`arch_init` calls:

```text
set_idt_gate(128, syscall_int_stub, 0, 0xee)
```

The selector is the ring-0 code selector `0x08`. IST is zero, so a
privilege-changing entry uses the TSS `RSP0` rather than an IST slot.
Attributes `0xee` describe a present DPL3 interrupt gate. DPL3 is required
because software interrupt privilege checks would reject a ring-3 caller
invoking a DPL0-only gate.

An interrupt gate clears IF during entry. The common path never re-enables it
before return, so syscalls execute with maskable interrupts disabled in this
compact kernel. That is a policy/performance limitation and also simplifies
single-core state reasoning. It is not an inherent requirement for syscall
handlers.

## 3. Reconstruct the saved frame

On a ring-3-to-ring-0 `int`, the CPU switches to the TSS `RSP0` stack and
pushes the old user stack selector and pointer, flags, code selector, and
continuation RIP. `syscall_int_stub` then pushes a synthetic zero error code
and vector 128. `isr_common` saves:

```text
RAX RBX RCX RDX RBP RSI RDI R8 ... R15
```

in an order chosen so the final stack pointer matches the field order in
`struct interrupt_frame`. It clears direction flag before calling C, moves
the frame pointer to `RDI`, and calls `interrupt_handler`.

The frame is owned by the active kernel stack for the duration of entry. It
is not copied into the process structure. `syscall_dispatch` may update
`frame->rax`; after the C call, assembly pops that modified value into live
`RAX`, discards vector/error, and executes `iretq`.

Frame layout is an ABI between assembly and C. The syscall register convention
is a second ABI between user and kernel. Mixing those two layers makes
debugging offsets needlessly hard.

## 4. Dispatch and return

`interrupt_handler` checks vector 128 and calls `syscall_dispatch`. The
dispatcher treats every register value as untrusted. It bounds I/O before
copying, distinguishes known descriptors, and leaves `result` initialized to
unknown-syscall.

The ordinary return path writes `result` into saved `RAX`. The common stub
restores all registers. Therefore every register except `RAX` returns with
its saved value under the current implementation. This is stronger than many
syscall ABIs promise, but user code should rely on the stated ToyOS contract,
not infer a permanent promise from one assembly version.

`iretq` restores user RIP, CS, RFLAGS, RSP, and SS. It performs descriptor and
canonical-address checks as part of return. ToyOS returns to the unchanged
saved continuation; it does not allow the dispatcher to supply arbitrary
return RIP or flags.

`SYS_EXIT` is exceptional: it prints results and exits QEMU rather than
returning to user space.

## 5. Why this is explicitly not `SYSCALL`/`SYSRET`

The x86-64 `SYSCALL` instruction does not consult an IDT gate and does not
automatically switch to a kernel stack. It derives entry selectors and RIP
from model-specific registers, saves return RIP in `RCX`, saves flags in
`R11`, masks selected flags, and otherwise leaves software to establish safe
per-CPU kernel state.

A production entry path normally needs a trusted per-CPU anchor, often
reached with `swapgs`, before replacing the untrusted user RSP with a kernel
stack. It must preserve state, handle tracing and scheduling, and validate
the return context. `SYSRET` has architectural constraints around canonical
addresses, selectors, and flags; robust kernels may choose `iretq` for cases
that cannot safely use the fast return.

ToyOS implements none of that MSR/per-CPU entry machinery. Claiming that its
`int 0x80` path “uses SYSCALL” because both provide syscalls would be
factually wrong. The project deliberately chooses a DPL3 interrupt gate
because the CPU then performs the validated TSS stack transition already
supported by the kernel. It trades speed for a smaller, inspectable boundary.

## 6. Diagnose an unknown syscall

The `EXPECT_FAULT=7` profile does not execute ring-3 `int 0x80`. In
`process_start`, it creates a static zeroed `interrupt_frame`, sets
`frame.rax = UINT64_MAX`, calls `syscall_dispatch` directly, and asserts the
signed result is `-38`. It emits `EXPECTED-FAULT: syscall PASS` and exits
through QEMU code `0x16`, which the host observes as status 45.

This test isolates dispatch policy. It does not test gate entry, TSS stack
switching, frame assembly, or `iretq`. The normal profile supplies dynamic
coverage of the integrated path and prints
`SELFTEST: ring3/TSS/int80 syscalls PASS`.

That distinction is an excellent report pattern:

```text
source says: default result is -38
fault profile observes: direct dispatcher rejects UINT64_MAX
normal profile observes: ring-3 calls complete
we infer: selected entry and policy contracts compose for the canonical image
```

## 7. Debugging checkpoints

At `syscall_dispatch`, inspect the frame using the exact
`build/week8/toyos.elf`. Predict that the first call has `RAX=1`, `RDI=1`,
`RSI` pointing into a user segment, and `RDX` equal to the hello string
length. Inspect saved `CS`, `SS`, `RIP`, and `RSP`. Compare live `RSP` with
the process kernel-stack range.

Disassemble `syscall_int_stub` and `isr_common` to verify push/pop symmetry.
A source struct alone cannot prove the emitted offsets; disassembly plus
debug information ties the contract to the exact artifact.

## Practical challenge

Use `challenges/day-004-syscall-path.sh` from the repository root.

**Purpose:** connect gate policy, register ABI, frame ownership, dispatch, and
return.

**Predict:** list the first syscall registers, the normal and unknown-call
markers, host statuses 33 and 45, and why `RAX` changes on return.

**Evidence:** the wrapper runs both checked profiles and inspects source,
symbols, and exact disassembly.

## Mastery prompts

Draw both stacks immediately after hardware entry and after `isr_common`.
Then explain in five sentences why replacing `int $0x80` with `syscall` in
the user assembly would not create a working or safe fast-syscall path.
