# Day 2 — Tiny ramfs Lookup, Open, Read, and Write

**Target time:** approximately 3 hours

> Trace state changes before assigning filesystem names to them. In this
> ramfs, persistence means only “until this VM stops.”

## 1. Run the vertical slice first

Use `challenges/day-002-ramfs-io.sh` from the repository root. Before running
it, predict the state transitions for `/hello`: lookup chooses inode 0, open
chooses a free slot and offset 0, a read requests 64 bytes but returns only the
file size, and EOF would return 0 after the offset reaches the size. Predict
also that `/note` accepts the user write while `/hello` does not.

The wrapper checks the normal Week 9 image and the missing-path rejection
profile. The rejection profile calls `vfs_open("/missing")` in kernel setup,
asserts return `-2`, emits an exact marker, and exits with guest value `0x17`,
which the harness observes as host status 47. It does not enter ring 3 and is
not a user pathname test.

## 2. Initialization constructs the whole filesystem

`vfs_init` initializes two elements of a static inode array:

```text
/hello  read-only  "USER: ramfs open/read through fd\n"
/note   writable   empty, then probed with "vfs\0"
```

`set_contents` copies until NUL or 256 bytes. It records the count of copied
bytes and does not copy the terminator. The call with `"vfs"` passes a source
string to `set_contents` only during setup? No: initialization instead opens
`/note` and calls `vfs_write` with `sizeof(probe)`, so four bytes are written:
`'v'`, `'f'`, `'s'`, and `'\0'`. File data may contain NUL; `size`, not a C
string terminator, defines readable content.

This detail is a useful C lesson. Files are byte sequences. Treating file data
as a C string would conflate a stored zero byte with end of file and could
read beyond a valid range when no zero exists.

## 3. Lookup is exact and linear

`vfs_open` starts with a null inode pointer, walks the two-element array, and
uses `strings_equal`. A match selects the address of that static inode. If
none matches, it returns `-2`, corresponding conceptually to `ENOENT`.

This is whole-string equality, not a component walk. `/hello/`, `hello`, and
`/HELLO` do not match `/hello`. There is no normalization. The loop is safe
for the stored literals and for a properly NUL-terminated caller string. The
user syscall path establishes that termination with a bounded byte-at-a-time
copy; direct kernel callers are responsible for the same precondition.

After lookup, another linear scan finds the first `open == false` file slot.
The slot borrows the inode pointer, resets offset to zero, sets `open`, and
returns `VFS_FILE_BASE + i`. If all four slots are open, the function returns
`-24`, conceptually `EMFILE`. Because there is no close, every successful open
consumes one slot for the rest of the boot.

Initialization itself consumes one slot by opening `/note`; the user program
then opens `/hello` and `/note`, leaving one free slot. This accounting matters
when predicting capacity. “Four slots” does not mean the user begins with four
available slots.

## 4. Read is bounded by request and EOF

`file_for` first validates the descriptor range `[5, 9)`, indexes the static
array, and returns the slot only if `open` is true. `vfs_read` rejects a bad
slot or null destination with `-9`.

For a valid slot:

```text
available = inode->size - file->offset
count = min(requested length, available)
copy count bytes from inode data at offset
offset += count
return count
```

At EOF, `available` and `count` are zero. Returning zero distinguishes EOF
from an error. A short read is not automatically a failure; it can mean that
the request reached the current file size.

The code depends on the invariant `offset <= size`. Opens establish offset 0.
Reads never advance beyond size. Writes either extend size to the new offset
or remain within it. No seek exists, so the invariant holds on implemented
paths. When extending the design, make this precondition explicit before the
unsigned subtraction.

## 5. Write combines permission, capacity, and position

`vfs_write` returns `-9` when the file slot is invalid, the source pointer is
null, or the inode is not writable. Combining those conditions simplifies
ToyOS but loses diagnostic precision: a read-only file would conventionally
produce a permission- or descriptor-mode-related error rather than “bad
descriptor.”

Capacity is checked without overflow:

```text
length > RAMFS_CAPACITY - offset
```

Given the offset invariant, subtraction is safe and avoids computing
`offset + length` before confirming it fits. A failing write returns `-28`
without copying a prefix and without moving the offset. A successful write
copies all requested bytes, advances the open-file offset, and extends inode
size if necessary. This all-or-error policy is a ToyOS choice; real writes can
be partial.

Because each open has a separate offset but points at shared inode data, two
opens can overwrite the same bytes independently. ToyOS has one process and
no concurrency, so it avoids races accidentally rather than through locking.

## 6. Debug by state transition

When a file result is surprising, record a before/after table:

```text
path | inode index | writable | slot | fd | old offset
request length | available/capacity | result | new offset | new size
```

Check the first disagreement with your prediction. If lookup selected no
inode, inspecting copy-out is premature. If the descriptor is wrong, do not
debug inode capacity yet. If return count is correct but user output is wrong,
then inspect `copy_to_user`, the user buffer address, and the following serial
write.

The controlled missing-path profile proves only one negative branch. It does
not test slot exhaustion, read-only writes, capacity overflow, null buffers,
EOF, repeated opens, or data persistence across reboot. List those as future
tests instead of implying coverage.

## 7. Ramfs is not a disk filesystem

Every inode and data byte lives in kernel static memory. There is no block
device, sector address, buffer head, bio, request queue, page cache, dirty
tracking, writeback, superblock, free-space allocator, crash consistency, or
journal. Resetting QEMU discards all changes.

That does not make the exercise fake. It isolates naming, object identity,
open-file position, bounds, and user/kernel copying before adding storage
failure and persistence. The right claim is “a tiny in-memory filesystem path
works,” not “ToyOS has a storage stack.”

## Practical challenge

**Purpose:** account for every lookup and byte-range decision in ramfs.

**Prediction:** normal lookup/read/write completes; `/missing` returns `-2`
and the checked rejection profile exits with host status 47.

**Evidence:** canonical source excerpts, exact symbols, normal QEMU output,
and the dedicated filesystem profile.

**Limits:** the wrapper does not manufacture unimplemented close, seek,
directories, persistence, partial writes, or concurrent access.
