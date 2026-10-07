# Day 1 — VFS Objects, Names, and Ownership

**Target time:** approximately 3 hours

> A file descriptor is not a file, a path is not an inode, and an inode is not
> an open operation. Keeping those identities separate makes lifetime bugs
> visible.

## 1. Start from the evidence

Before reading an abstract filesystem diagram, run
`challenges/day-001-vfs-objects.sh` from the repository root. Predict three
facts: `/hello` and `/note` name two inodes; a successful open returns a number
from 5 through 8; and the offset belongs to an open-file slot rather than to
the inode. The wrapper builds the canonical Week 9 image, checks its normal
marker and status, and displays the current structures and symbols.

Do not turn a successful marker into more evidence than it is. It proves that
the selected initialization and user path completed under this build and QEMU
configuration. It does not prove concurrent safety, reclaim, arbitrary path
handling, or persistence.

## 2. Why kernels introduce a VFS vocabulary

Programs want operations such as open, read, and write. Storage objects vary:
a ram-backed file, a disk filesystem file, a pipe, a terminal, and a device
node do not store bytes the same way. A virtual filesystem layer gives kernel
code a stable vocabulary while concrete implementations supply operations.

Four concepts are especially useful:

- A **path** is a name supplied for resolution. It is transient input, not the
  stored object.
- A **dentry** associates a name with an object in a directory context and can
  cache successful or failed lookup. ToyOS has no dentry structure or cache.
- An **inode** represents filesystem object identity and metadata. ToyOS's
  inode embeds its data and stores a whole absolute path, size, and writable
  flag.
- A **file** or open-file description represents one open instance. It carries
  the current offset and points at an inode.
- A **file descriptor** is a small process-visible index that selects an
  open-file description.

ToyOS implements only a narrow shape of that model. The names are still worth
using because they expose why the offset is not placed in `struct inode`. If
two independent opens shared an inode-level offset, one reader would move the
other reader's position. In ToyOS, each successful `vfs_open` selects a free
`struct file`, stores the inode pointer, resets that slot's offset to zero, and
returns `5 + slot`.

## 3. Build an ownership ledger

The two arrays in `vfs.c` have static storage duration. They are kernel-global
and exist for the life of the kernel. `vfs_init` assigns two pointers to
string literals and initializes inode contents. No allocation or destruction
occurs.

An ownership ledger for this implementation is:

```text
object                 owner/lifetime                 mutable state
path argument          syscall stack temporarily      copied characters
inode array            VFS global / kernel lifetime   data, size, writable
path string literals   kernel image / kernel lifetime immutable bytes
file array             VFS global / kernel lifetime   inode pointer, offset, open
returned descriptor    user-visible value             no object ownership itself
```

The descriptor does not own the inode. It encodes an array position. The file
slot does not own dynamically allocated memory either; it borrows a pointer to
a static inode. These facts make the current code simple, but they would be
unsafe assumptions if inodes could be removed or mounts could disappear.

In a fuller kernel, ownership usually involves reference counts. A process
descriptor table refers to an open-file description; duplicated descriptors
may share that description and therefore its offset; the open file refers to
an inode; dentries and inodes can outlive one pathname lookup because caches
and active users retain them. Unlink removes a name but need not destroy an
object that remains open. ToyOS has none of those transitions, so do not infer
them from its type names.

## 4. Path resolution is policy, not string equality

ToyOS's lookup loops over two inodes and compares the supplied string to each
stored absolute path. That is bounded only indirectly by the syscall's
48-byte path-copy buffer. There are no directories, components, `.` or `..`,
mount points, symbolic links, current working directory, root confinement,
permissions, or rename races.

A real path walk repeatedly resolves components under a starting directory.
It must define locking and lifetime rules while names can change. A dentry
cache can preserve positive results and negative results such as “this name
was absent,” but cache validity is a kernel-policy question. The architecture
does not decide whether symbolic links are followed or whether a mount can be
crossed.

This distinction illustrates a theme for the week:

```text
architecture/device contract: how bytes and privilege transitions work
kernel policy: what names mean and which operation is permitted
C lifetime: whether every pointer still names a live object
```

## 5. Inspect invariants before debugging symptoms

For a wrong read result, avoid immediately blaming user copying. Check the
earliest relevant invariant:

1. Is the path NUL-terminated within the syscall's 48-byte capacity?
2. Did lookup select the intended inode?
3. Did open reserve a free file slot and set offset zero?
4. Does the descriptor decode back to that slot?
5. Is `file->inode` still valid?
6. Is `offset <= inode->size` before subtracting?
7. Did the operation move only that file slot's offset?

Current initialization makes `offset <= size` true for the demonstrated paths.
The subtraction in `vfs_read` relies on that invariant. If future seek support
allowed offsets beyond EOF, unsigned `size - offset` would underflow unless
the read path first checked the relationship.

## 6. Compare the object model

xv6 separates descriptors, `struct file`, and filesystem inodes. Its open
files are reference counted and can select inode-backed, pipe, or device
operations. Its filesystem has directories, a buffer cache, logging, and a
disk layout. Verify names in the revision used rather than copying line
numbers from memory.

Linux has a much richer VFS: a task's descriptor table points at `struct file`
objects; files refer to dentries and inodes; superblocks represent mounted
filesystem instances; operation tables dispatch to implementations. Names,
mount namespaces, credentials, caches, writeback, and concurrent lifetime all
add invariants. The conceptual correspondence is useful, but ToyOS structures
are not miniature drop-in Linux structures.

## 7. Explain the observed path

After running the wrapper, write a causal account of one `/hello` open. Name
the source of the path bytes, the temporary kernel destination, the lookup
comparison, the selected static inode, the selected static file slot, the
offset initialization, the descriptor arithmetic, and the evidence that user
execution later completed.

Then state the limit: no close means an open slot is never released. Repeating
successful opens eventually reaches `-24`. That is a lifetime policy, not an
x86 rule and not a ramfs storage limitation.

## Practical challenge

**Purpose:** identify each Week 9 VFS object, its owner, and its lifetime.

**Prediction:** static inodes outlive every open, each open slot gets its own
offset, and the checked image reaches `WEEK9: PASS` with host status 33.

**Evidence:** the wrapper prints current definitions and symbols, runs the
canonical checked image, and records artifact identity.

**Limits:** no dentry object, close, reclamation, concurrency, permissions,
directories, mount, or persistence is exercised.
