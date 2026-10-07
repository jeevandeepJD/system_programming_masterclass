# Day 7 — Integrated Filesystem and Device Fault/Debug Report

**Target time:** approximately 3 hours

> A useful driver report ties every conclusion to artifact identity, mutable
> ownership, a detection boundary, and an explicit non-claim.

## 1. Generate evidence before writing conclusions

Run `challenges/day-007-week9-report.sh`. Predict normal/filesystem/device host
statuses 33, 47, and 49 with exact markers. The wrapper creates a temporary
report, runs the full canonical Week 9 regression, hashes the relevant ELFs,
inspects symbols and headers, and extracts the implemented contracts.

Generated output is not the explanation. Add your own causal trace, including
which state changes before each result and which absent feature prevents a
broader conclusion. Never present suggested GDB commands as observations.

## 2. Trace user open/read/write into ramfs

Begin at `_user_start` in the exact Week 9 user ELF:

```text
RAX=7, RDI=&"/hello" → int 0x80
gate/stub → syscall_dispatch
copy_user_string → page-wise checked copy into 48-byte kernel array
vfs_open → exact lookup → static inode → free file slot at offset 0
fd returned in saved RAX → iretq

RAX=6, RDI=fd, RSI=&io_buffer, RDX=64 → int 0x80
vfs_read → inode bytes into dispatcher stack buffer; file offset advances
copy_to_user → checked user destination
count returned → user invokes write fd 1 → COM1 output
```

Then trace `/note`. Initialization has already opened it and written four
probe bytes. The user opens it independently, obtaining another file slot
with offset zero, and writes `"user-fd-write"` over the beginning. The inode
is shared; the offsets are not.

For every step record:

- privilege level and active address space;
- source and destination byte owner;
- path, inode, file slot, and descriptor identity;
- old/new offset and size;
- return value and earliest error detector;
- source, ELF, or runtime evidence type.

This catches a common compressed but misleading statement: “the user reads
the inode.” The user never receives an inode pointer. It supplies a descriptor
and user buffer; kernel code resolves the slot and stages bytes through a
kernel stack buffer.

## 3. Trace PCI discovery into MMIO

The independent device chain is:

```text
outl 0xcf8 with enabled bus0/deviceN/function0/offset0 address
inl 0xcfc → compare 0x11e81234
read BAR0 at offset 0x10 → reject I/O BAR → mask low flags
map physical BAR page at virtual 0x60000000 writable
volatile 32-bit ID read → compare 0x010000ed
checked aligned write at 0x04 → read inverse → pass marker
```

Record the QEMU version and `-device edu`; the virtual machine configuration
is part of artifact identity. Distinguish the PCI standard, x86 config-port
mechanism, QEMU EDU register contract, page-table mapping mechanism, and
ToyOS policy.

The two Week 9 slices meet only in boot orchestration and the final successful
user exit. EDU does not back ramfs. Ramfs does not feed a block request to
EDU. An integrated report must not draw a nonexistent data arrow between
them.

## 4. Interpret the controlled rejection profiles

**Filesystem profile (`EXPECT_FAULT=8`).** During `vfs_init`, after the normal
`/note` probe, kernel code calls `vfs_open("/missing")`. Exact lookup returns
`-2`; the assertion verifies that result; the profile emits
`EXPECTED-FAULT: filesystem PASS` and writes guest exit value `0x17`, observed
as host status 47. It does not test user copying or descriptor dispatch.

**Device profile (`EXPECT_FAULT=9`).** `edu_init` completes discovery, mapping,
ID, and liveness. It then calls `edu_checked_write(3, 0)`. The helper rejects
the request because offset 3 is not the only authorized liveness offset and is
unaligned. The profile emits `EXPECTED-FAULT: device PASS` and guest value
`0x18` appears as status 49. It does not issue invalid MMIO.

An unexpected page fault belongs to neither selected profile. Gather CR2,
error bits, saved RIP/CS/RSP, active CR3, the relevant PTE chain, raw BAR, and
exact ELF. If the fault occurs before mapping, debug setup; if the access
completes with a wrong value, debug address/width/device semantics.

## 5. Use GDB as an evidence instrument

The provided Week 9 command file loads `build/week9/toyos.elf`, connects to
`:1234`, and sets breakpoints at `kernel_main`, `vfs_init`, `edu_init`, and
`syscall_dispatch`. Before an interactive session, predict:

- the matched PCI device number and raw BAR;
- `edu_mmio` before and after mapping;
- ID and liveness values;
- path bytes in the dispatcher array;
- selected inode and file slot;
- descriptor and offset before/after read;
- saved syscall registers and return `RAX`.

If symbols disagree with execution, first verify the paused VM uses that ELF
and profile. If a descriptor appears invalid, derive its slot arithmetically
before inspecting random memory. If an MMIO read is suspicious, do not repeat
writes blindly; a real device register may have side effects.

## 6. Compare ToyOS, xv6, and Linux

ToyOS Week 9 uses static arrays and synchronous direct copies. It has one
process, two inodes, four global slots, fixed absolute paths, no close, and no
persistent storage. Device work is one polled MMIO operation after minimal PCI
discovery.

xv6-riscv has per-process descriptor tables, reference-counted open files,
inodes and directories, a buffer cache, block allocation, a write-ahead log,
and a virtio disk path. Its device discovery and interrupt mechanisms are
RISC-V platform-specific rather than x86 config-port based. It is still
educational, but its storage lifetime and crash model are materially richer.

Linux separates descriptor tables, open files, dentries, inodes, superblocks,
filesystem implementations, page cache/writeback, block I/O, and many driver
models. PCI core code enumerates topology and resources; managed mapping and
MMIO accessors encode platform requirements; interrupt and DMA APIs establish
lifetime and ordering contracts. Linux also deals with hotplug, power
management, IOMMUs, concurrency, and recovery.

The comparison is useful only when tied to an invariant. Reference counting
prevents premature destruction. Dentry/inode separation handles naming versus
identity. A page cache coordinates file data in memory. Journaling protects
selected metadata invariants across crashes. DMA APIs protect ownership and
addressability. Interrupt APIs coordinate asynchronous completion.

## 7. Preview a defensible driver path

A next EDU driver milestone could remain polled:

1. define typed register offsets and read/write helpers;
2. introduce explicit MMIO mapping attributes;
3. read and validate device state;
4. submit one bounded operation;
5. poll with a finite timeout;
6. report device error status;
7. inject timeout, invalid length, and wrong-state tests;
8. serialize access if more than one caller exists.

An IRQ milestone should follow only after routing and shared-state lifetime are
defined. A DMA milestone should follow only after buffer physical addressing,
pinning, ownership, bus-master enablement, coherency, and bounds are explicit.
Connecting file I/O to a device would additionally require a block contract,
cache policy, and filesystem mapping. None is implied by the current liveness
test.

## 8. State conclusions narrowly

Week 9 demonstrates:

- separation of static inode identity and static per-open offset state;
- exact bounded lookup of two absolute paths;
- bounded ramfs reads and writes through user descriptors;
- checked user path and data copying;
- x86 PCI bus-0/function-0 configuration reads;
- exact QEMU EDU identification and BAR0 mapping;
- checked 32-bit liveness MMIO behavior;
- controlled missing-path and invalid-register rejection.

It does not demonstrate close/reclaim, directories, permissions, mount
semantics, persistence, page cache, block I/O, journaling, concurrent access,
portable MMIO mapping, interrupts, DMA, physical hardware, or a filesystem
backed by EDU.

## Practical challenge

**Purpose:** produce one reproducible report connecting user file operations,
VFS/ramfs state, PCI/MMIO bring-up, fault boundaries, and comparisons.

**Prediction:** checked outputs are 33/47/49; the filesystem fault ends in
kernel initialization, while the device fault rejects offset 3 before MMIO.

**Evidence:** tool and artifact identity, full Week 9 regression, exact
symbols/source, integrated traces, and explicit limits.

**Limits:** the wrapper cannot substitute for an actually captured GDB
session and makes no page-cache, block, IRQ, DMA, or persistence claim.
