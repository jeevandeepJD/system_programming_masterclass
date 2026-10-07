# Day 5 — Safe User Copying, Pointer Validation, and Bad Pointers

**Target time:** approximately 3 hours

> A user pointer is a number supplied by an adversarial caller. Validate the
> translation and permission for every page before using the bytes.

## 1. Reject the tempting direct dereference

At syscall entry the kernel runs at CPL0. If it directly dereferences a user
register as a C pointer, the CPU applies supervisor access rules. That may
allow access to supervisor mappings the user could never touch, fault halfway
through a range, or create a confused validation/use sequence.

ToyOS instead routes syscall buffers through `copy_from_user` and
`copy_to_user` in `projects/toyos/src/memory.c`. These routines treat the
address as an integer in the active process address space and walk its page
tables explicitly.

Safe copying is both architecture and policy:

- architecture defines page-table translation and U/S/write bits;
- the helper ABI defines success/failure and direction;
- ToyOS policy caps every helper operation at 4096 bytes;
- callers decide which syscall lengths and descriptors are meaningful.

## 2. Walk all levels with user permission

`user_translate` starts from `active_root` and uses shifts 39, 30, 21, and 12.
At every level it requires both `PTE_PRESENT` and `PTE_USER`. For a
kernel-to-user copy, it also requires `PTE_WRITE`. It rejects huge entries.

At the leaf, it combines the physical page address with the 12-bit offset and
returns two values: a kernel-accessible pointer through the identity mapping
and the number of contiguous bytes left in that page.

Checking U/S at every level prevents a leaf under a supervisor-only parent
from being treated as valid user memory. Checking write permission for
`copy_to_user` prevents the kernel helper from silently bypassing intended
read-only user policy. In the canonical loader all user leaves are writable,
but the helper's contract is stronger than that current mapping policy.

The walk assumes four-level paging and identity access to paging structures
and data frames. It does not check canonicality before indexing. For the
small accepted user range this is adequate, but a hardened walker would
define address bounds and overflow behavior explicitly.

## 3. Copy page by page

`user_copy` loops until `length` reaches zero. For each iteration it translates
the current user address, computes:

```text
count = min(remaining length, bytes left in current page)
```

and copies exactly that run. It then advances the kernel pointer, user
address, and remaining count.

This handles a valid buffer that crosses page boundaries. It also detects a
missing or wrongly permitted second page rather than assuming the first
translation covers the full range.

There is a partial-copy semantic: if an early page succeeds and a later page
fails, bytes already copied remain changed, but the helper returns `false`
without reporting a count. For `copy_from_user`, the kernel's temporary
buffer may contain a prefix that callers must ignore on failure. For
`copy_to_user`, user memory may have been partially modified. Current small
canonical requests do not expose a transactional guarantee.

## 4. Apply two independent bounds

Both public copy helpers reject lengths above 4096. `syscall_dispatch` uses a
256-byte stack buffer and independently rejects I/O lengths larger than
`MAX_IO`. These bounds protect different resources:

```text
4096-byte helper bound: generic user-copy work limit
256-byte syscall bound: local kernel stack-buffer capacity
64-byte pipe bound: persistent IPC storage capacity
```

Do not merge them into one “maximum syscall size.” A write to serial can use
up to 256 bytes, while a write to a nearly full pipe can accept less or fail.
`SYS_PIPE` copies only two 32-bit descriptors.

The dispatcher converts `frame->rdx` to `size_t`; on x86-64 both are 64 bits.
It checks the bound before copying into the fixed array. That ordering is the
essential memory-safety property.

## 5. Understand the injected bad-pointer profile

With `EXPECT_FAULT=6`, `process_start` activates the process root and then
calls:

```text
copy_from_user(&byte, 0x1234, 1)
```

The address is not a valid user mapping. `user_translate` returns false,
`copy_from_user` returns false, and the assertion requires that rejection.
The kernel emits `EXPECTED-FAULT: user-pointer PASS` and exits with debug-exit
code `0x15`; the host harness expects status 43.

Despite the profile name, this is not an architectural page fault. It is an
injected invalid request handled by software validation. No CPL3 instruction
executes in this profile because the check happens before `enter_user`.

That behavior demonstrates a valuable boundary: invalid user input should
usually become a syscall error or controlled process fault, not an
unrecoverable kernel exception. ToyOS's test checks the lower-level rejection;
the normal dispatcher maps copy failure to `-14`.

## 6. Discuss fault-aware copying honestly

Real kernels may combine range checks, access-control state, exception-table
fixups, and architecture-specific user-access enable/disable mechanisms.
Mappings can change concurrently, pages may be demand-faulted, and copy
operations can return short counts. Spectre-class concerns and address-space
separation add more constraints.

ToyOS avoids concurrency, demand paging, process teardown, and SMP. Its page
tables remain stable during a syscall, and all relevant physical frames are
identity mapped. Therefore a software walk followed by physical-pointer copy
is sufficient for the selected model.

This does not prove resistance to malicious races, noncanonical inputs,
integer wrap at the end of an address range, huge user pages, or arbitrary
corrupted page tables. State these as limits rather than silently importing
Linux guarantees.

## 7. Debug pointer failures systematically

For a failed copy, record:

1. direction and requested length;
2. user start and calculated end, including overflow analysis;
3. active CR3 and `active_root`;
4. each page-table entry and required bits;
5. page offset and cross-page count;
6. syscall result and whether any prefix changed;
7. exact profile ELF and marker/status.

If an unexpected architectural #PF occurs, add CR2, error code, saved RIP,
and whether the fault happened while reading a table, touching the translated
frame, or directly dereferencing a user virtual address.

The source walker is easier to reason about when written as a table:

```text
level | index | entry | P | U/S | R/W | huge | next address
```

One row per level exposes the first violated invariant.

## Practical challenge

Use `challenges/day-005-user-pointer.sh` from the repository root.

**Purpose:** verify page-wise user copying and the selected invalid-pointer
rejection.

**Predict:** `0x1234` is rejected without an architectural #PF; the fault
profile emits its marker and host status 43.

**Evidence:** the wrapper runs normal and injected profiles, shows walker and
dispatcher contracts, and disassembles copy routines from the exact fault
ELF.

## Mastery prompts

Trace a 32-byte copy beginning 16 bytes before a page boundary when the second
page is absent. State which bytes can change in each direction and whether
the current API reports a partial count. Then explain why “the CPU will page
fault if it is bad” is not an adequate syscall validation policy.
