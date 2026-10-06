# Day 2 — x86-64 Page Tables, the Mapper, TLBs, and Faults

**Target time:** approximately 3 hours

> A virtual address is a question asked of a hierarchy plus cached history.
> Editing a page-table entry changes memory; it does not automatically erase
> every translation the CPU may remember.

## 1. Walk the address, not the pointer expression

For the 48-bit canonical addresses used here, x86-64 divides a virtual
address into PML4, PDPT, PD, PT, and 12-bit offset fields. Four table levels
contain 512 eight-byte entries each. `CR3` identifies the root.

ToyOS's bootstrap in `projects/toyos/src/boot.S` uses 2 MiB identity mappings
for the first GiB. The Week 7 mapper in `projects/toyos/src/memory.c` creates
ordinary 4 KiB mappings outside that region. These facts coexist: page size
is selected by entries in the walk, not globally.

## 2. Read `next_table` as ownership transfer

`next_table` tests `PTE_PRESENT`. If absent, it obtains a zeroed physical
frame, installs its address with present/write flags, and thereby transfers
that frame to page-table ownership. If an entry is huge, it returns `NULL`
because this teaching mapper does not split a 2 MiB mapping.

The cast from entry address bits to `uint64_t *` works only because the
bootstrap identity map reaches allocator results. In a higher-half kernel,
the mapper would need a direct-map conversion or temporary mapping.

## 3. Map exactly one leaf

`map_page` extracts indices with shifts 39, 30, 21, and 12. It walks from
`bootstrap_pml4`, refuses an already-present leaf, masks the physical address,
allows only low flag bits, and forces present.

Audit assumptions the compact interface does not enforce:

- virtual and physical addresses should be page-aligned;
- the virtual address should be canonical;
- flags should represent an intentional supervisor policy;
- partially allocated intermediate tables are not rolled back on failure;
- there is one active address space and one CPU.

Those are ToyOS caller obligations. Production interfaces encode more of them
and synchronize updates.

## 4. Invalidate cached translation state

After writing the PTE, ToyOS executes `invlpg` for the virtual address with a
memory clobber. The compiler barrier and CPU invalidation solve different
ordering problems. On one CPU with one address space, this is enough for the
demonstration.

On SMP, every CPU that could have cached the old translation needs a TLB
shootdown protocol. Changing `CR3`, PCID use, global mappings, and paging-
structure caches complicate the exact rule. “We wrote the PTE” is not a
complete mapping update story.

## 5. Diagnose a page fault as a tuple

The injected store in `projects/toyos/src/kernel.c` targets `0x70000000`.
The handler in `projects/toyos/src/arch.c` combines:

```text
CR2 fault address + page-fault error bits + saved RIP + exact ELF
```

The expected result is vector 14, address `0x70000000`, and
`not-present write supervisor`. A protection fault would mean the translation
exists but permissions reject the access. Reserved-bit and instruction-fetch
bits answer different questions; do not collapse every #PF into “unmapped.”

## 6. Carry the model across architectures

AArch64 commonly walks translation tables rooted by `TTBR0_ELx`/`TTBR1_ELx`;
fault diagnosis uses `ESR_ELx`, `FAR_ELx`, and `ELR_ELx`, and TLB maintenance
uses explicit `TLBI` operations with ordering barriers.

RISC-V uses `satp` to select the address space and translation mode, page
table entries appropriate to Sv39/Sv48, `sfence.vma` for translation-cache
synchronization, and `scause`, `stval`, and `sepc` for diagnosis.

These are conceptual correspondences, not identical flags or ordering
sequences. Consult the current architecture manual before writing low-level
code.

## 7. Practical challenge

Use `challenges/day-002-mapper-fault.sh` from the repository root.

**Purpose:** tie a successful mapping and a deliberate missing mapping to
specific source and dynamic evidence.

**Predict:** explain why `0x40000000` becomes writable while `0x70000000`
faults, including the expected error classification.

**Evidence:** the wrapper checks the normal mapper marker and expected fault
marker, then displays exact-ELF mapper symbols/disassembly and fault source.
Static disassembly supports instruction identity; serial output supports the
tested runtime path.

## Mastery prompts

Draw all four indices for one ToyOS virtual address, state who owns each newly
allocated table frame, and explain why a stale TLB entry can contradict the
new bytes in memory without the CPU being “wrong.”
