# Day 1 — Freestanding C and the Project Contract

**Target time:** approximately 2½–3 hours

> Before `kernel_main` can print one byte, which guarantees come from C, which
> come from the compiler ABI, and which has ToyOS built for itself?

An ordinary Linux process inherits a loader, virtual address space, stack,
C runtime, file descriptors, and system-call boundary. ToyOS begins under a
bootloader with none of libc's hosted environment. “Freestanding” is not a
synonym for “anything goes”: the C abstract machine still matters, while the
implementation must provide the environment around it.

---

## 1. Read the contract before the code

The canonical project is [`projects/toyos`](../../projects/toyos/). Its Week 6
build compiles four C and three assembly files, but milestone guards keep the
memory and scheduler features inactive. The important boundaries are:

```text
GRUB/Multiboot2 handoff
  → assembly bootstrap owns initial machine state
  → kernel_main uses a compiler calling convention
  → arch_init installs ToyOS exception machinery
  → serial output and QEMU exit provide test evidence
```

`include/toyos.h` is an internal interface contract. It declares fixed-width
types, the saved exception frame, assembly symbols, and functions shared
across translation units. It does not define what the CPU pushes on exception
entry; the architecture does. It expresses how this implementation exposes
that state to C.

## 2. Hosted versus freestanding C

C distinguishes hosted and freestanding implementations. In a hosted
environment, program startup reaches `main` with a substantial standard
library available. In this freestanding build:

- the entry symbol is `_start`, not `main`;
- no C runtime initializes the process;
- `-nostdlib` prevents the linker from supplying normal startup and libraries;
- kernel code provides its own output, panic, and termination mechanisms;
- some library-shaped operations could still be emitted by a compiler unless
  options and code generation are controlled.

`-ffreestanding` tells GCC that a hosted environment cannot be assumed.
`-fno-builtin` additionally prevents ordinary builtin substitution. Neither
option initializes page tables, chooses an ABI, or makes undefined C behavior
safe.

The kernel uses `stdint.h`, `stddef.h`, and `stdbool.h`. Freestanding does not
mean “no headers”; a compiler supplies the freestanding headers required by
its implementation.

## 3. Why these machine options exist

Read `projects/toyos/Makefile` as an executable specification.

- `-m64` selects x86-64 code generation after the bootstrap reaches long mode.
- `-mno-red-zone` prevents compiler use of the 128 bytes below `RSP`. An
  asynchronous exception can otherwise overwrite compiler-owned temporaries.
- `-mgeneral-regs-only` avoids generated floating-point/vector register use
  before the kernel has established save/restore policy for that state.
- `-mcmodel=kernel` chooses an x86-64 addressing model suitable for a kernel
  image; it is a code-generation choice, not a mapping policy.
- `-fno-pie` avoids position-independent executable assumptions.
- `-fno-stack-protector` avoids references to a protector runtime the project
  has not implemented.
- `-fno-asynchronous-unwind-tables` omits unwind tables that this milestone
  does not consume.

The warning set ends in `-Werror`. That improves build discipline but does not
prove absence of memory errors, ABI mistakes, or architectural faults.

## 4. Language, ABI, architecture, implementation

Use this classification whenever a claim feels ambiguous:

| Claim | Owner |
|---|---|
| Dereferencing an invalid C pointer has undefined C behavior | C language |
| First two integer arguments arrive in `RDI` and `RSI` | System V AMD64 ABI |
| `CR2` records the page-fault linear address | x86-64 architecture |
| ToyOS prints `PAGE-FAULT address=` over COM1 | ToyOS implementation |

The deliberate page-fault injection converts a controlled implementation test
into architecture evidence. It must not be generalized into a claim that
arbitrary undefined C behavior reliably raises a page fault.

The bootstrap passes two 32-bit values in `EDI` and `ESI` before calling:

```c
void kernel_main(uint32_t magic, uint32_t multiboot_info);
```

That register choice is ABI. The meanings of the values come from the
Multiboot2 contract. Their checks and panic response are ToyOS policy.

## 5. A narrow runtime surface

`arch.c` implements serial output without allocation, formatting libraries,
or system calls. `serial_hex` and `serial_dec` are deliberately bounded.
`panic_at` prints a message and source location, then exits QEMU. The macro:

```c
#define ASSERT(condition) \
    do { if (!(condition)) { PANIC("assertion failed: " #condition); } } while (0)
```

is implementation policy. It evaluates `condition` once and reports the
expansion-site file and line. It is not the hosted `<assert.h>` facility, and
it does not recover.

## 6. Practical challenge

Use `challenges/day-001-project-contract.sh` from the repository root.

**Purpose:** inspect the exact compile/link contract and verify that the
existing Week 6 profile links without hosted startup libraries.

**Predict:** write down which entry symbol you expect and whether the final
ELF should have unresolved symbols. Predict one flag whose removal could
silently compile but violate an interrupt-time assumption.

**Run:** the wrapper prints selected Makefile lines, builds the canonical
profile, then asks `readelf` about the entry and symbol table. It does not
modify `projects/toyos`.

**Evidence:** preserve the entry address, `_start` and `kernel_main` symbols,
and the undefined-symbol count. A zero count proves only that this ELF's
static symbol references were resolved—not that every runtime path is valid.

## 7. Architecture lens at a real boundary

Freestanding C is portable; bootstrap and exception ABIs are not.

- x86-64 reaches C here through a custom 32-to-64-bit assembly path and uses
  the System V AMD64 register convention.
- AArch64 firmware or a boot protocol commonly enters at an exception level;
  an assembly entry establishes `SP`, clears required storage, and passes
  arguments in `x0`–`x7` under AAPCS64.
- RISC-V firmware commonly hands off in S-mode or M-mode with arguments in
  `a0`/`a1`; the psABI governs C calls, while privileged state governs traps.

These are boundary comparisons, not claims that all platforms boot alike.

---

## Mastery: Explain · Observe · Build

### Explain

1. Why does `-ffreestanding` not remove the C abstract machine?
2. Why is `-mno-red-zone` connected to asynchronous entry?
3. Why are Multiboot2 argument meaning and AMD64 register placement separate
   contracts?
4. Why is a successful link weaker evidence than a successful fault test?

### Observe

Annotate one line each from the Makefile, header, bootstrap, and kernel entry.
Label every line C, ABI, architecture, or ToyOS implementation.

### Build

Without editing canonical code, write a one-page “entry contract” containing:
preconditions at `_start`, postconditions before `kernel_main`, callable
services available in C, and unsupported assumptions. Every statement should
cite a source path or observed artifact.

### Evidence limits

Static inspection cannot prove that GRUB loads the image, that page tables are
accepted, or that serial I/O works. Those claims need later dynamic evidence.
