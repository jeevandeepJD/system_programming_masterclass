# Day 3 — Long Mode, Initial Pages, GDT, and Stack

**Target time:** approximately 3 hours

> GRUB enters a 32-bit contract, but GCC emitted 64-bit kernel code. Which
> state transitions make the first C call legal?

The transition in `projects/toyos/src/boot.S` is short because it assumes a
specific starting contract and creates only the state this milestone needs.
Read it as a dependency graph, not as magic register incantations.

---

## 1. Preconditions and ordering

At `_start`, ToyOS immediately disables maskable interrupts, saves `EAX` and
`EBX`, and selects a bootstrap stack. Disabling interrupts does not disable
exceptions or NMI. It avoids ordinary interrupt delivery before an IDT exists.

The required chain is:

```text
build page tables
  → set CR4.PAE
  → load CR3 with PML4 address
  → set EFER.LME
  → set CR0.PG
  → load bootstrap GDT
  → far jump to a 64-bit code descriptor
```

Changing this order can make the transition architecturally invalid. Consult
the current Intel or AMD architecture manual for exact prerequisites; source
order is evidence of ToyOS's sequence, not the normative rule by itself.

## 2. The initial translation

ToyOS allocates one page each for a PML4, PDPT, and page directory. It links
entry zero at each upper level, then fills 512 page-directory entries.
Each entry maps one 2 MiB page with flags `0x83`:

```text
present | writable | page-size
```

For index `i`, the physical base is `i << 21`. The result identity maps the
first 1 GiB:

```text
virtual 0x00000000..0x3fffffff
    == physical 0x00000000..0x3fffffff
```

This supports the 1 MiB-linked kernel, bootstrap structures, stack, and
Multiboot information only if GRUB placed them within that range. The project
README states this deliberate assumption. Identity mapping is implementation
policy; long mode does not mandate it.

The assembly writes only the low 32 bits of the upper-level addresses and
zeros the high half of each 64-bit PDE. This relies on all bootstrap objects
being below 4 GiB, which the current link placement satisfies.

## 3. GDT still matters in long mode

Long mode reduces ordinary segmentation, but a valid code descriptor and
selector remain necessary. The bootstrap GDT contains null, code, and data
descriptors. `lgdt` loads its pointer; a far jump to selector `0x08` both
changes `CS` and reaches `long_mode_entry`.

Inside 64-bit code, ToyOS loads data selectors, clears `FS` and `GS`
selectors, establishes `RSP`, and clears `RBP`. Later, `arch.c` installs the
runtime GDT with a TSS descriptor. Bootstrap GDT and runtime GDT serve
different phases.

## 4. Stack and ABI handoff

The BSS reserves a 32 KiB stack aligned to 16 bytes. The stack grows toward
lower addresses. Immediately before `call kernel_main`, `RSP` points at
`boot_stack_top`. The call pushes an eight-byte return address.

System V AMD64 stack alignment rules concern the stack at call boundaries and
function entry. Check generated disassembly when making a precise alignment
claim; do not infer all compiler behavior from source.

The assembly moves saved boot values into `EDI` and `ESI`. Writing a 32-bit
subregister zeros the upper half, matching the `uint32_t` C parameters. This
is the concrete ABI bridge.

If `kernel_main` returns, the bootstrap enters a `cli; hlt` loop. Normal Week
6 completion instead calls the implementation's QEMU exit device.

## 5. What architecture supplies and ToyOS chooses

**Architecture:**

- meanings of `CR0.PG`, `CR4.PAE`, `CR3`, and `EFER.LME`;
- four-level translation in the selected x86-64 mode;
- descriptor loading and far control transfer behavior;
- instruction encodings and exception conditions.

**ABI:**

- integer argument registers;
- stack alignment and register preservation conventions;
- object and symbol conventions used by C and assembly.

**ToyOS implementation:**

- first-GiB identity map;
- 2 MiB pages;
- stack size and location;
- selectors `0x08` and `0x10`;
- choice to enter a C function with two Multiboot values.

## 6. Practical challenge

Use `challenges/day-003-long-mode-trace.sh` from the repository root.

**Purpose:** correlate bootstrap source, linked addresses, decoded
instructions, and the normal boot marker.

**Predict:** calculate the virtual/physical interval covered by 512 entries
of 2 MiB each. Predict which registers carry `kernel_main` arguments and the
serial marker expected after `arch_init`.

**Run:** the wrapper builds the canonical image, prints the `_start` through
`long_mode_entry` disassembly and relevant symbols, then runs the existing
normal Week 6 regression.

**Evidence:** annotate instructions that establish each dependency. Preserve
the `BOOT: Multiboot2 -> 32-bit entry -> long mode PASS` marker and explain
that it is emitted after `arch_init`, not at the instant long mode begins.

## 7. Architecture lens at the mode boundary

This transition is specifically x86-64.

- AArch64 has exception levels and configured execution states rather than
  x86's protected-mode-to-long-mode sequence. Firmware handoff and page-table
  enablement use different system registers and barriers.
- RV64 uses privilege modes and `satp`-selected translation schemes. There is
  no GDT or segment selector; trap vectors and return state use CSRs.

The portable idea is narrower: assembly establishes architectural state and
an ABI-compliant stack before calling C. The registers and ordering are
architecture-specific.

---

## Mastery: Explain · Draw · Observe · Build

### Explain

1. Why is a far jump needed after enabling long mode?
2. Why does identity mapping simplify this bootstrap?
3. Why does a 1 GiB initial map imply a placement assumption?
4. Why is the initial stack simultaneously an implementation object and an
   ABI resource?

### Draw

Draw the three allocated table pages and one translation for address
`0x00100000`. Show indices, the 2 MiB leaf, and unchanged offset. Then mark
where `CR3` points.

### Observe

Match each control-register update in disassembly to its source line. Record
whether your tool prints symbolic names or only opcodes. Tool formatting is
not architectural evidence.

### Build

Write a transition checklist where each row has:

```text
precondition | instruction | resulting invariant | later consumer | failure
```

Include the page tables, mode bits, GDT, stack, and argument registers.

## Evidence limits

Disassembly proves bytes exist at linked addresses. The serial regression
proves this QEMU run traversed enough of the path to initialize architecture
and print. Neither alone proves the path on all x86-64 implementations.
