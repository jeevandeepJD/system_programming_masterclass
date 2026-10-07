# Day 1 — Ring 3, the GDT, TSS, and `iretq`

**Target time:** approximately 3 hours

> “Enter user mode” is not one instruction. It is a chain of descriptor,
> stack, frame, mapping, and return assumptions that must agree.

## 1. Begin with privilege, not process vocabulary

x86-64 exposes four privilege levels, but mainstream kernels normally use
CPL0 for the kernel and CPL3 for applications. CPL is encoded in the low two
bits of `CS`; selectors also carry a requested privilege level, and segment
descriptors carry a descriptor privilege level. The processor applies rules
involving CPL, RPL, and DPL when software loads selectors or invokes gates.

Those checks are architecture mechanism. Saying “PID 1 may write to serial”
is policy. Saying syscall 1 uses `RDI`, `RSI`, and `RDX` is ABI. Keep those
sentences separate while reading ToyOS.

ToyOS adds two user descriptors in `projects/toyos/src/arch.c`. GDT entry 3
is user data and entry 4 is user code. Their DPL is 3. The selectors used by
`enter_user` are `0x1b` for user data and `0x23` for user code: the low bits
request ring 3. The numerical selector values are a consequence of this GDT
layout, not universal x86 constants.

## 2. Understand why a TSS still matters

Long mode does not use the old hardware task-switching model for ordinary
process scheduling, but a 64-bit TSS remains essential. It stores privilege
stack pointers `RSP0`–`RSP2`, interrupt-stack-table pointers, and an I/O-map
boundary.

At boot, `gdt_init` creates a two-slot 64-bit TSS descriptor in GDT entries 5
and 6, initializes `tss.rsp[0]` to the static interrupt stack, points IST1 at
the same initial top, and executes `ltr` with selector `0x28`. Week 6 used
that infrastructure for selected exceptions. Week 8 updates `RSP0` before
entering user mode:

```text
process kernel-stack allocation
→ align top to 16 bytes
→ arch_set_kernel_stack
→ tss.rsp[0] = kernel_top
```

The process owns the 16 KiB allocation for kernel lifetime in this milestone.
There is no process teardown, so there is no reclamation path. The TSS does
not own the bytes; it holds the address the CPU will use on a ring-3-to-ring-0
gate transition.

## 3. Read the initial `iretq` frame bottom-up

`process_start` first activates the process PML4. It then updates TSS `RSP0`
and calls `enter_user(entry, user_stack)`. In
`projects/toyos/src/user_entry.S`, `enter_user` disables interrupts while
constructing the frame, loads user data selectors into `DS` and `ES`, and
pushes:

```text
SS = 0x1b
RSP = user stack top
RFLAGS with IF set
CS = 0x23
RIP = validated ELF entry
```

Because stacks grow downward, the final push leaves `RIP` at the top. `iretq`
consumes this privilege-return frame and changes CPL from 0 to 3. The
instruction is being used as a controlled first entry even though there was
no earlier user interrupt to return from.

The assembly ORs bit 9 into the saved flags, enabling maskable interrupts
after the transition. It does not execute `sti` in ring 3. The kernel controls
the future `RFLAGS` value through the crafted frame.

## 4. Account for every prerequisite

`iretq` succeeds only if several independent contracts already hold:

- `CS` and `SS` selectors name present, suitable DPL3 descriptors.
- `RIP` and `RSP` are canonical user addresses.
- the process page tables map the instruction and stack with U/S set at every
  level of the walk;
- the instruction bytes are executable under the current paging policy;
- the stack has writable user pages below `USER_STACK_TOP`;
- TSS `RSP0` names writable supervisor memory for the next privilege entry.

ToyOS validates enough of these for its embedded image, but not all possible
hostile ELF properties. For example, `load_elf` bounds load segments yet does
not separately prove that `e_entry` lies within a mapped executable segment.
The teaching mapper also does not enforce NX or W^X, so all load segments are
writable.

## 5. The syscall gate completes the round trip

`arch_init` installs IDT vector 128 with attributes `0xee`. This is a present
64-bit interrupt gate with DPL3. DPL3 is what allows a CPL3 `int $0x80`
instruction to select it; ordinary exception gates remain DPL0 unless the
architecture gives them other semantics. As an interrupt gate, it clears IF
on entry.

When `int $0x80` crosses privilege, the CPU obtains the ring-0 stack pointer
from the loaded TSS, switches stacks, and records the old user `SS`, `RSP`,
`RFLAGS`, `CS`, and `RIP`. The assembly stub adds normalized vector and error
slots and saves general registers. That final in-memory shape matches
`struct interrupt_frame`.

This stack switch is the concrete reason `RSP0` cannot be stale or shared
carelessly. In a multi-process kernel, every scheduling transition must make
the TSS point to the kernel stack belonging to the process that can next
enter. ToyOS has one process and avoids that coordination problem.

## 6. Debug the transition with architecture evidence

Static evidence can prove descriptor constants, push order, symbols, and
instructions. Runtime evidence is needed to prove a selected path executed.
For an interactive session, build and pause `debug-week8`, attach using the
canonical Week 8 GDB script, then inspect:

```text
before enter_user: CR3, tss.rsp[0], entry, user_stack
at syscall_dispatch: CS/SS saved in frame, saved user RIP/RSP, live kernel RSP
before iretq: restored register values and remaining hardware frame
```

State predictions before running. At dispatch, live CPL is 0 while
`frame->cs` should describe the interrupted ring-3 context. Do not confuse
the current debugger register `CS` with saved `frame->cs`.

## 7. Architecture comparison

RISC-V commonly enters user mode with an `sret` after setting `sepc`,
`sstatus`, and an appropriate user page table; traps use `stvec`, `sscratch`,
and software-managed trap-entry conventions. AArch64 uses exception levels,
`ELR_EL1`, `SPSR_EL1`, vector tables, and `eret`. These are architecture
counterparts, not alternative names for a TSS or x86 selector.

The transferable idea is the invariant set: establish a less-privileged
continuation, provide a trusted entry stack, define saved-state ownership,
and validate the return state.

## Practical challenge

Use `challenges/day-001-ring3-entry.sh` from the repository root.

**Purpose:** connect GDT/TSS construction, the user IDT gate, and the initial
`iretq` frame to the exact Week 8 artifact.

**Predict:** identify selector values, push order, normal host status 33, and
the ring3/TSS/int80 self-test marker.

**Evidence:** the wrapper displays canonical source contracts, builds and
checks Week 8, and disassembles `enter_user` and the syscall stub from the
exact ELF.

## Mastery prompts

Explain why the TSS is used even though ToyOS does not use hardware task
switching. Then classify each of these as architecture, ABI, or policy:
DPL3 on vector 128; syscall number 1; maximum I/O length 256; `RAX` return
value; the CPU loading `RSP0`; PID 1.
