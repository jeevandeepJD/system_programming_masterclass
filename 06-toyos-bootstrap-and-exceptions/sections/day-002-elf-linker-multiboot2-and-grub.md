# Day 2 — ELF, Linker, Multiboot2, and GRUB

**Target time:** approximately 2½–3 hours

> The compiler produced object files. What makes the result a kernel image
> that GRUB can recognize rather than merely another ELF file?

The ELF container, linker script, Multiboot2 header, GRUB configuration, and
ISO each answer a different question. Keep them separate.

---

## 1. The artifact pipeline

```text
C/assembly source
  → relocatable ELF objects
  → linker.ld arranges one executable ELF
  → Multiboot2 header advertises the boot contract
  → grub2-file validates recognition
  → grub2-mkrescue packages ELF + grub.cfg into an ISO
  → GRUB loads and transfers control to ELF entry _start
```

ELF describes sections, segments, symbols, addresses, and an entry point. ELF
alone does not require a Multiboot2-aware loader. The header in `src/boot.S`
is what lets GRUB recognize this kernel under that protocol.

## 2. What the linker script chooses

`projects/toyos/linker.ld` says:

```text
ENTRY(_start)
. = 1M
.multiboot2 → .text → .rodata → .data → .bss
```

The image begins at link address 1 MiB. The Multiboot2 input section is kept
and aligned to eight bytes. Code and later sections receive page-oriented
alignment. `.bss` is `NOLOAD`: the ELF describes its memory extent without
storing all zero bytes in the file.

This is implementation policy expressed to the linker. The CPU does not know
ELF section names. GRUB and the linker interpret the file; after loading,
the processor fetches bytes at addresses.

The symbols `__kernel_start` and `__kernel_end` mark an interval that later
memory code can reserve. Their values come from linker evaluation, not C
allocation.

## 3. Sections are not segments

Sections organize linking and analysis: `.text`, `.rodata`, `.bss`, symbol
tables. Program headers describe loadable segments for a loader. A section
listing can show where the Multiboot2 header resides; a program-header listing
shows what the ELF asks a loader to map or copy.

Use `readelf -h`, `-S`, and `-l` for distinct evidence:

- `-h`: class, machine, file type, and entry address;
- `-S`: section names, addresses, offsets, flags, and sizes;
- `-l`: loader-facing segments and section-to-segment mapping.

Do not infer file bytes for `.bss` merely from its in-memory size.

## 4. The Multiboot2 header arithmetic

The assembly defines:

```text
magic + architecture + header_length + checksum ≡ 0 (mod 2^32)
```

Architecture value zero denotes the Multiboot2 i386 protected-mode entry
contract. That is why GRUB enters this image in 32-bit mode even though most
of the kernel is x86-64. The terminating header tag has type zero and size
eight.

At entry, this project records:

- `EAX`: Multiboot2 bootloader magic;
- `EBX`: physical address of the Multiboot2 information structure.

Those are protocol facts. Saving them before reusing registers is a bootstrap
implementation decision.

## 5. GRUB's role and its limit

`grub.cfg` selects:

```text
multiboot2 /boot/toyos.elf
boot
```

GRUB locates the header, loads the ELF, prepares Multiboot2 information, and
transfers control. It does not install ToyOS's final GDT, IDT, TSS, exception
stubs, or serial driver. Those remain kernel work.

The Makefile copies the ELF and configuration into an ISO staging tree and
runs `grub2-mkrescue`. Seeing files in `build/week6/iso/` proves staging;
successfully booting the ISO proves more.

## 6. Relocations and symbols

Compilation creates references whose final addresses may be unknown. The
linker resolves them under `linker.ld`. The map file
`build/week6/toyos.map` records the selected layout and is useful when a
debugger is unavailable.

`nm -n` orders symbols by address. `objdump -d` decodes executable bytes
according to the ELF machine type. Neither proves that an instruction
executed. Dynamic serial or debugger evidence is needed for that claim.

## 7. Practical challenge

Use `challenges/day-002-elf-boot-contract.sh` from the repository root.

**Purpose:** connect the linker and boot source to the generated ELF and ISO.

**Predict:** predict the ELF class, machine, entry symbol, lowest important
section, and whether `.bss` occupies file bytes like `.text`.

**Run:** the wrapper builds Week 6, validates the Multiboot2 header, prints
focused ELF metadata and symbols, and displays the packaged GRUB
configuration.

**Evidence:** preserve the exact ELF machine, entry, `.multiboot2` location,
load segments, `_start`, `__kernel_start`, `__kernel_end`, and validation
result. Explain which tool interpreted each fact.

## 8. No claim beyond evidence

`grub2-file --is-x86-multiboot2` proves that the inspected file satisfies
that tool's recognition test. It does not prove firmware compatibility,
successful ISO boot, correct page tables, or correct exception handling.

A QEMU pass marker proves the tested image reached that marker under the
recorded emulator configuration. It does not prove behavior on every physical
machine.

---

## Mastery: Explain · Draw · Observe · Build

### Explain

- Why does the ELF entry point differ from C's `kernel_main`?
- Why must the Multiboot2 header be retained and early enough in the image?
- Why are sections and loadable segments not interchangeable?
- Why can a correct ELF still fail after transfer to `_start`?

### Draw

Draw one diagram with file offsets on the left and linked memory addresses on
the right. Place the Multiboot2 header, text, read-only data, data, and BSS.
Mark which facts are consumed by the linker, GRUB, and CPU.

### Observe

Compare `readelf` output with `linker.ld`. For each alignment boundary,
identify whether the evidence is stated by source, produced by the linker, or
observed in the final ELF.

### Build

Create a compact boot-chain ledger:

```text
claim | source/artifact | inspecting tool | observed value | limitation
```

Do not list a command without its question and expected evidence.

## Current references

For exact protocol rules consult the current Multiboot2 specification and
current GNU binutils documentation. ELF details should be checked against the
System V ABI and AMD64 supplement applicable to the toolchain. Local notes are
helpful orientation, not replacements for those versioned documents.
