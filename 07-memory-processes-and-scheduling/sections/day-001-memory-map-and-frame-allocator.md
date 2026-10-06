# Day 1 — Multiboot Memory Map and Bounded Physical Frames

**Target time:** approximately 3 hours

> RAM is not “everything below the installed-memory number.” Firmware and the
> bootloader describe ranges; the kernel must validate, clip, and reserve them.

## 1. Keep physical and virtual questions separate

A physical-frame allocator answers, “Which 4 KiB physical frame may the
kernel own?” A mapper answers, “Which virtual address translates to which
physical frame?” `malloc`-style code answers, “Which byte range may this
caller use?” Mixing those questions causes excellent-looking but false
debugging stories.

ToyOS begins with an identity-mapped first GiB, so a physical address can
currently be cast to a pointer. That convenience is a bootstrap policy, not a
general truth about paging.

## 2. Parse a tagged, variable-length handoff

GRUB passes the Multiboot2 information pointer into `kernel_main` in
`projects/toyos/src/kernel.c`. In `projects/toyos/src/memory.c`,
`memory_init` reads `total_size`, starts after the eight-byte fixed header,
and advances each tag by its size rounded to eight bytes. For type 6 it also
honors `entry_size`; hard-coding the local C struct size as the stride would
reject compatible larger entries.

Before trusting a field, ask:

- Does the outer tag fit before `limit`?
- Is `tag->size` large enough for its fixed header?
- Is the memory-map entry stride large enough?
- Can `address + length` overflow?
- Is the range type 1, meaning available RAM?

ToyOS checks the central size and overflow conditions. A production parser
would reject malformed zero-size tags and prove every pointer addition safe
before dereference.

## 3. Start reserved, then release

`frame_bitmap` contains one bit per 4 KiB frame below 128 MiB. Initialization
sets every bit to one: unavailable. Only page-aligned portions of available
Multiboot ranges are cleared. This fail-closed order matters—unmentioned
memory never silently becomes allocatable.

`make_available` rounds the start up and end down, clips the end to
`MAX_PHYSICAL_MEMORY`, and clears full frames. The 128 MiB ceiling is a ToyOS
teaching bound, not an x86-64 limit. It keeps the static bitmap small and
matches QEMU's configured RAM, while deliberately ignoring higher RAM.

## 4. Reserve live boot objects again

After releasing usable ranges, `reserve_range` marks physical address zero
through `__kernel_end` used, then reserves the Multiboot information block.
This includes the image, bootstrap tables, static stacks, bitmap, and other
linked storage because they lie below `__kernel_end`.

The second reservation is not redundant. Firmware can call a region usable
while the bootloader has placed transient data inside it. The kernel owns
that handoff until parsing is over; ToyOS conservatively retains it.

## 5. Audit allocator invariants in C

`frame_alloc` scans words and bits, sets one free bit, decrements
`free_frames`, zeroes the frame, and returns its physical address. Zero is the
failure sentinel, so frame zero is permanently reserved. `frame_free` checks
alignment, bound, and nonzero before clearing.

Write these invariants beside the code:

```text
bit 1 means unavailable or allocated
free_frames equals the number of zero bits in the bounded bitmap
an allocated frame is page-aligned, nonzero, and below 128 MiB
only the allocator changes frame ownership after initialization
```

`frame_free` now asks `frame_is_used` before clearing the bit. A second free
therefore follows explicit misuse policy instead of silently leaving the
count unchanged. There is still no owner record, reference count, lock, or
per-CPU cache. Callers must transfer ownership exactly once.

## 6. Debug from the invariant outward

For an unexpected zero return, inspect `free_frames`, then the bitmap, then
the parsed ranges and reservations. For corruption after allocation, verify
that the identity map covers the returned physical frame before blaming the
zeroing loop. In GDB, use the exact `build/week7/toyos.elf`; stale symbols can
turn a correct address into a persuasive fiction.

The normal runtime line `SELFTEST: frame allocator PASS` proves one
allocate/free round trip after initialization under this QEMU profile. The
`expected-fault-week7-allocator` profile then frees that same frame again.
`frame_free` observes a clear bitmap bit, emits
`EXPECTED-FAULT: allocator PASS`, and exits through QEMU code `0x13`, which
the host harness observes as status 39. Together these distinguish normal
ownership transfer from one detected misuse. They do not prove exhaustion,
fragmentation, concurrent calls, arbitrary firmware maps, or complete
allocator safety.

Treat the injected case as controlled evidence:

```text
allocate probe → bit becomes used → free probe → bit becomes free
→ inject second free → frame_is_used is false
→ expected allocator marker → checked host status 39
```

This is a double-free detector, not recovery. A non-test build panics on the
same invalid transition.

## 7. Practical challenge

Use `challenges/day-001-memory-frames.sh` from the repository root.

**Purpose:** connect parser and bitmap invariants to the canonical Week 7
artifact.

**Predict:** state the allocator bound, page size, initial bit meaning, normal
self-test marker, injected double-free marker, and host status 39 before
running it.

**Evidence:** the wrapper shows relevant source contracts, checks the normal
profile and `expected-fault-week7-allocator`, and inspects the exact
`build/week7-allocator/toyos.elf`. Explain why the second call is rejected
before `free_frames` can be incorrectly changed.

## Mastery prompts

Explain why “available” is not yet “allocatable,” draw one unaligned memory
range before and after page rounding, and reconstruct the exact bitmap state
at both `frame_free(probe)` calls.
