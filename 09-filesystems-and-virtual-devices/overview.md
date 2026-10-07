# Week 9 — Filesystems and Virtual Devices

**Format:** seven hands-on-first daily lessons collected into one mastery package

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> Begin with a prediction, run the canonical artifact, identify the mutable
> owner, and stop every claim at the boundary of the evidence.

This package studies the existing
[`projects/toyos`](../projects/toyos/) Week 9 milestone. Every challenge is an
executable wrapper around that canonical project. The wrappers do not copy,
patch, or simulate the kernel. Their recurring structure is:

```text
purpose → prediction → evidence → explanation → limits
```

Week 9 adds two deliberately small vertical slices. The filesystem slice has
two static ramfs inodes, four global open-file slots, offsets, absolute-path
lookup, and ring-3 `open`, `read`, and `write`. The device slice enumerates PCI
bus 0 with x86 configuration mechanism #1, identifies QEMU EDU `1234:11e8`,
maps BAR0, checks the EDU identification register, and runs its liveness
inversion operation. ToyOS has no real block layer, page cache, journaling,
directories, close, permissions, IRQ-driven EDU operation, or DMA.

## Daily roadmap

### Day 1 — Why VFS objects exist

Start by inventorying `inode`, `file`, path, and descriptor state in the
running kernel. Then distinguish identity, per-open position, naming, lookup,
and ownership.

- Lesson: [day-001-vfs-objects-and-ownership.md](sections/day-001-vfs-objects-and-ownership.md)
- Challenge: `challenges/day-001-vfs-objects.sh`

### Day 2 — Tiny ramfs operations

Trace absolute-path lookup, initialization, open-slot allocation, EOF,
capacity checking, and offset movement in the two-inode ramfs.

- Lesson: [day-002-ramfs-lookup-and-io.md](sections/day-002-ramfs-lookup-and-io.md)
- Challenge: `challenges/day-002-ramfs-io.sh`

### Day 3 — User descriptors and lifetimes

Follow user registers through bounded path copying and descriptor dispatch.
Classify `-2`, `-9`, `-14`, `-22`, `-24`, and `-28` by detection boundary.

- Lesson: [day-003-user-fd-syscalls.md](sections/day-003-user-fd-syscalls.md)
- Challenge: `challenges/day-003-user-fd.sh`

### Day 4 — The missing storage stack

Use the implemented ramfs as a baseline, then build a conceptual
file→page-cache→filesystem→block path without attributing any of those absent
layers to ToyOS.

- Lesson: [day-004-storage-stack-boundary.md](sections/day-004-storage-stack-boundary.md)
- Challenge: `challenges/day-004-storage-boundary.sh`

### Day 5 — PCI discovery and BARs

Inspect configuration mechanism #1, bus/device/function addressing, vendor and
device IDs, BAR decoding, and the assumptions behind finding QEMU EDU.

- Lesson: [day-005-pci-enumeration-and-bars.md](sections/day-005-pci-enumeration-and-bars.md)
- Challenge: `challenges/day-005-pci-edu.sh`

### Day 6 — MMIO and the device contract

Trace mapped register access and the EDU liveness operation. Separate compiler
visibility, CPU ordering, device semantics, and kernel policy; mark IRQ and DMA
as explicit future boundaries.

- Lesson: [day-006-mmio-ordering-and-edu.md](sections/day-006-mmio-ordering-and-edu.md)
- Challenge: `challenges/day-006-mmio-edu.sh`

### Day 7 — Integrated fault/debug report

Generate one report spanning ring-3 open/read/write, VFS/ramfs ownership,
PCI/MMIO setup, both controlled rejection profiles, ToyOS/xv6/Linux
comparisons, and the next driver path.

- Lesson: [day-007-integrated-filesystem-device-report.md](sections/day-007-integrated-filesystem-device-report.md)
- Challenge: `challenges/day-007-week9-report.sh`

## Integrated checkpoint

Narrate both chains without merging their owners:

```text
user path bytes → checked copy → VFS lookup → inode identity
→ open-file slot/offset → fd → checked user buffer → ramfs bytes

PCI config ports → vendor/device match → BAR decode → physical MMIO page
→ kernel mapping → volatile register access → EDU-defined inversion
```

For every arrow state:

1. the architecture or device contract;
2. the kernel policy;
3. the mutable owner and lifetime;
4. the error boundary;
5. the exact source, ELF, or checked QEMU evidence.

## Architecture scope

The canonical implementation is x86-64. Its PCI configuration access uses
I/O-port instructions, and its mapper invalidates a changed translation with
`invlpg`. AArch64 systems commonly discover devices through firmware/device
tree or ACPI, map MMIO with device memory attributes, and use `dmb`/`dsb` as
required. RISC-V platforms commonly describe devices in a device tree, use
memory-mapped host bridges, and use `fence` where ordering requires it. Those
differences matter at the device-access boundary; the VFS policy concepts do
not depend on one ISA.

## Debugging discipline

Establish artifact identity before interpreting a fault. The normal Week 9
profile requires QEMU’s `edu` device, host status 33, and marker `WEEK9: PASS`.
The filesystem rejection profile requires status 47 and
`EXPECTED-FAULT: filesystem PASS`. The invalid-register profile requires
status 49 and `EXPECTED-FAULT: device PASS`. These are checked negative tests,
not accidental crashes.

The canonical `gdb/week9.gdb` stops at `kernel_main`, `vfs_init`, `edu_init`,
and `syscall_dispatch`. Suggested observations are predictions until actually
captured. For a filesystem failure inspect the copied path, selected inode,
file slot, offset, requested length, and return value. For a device failure
inspect the PCI address/data transaction, raw BAR, masked physical address,
PTE, register offset, width, value, and exact QEMU command line.

## Reference discipline

xv6-riscv comparison landmarks commonly include `kernel/file.c`,
`kernel/fs.c`, `kernel/bio.c`, `kernel/log.c`, `kernel/sysfile.c`, and its
virtio disk driver; verify the revision used. Linux landmarks include `fs/`,
`mm/filemap.c`, `block/`, `drivers/pci/`, `include/linux/io.h`, and a concrete
driver under `drivers/`; inspect a current tree before line-level claims.

Do not auto-fill mastery, confidence, hours, or completion.
