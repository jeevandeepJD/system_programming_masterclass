# Day 4 — File to Page Cache to Filesystem to Block: A Conceptual Boundary

**Target time:** approximately 3 hours

> ToyOS demonstrates file semantics over static RAM. It does not demonstrate
> a page cache, block layer, disk filesystem, writeback, or journaling.

## 1. Prove what is absent

Run `challenges/day-004-storage-boundary.sh`. Predict that it will find the
ramfs arrays and direct byte-copy loops, while searches for block requests,
dirty pages, journal transactions, sector I/O, and writeback find no Week 9
implementation. The wrapper also runs the canonical image so absence claims
are tied to the same project that supplies the positive VFS evidence.

An absence argument should be scoped carefully. Searching selected source
files is not a mathematical proof about every possible artifact. Here the
project source map, build inputs, symbols, and direct implementation agree:
Week 9 links `vfs.c` and `edu.c`; ramfs data is embedded in inodes; EDU is used
only for MMIO liveness. No block storage path is claimed.

## 2. Build the conceptual read path

In a conventional cached filesystem, a buffered user read may follow this
conceptual chain:

```text
fd → open file and offset → inode/mapping → page-cache lookup
   → cache hit: copy cached bytes to user
   → cache miss: filesystem maps file offset to storage blocks
               → block I/O request → device driver → controller/media
               → completion → page becomes uptodate → copy to user
```

Each arrow has ownership and waiting rules. The open file owns a position.
The inode identifies the file. Cached pages need references and state such as
locked, uptodate, dirty, or under writeback. A block request owns a transfer
description until completion. A driver owns queue and device state. User
memory must remain safely accessible for the copy, but buffered I/O normally
does not let a storage controller write arbitrary user pointers.

ToyOS collapses that chain:

```text
fd → static file slot → static inode → embedded 256-byte array
```

There is no cache miss because there is no lower backing store. Calling the
inode byte array a “page cache” would be false: it is the authoritative
storage for this boot, not a cached copy of another object.

## 3. What a page cache contributes

A page cache stores file content in memory in page-sized units and coordinates
reads, writes, mappings, reclaim, and writeback. It must answer questions that
ToyOS can ignore:

- Is this page present and uptodate?
- Who waits while I/O fills it?
- Can reclaim remove it?
- If dirty, when and how is it written back?
- What happens when writeback fails?
- How do a memory mapping and `read` observe coherent bytes?
- What locking prevents duplicate fills or stale size observations?

The CPU page tables that map virtual memory are not the page cache. Both may
deal in page-sized memory, but they solve different problems. Page tables
translate addresses and enforce access permissions. The page cache represents
cached file content and its I/O state. ToyOS's user copying walks page tables;
that does not imply a file cache.

## 4. Filesystem mapping and metadata

A persistent filesystem must translate a logical file offset into storage
locations while maintaining metadata: file size, allocation, directory
entries, ownership, timestamps, and free space. Sparse regions may read as
zero without allocated blocks. Extending a file may allocate blocks and must
handle partial failure.

ToyOS's “mapping” is array indexing: `inode->data[file->offset + i]`. Its
metadata consists of path pointer, size, and writable Boolean. It has no
on-media format, superblock, allocation bitmap, directory records, checksum,
or mount operation.

xv6 offers a useful next step because its filesystem has inodes, directories,
block mapping, a buffer cache, and a write-ahead log over a virtio disk.
Linux supports many filesystem implementations over generic VFS, page-cache,
writeback, and block interfaces. Exact paths and APIs evolve; inspect the
revision being discussed.

## 5. The block layer and driver boundary

A block layer accepts operations over logical sectors or blocks, schedules or
merges work according to policy, and submits requests to a driver. The driver
translates those requests into a device protocol, manages registers and
descriptors, and reports completion.

The Week 9 EDU device is not used as a block device. ToyOS writes one
liveness register and reads its inverted value. It neither transfers file
data through EDU nor submits sector requests. EDU has educational DMA and
interrupt facilities, but this project explicitly does not enable them.

If a future ToyOS storage driver used programmed I/O, the CPU might copy data
through device registers while polling status. An interrupt-driven driver
would submit work, retain request state, and complete later. DMA would require
device-visible physical addresses, pinned buffers, ownership transitions,
cache/coherency handling where applicable, and strict length validation.
Those are new invariants, not automatic consequences of mapping a BAR.

## 6. Why journaling is a different layer

Journaling or write-ahead logging protects selected filesystem invariants
across crashes by controlling the order in which metadata and sometimes data
become durable. A typical transaction records intended changes in a log,
persists enough information, commits, then installs or checkpoints changes.
Recovery replays committed work and ignores incomplete work.

Memory barriers and MMIO ordering do not provide crash consistency. A CPU
barrier can constrain ordering of operations as observed by processors or
devices under an architecture's rules. It does not by itself guarantee that a
storage device has committed data to durable media. Flush and force-unit
access protocols, controller caches, filesystem transaction rules, and error
handling matter.

ToyOS has no journaling because it has no persistent filesystem or block
write path. Its static RAM changes vanish at reset. Describing the simple
all-or-error ramfs write as “transactional” would confuse in-memory control
flow with durable atomicity.

## 7. Design the next slice without pretending it exists

A defensible next teaching slice might add a tiny read-only block-backed
filesystem:

1. identify a suitable virtual block device;
2. define a synchronous, polled block-read contract;
3. allocate a kernel-owned aligned buffer;
4. validate device-visible addresses and lengths;
5. read and verify one fixed superblock;
6. map one file's logical block;
7. fill a simple cache entry;
8. expose it through existing open/read semantics;
9. inject timeout, malformed metadata, and out-of-range block tests.

Only after that works should claims include block I/O. Dirty data, writeback,
crash recovery, IRQ completion, and DMA each deserve separate milestones and
negative tests.

## Practical challenge

**Purpose:** distinguish the implemented ramfs from the conceptual persistent
storage stack.

**Prediction:** evidence shows direct inode-array copies and no real page
cache, block request, journaling, or file-data device operation.

**Evidence:** canonical build inputs, source paths, symbol inventory, and a
checked normal run.

**Limits:** text and symbol inspection cannot validate an absent future
design; the conceptual path is a design model, not a ToyOS execution trace.
