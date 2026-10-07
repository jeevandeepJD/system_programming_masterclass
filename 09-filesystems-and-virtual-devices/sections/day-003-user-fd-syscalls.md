# Day 3 — User File Descriptors, Syscalls, and Error Lifetimes

**Target time:** approximately 3 hours

> A safe syscall path validates two lifetimes: the user memory range during
> copying and the kernel object selected by the descriptor during use.

## 1. Observe the user program before the dispatcher

Run `challenges/day-003-user-fd.sh`. First predict the register-level path:
`SYS_OPEN` places 7 in `RAX` and a user path pointer in `RDI`; the result is
returned in `RAX`. `SYS_READ` uses 6 with descriptor in `RDI`, destination in
`RSI`, and length in `RDX`. File writes reuse `SYS_WRITE` number 1: descriptor
1 selects serial, descriptor 4 selects the old pipe, and other descriptors
are offered to VFS in Week 9.

The wrapper checks the exact image and disassembles the user entry. Source
shows intent; disassembly establishes what the selected ELF contains; QEMU
output establishes one executed path. Keep these evidence classes distinct.

## 2. Open copies a string across the trust boundary

The syscall dispatcher cannot pass a user pointer directly to `vfs_open`.
User virtual addresses belong to the active user address space and may be
unmapped, supervisor-only, or end at a page boundary. `copy_user_string`
copies one byte at a time through `copy_from_user`, up to a 48-byte kernel
array.

It returns true only after copying a NUL. An invalid translation produces
false and the syscall returns `-14`. A valid 48-byte sequence without NUL also
returns false and currently maps to the same `-14`, although conceptually it
is a length/termination error rather than a bad address. The copied kernel
array has automatic storage duration inside the `SYS_OPEN` case. It remains
live during `vfs_open`, which compares immediately and does not retain the
pointer. Retaining it in an inode would create a dangling pointer after
dispatch returned.

Copying one byte at a time is simple and naturally checks page transitions,
but it is inefficient. A production helper might copy chunks while preserving
fault recovery and termination semantics. Performance does not justify
skipping validation.

## 3. Descriptors select policy

ToyOS Week 9 has no per-process descriptor table. Instead, descriptor values
have hard-coded meanings:

```text
1       serial output
3       global pipe read end
4       global pipe write end
5..8    VFS file slots 0..3
```

The single process makes this convention usable for teaching, but it is not a
general descriptor model. A full process descriptor table permits the same
integer in different processes to select different open files, supports
close, duplication, inheritance, flags, and controlled sharing.

The dispatcher first stages up to 256 bytes in its kernel stack buffer. For a
file write it checks and copies user bytes before calling `vfs_write`. For a
file read, it calls `vfs_read` into the kernel buffer and only then copies the
returned count to user memory. This staging ensures ramfs never directly
dereferences a user pointer.

## 4. Trace ownership through read and write

For a file read:

```text
user owns mapped destination bytes
dispatcher owns temporary 256-byte stack buffer
file slot owns current offset
inode owns ramfs data and size
vfs_read copies inode → kernel buffer and advances offset
copy_to_user copies kernel buffer → validated user pages
```

There is a subtle policy issue: the offset advances before `copy_to_user`.
If user copy-out fails, the syscall returns `-14`, but the file position has
already moved. A production design must intentionally define and test this
failure behavior, often by faulting in pages, locking state, or handling
partial copies. ToyOS does not roll back the offset.

For a file write, user copy-in completes before the inode changes. If copy-in
fails, VFS sees no operation. If VFS rejects capacity, no bytes or offsets
change. These orderings are part of syscall policy, not an ISA requirement.

## 5. Classify errors by detection boundary

Current numeric results include:

- `-2`: VFS lookup found no matching inode.
- `-9`: descriptor/slot or VFS buffer validity failed; also used for
  read-only writes.
- `-14`: user copy or bounded string copy failed.
- `-22`: read length exceeded the dispatcher's 256-byte staging capacity.
- `-24`: no open-file slot remained.
- `-28`: write would exceed the 256-byte inode capacity.
- `-38`: syscall number was unknown.

Do not describe these as complete POSIX `errno` behavior. They are negative
return conventions in this kernel. The assembly caller receives the two's
complement value in `RAX`; there is no libc translating it into `errno`.

Debugging begins at the earliest detector. For `-14`, inspect the active CR3,
the user virtual address and page walk, access permission, requested range,
and string terminator. For `-2`, first prove the kernel copy succeeded, then
inspect exact path bytes. For `-9`, decode the descriptor and file-slot open
state. For `-28`, record offset, length, and remaining capacity.

## 6. Lifetime is incomplete without close

The static inode and file arrays never disappear, so pointers remain valid for
kernel lifetime. But resources leak by design: no syscall clears `file.open`.
The initialization probe consumes one slot, and the user path consumes two
more. A future close must validate the descriptor, release exactly one
reference, prevent use-after-close, and define behavior when descriptors share
an open file.

Adding processes would require deciding whether descriptor state belongs to a
process, how fork shares open-file descriptions, how exec preserves or closes
entries, and what teardown releases. Locks would become necessary if another
CPU or task could mutate slots and offsets concurrently. A raw pointer plus a
Boolean is not enough for those lifetimes.

## 7. Compare syscall boundaries

xv6 copies pathname arguments into kernel memory and maps process descriptor
indices to reference-counted `struct file` objects. Its `read` and `write`
dispatch according to file type and coordinate inode operations through its
filesystem and logging rules.

Linux separates syscall entry, descriptor lookup, `struct file` lifetime,
VFS operations, filesystem implementation, and architecture-aware user-copy
handling. Linux can return short counts, restart interrupted operations, apply
credentials and flags, and coordinate concurrent close. Those mechanisms
address concrete invariants absent from ToyOS.

The architecture difference is primarily at syscall and user-memory access.
ToyOS uses x86 `int 0x80`, page-table U/S checks, and a software walk. xv6 on
RISC-V uses `ecall` and supervisor page-table conventions. VFS concepts such
as separate open offset and inode identity remain kernel-policy concepts.

## Practical challenge

**Purpose:** follow user bytes, descriptor values, kernel buffers, offsets,
and errors through the implemented syscall path.

**Prediction:** the canonical user opens `/hello`, reads its current size,
writes that count to serial, opens `/note`, writes its message, and exits with
the normal checked result.

**Evidence:** source and ELF instruction sequence, dispatcher/VFS contracts,
artifact hashes, and QEMU markers.

**Limits:** no libc, process-local fd table, close, dup, fork inheritance,
blocking, rollback after copy-out failure, or concurrent mutation exists.
