# Day 2 — User Address Spaces, U/S Permissions, and Kernel Sharing

**Target time:** approximately 3 hours

> A private root is not automatically a private system. Isolation comes from
> every reachable entry, every shared mapping, and every kernel access path.

## 1. Follow ownership from the root

`process_init` calls `address_space_create` and stores the returned pointer in
`init_process.root`. `frame_alloc` supplies one frame for the new PML4 and one
for a replacement first PDPT. Allocator frames are identity-mapped in this
teaching kernel, so physical addresses can also be dereferenced as kernel
virtual pointers.

The process owns these paging structures for the rest of the boot. There is
no reference count, teardown, or rollback. If the second allocation fails,
the first frame is leaked. That is an honest consequence of the compact
implementation, not a property of paging.

The function copies all 512 kernel PML4 entries and all 512 entries from the
bootstrap root's first PDPT. It then replaces `root[0]` with the new PDPT,
marked present and writable but supervisor-only. Thus the process receives
the existing low identity mappings needed by kernel code and data, while
gaining private lower-level structures beneath its first PML4 slot.

“Private PML4” is accurate. “Completely private address space” needs
qualification: many kernel mappings and their underlying physical frames are
shared. The copied entries are values pointing at common tables. ToyOS relies
on U/S permissions to keep CPL3 out.

## 2. Apply the U/S rule at every level

For a user access to a 4 KiB page, the U/S bit must permit user access through
every paging-structure entry in the walk. Setting `PTE_USER` only on the leaf
is insufficient if a PML4E, PDPTE, or PDE remains supervisor-only.

ToyOS handles this in `next_table`. When a user mapping creates an
intermediate table, that entry includes `flags & PTE_USER`. When the table
already exists in Week 8, the code ORs the user bit into the parent entry.
`map_in_root` repeats this at each level and installs the leaf with requested
low flags plus present.

This propagation has an important review question: what else is reachable
through an intermediate table whose parent was widened to user? Isolation
still depends on each leaf's U/S bit, but broad parent changes should be
audited. A mature kernel deliberately partitions user and kernel ranges and
defines which upper-level structures may be shared.

## 3. Separate virtual ownership from physical ownership

The address-space root belongs to the process, while copied kernel entries
refer to shared paging structures and mappings. Each ELF and user-stack leaf
gets a newly allocated physical frame. The process conceptually owns those
frames, although ToyOS has no structure that can enumerate and release them.

This yields several distinct claims:

```text
virtual range owner: process policy
page-table frame owner: process address-space implementation
user data frame owner: loaded process image or stack
kernel mapping owner: kernel, shared into the process root
active translation owner: CPU selected by CR3
```

Do not reduce all five to “the process owns memory.” Debugging leaks,
use-after-free, and cross-process exposure requires the narrower vocabulary.

## 4. Activate the root and understand TLB scope

`address_space_activate` saves the root in `active_root` and moves its address
to `CR3`. Loading `CR3` changes the hardware translation context and normally
invalidates relevant non-global translations. ToyOS does not use PCIDs,
global-page policy, multiple CPUs, or TLB shootdowns.

The software variable `active_root` is not authoritative hardware state. It
exists so `copy_from_user` and `copy_to_user` can walk the same root the
single-CPU kernel expects to be active. If future scheduling changed `CR3`
without changing `active_root`, safe-copy validation could inspect one
address space and execute against another. Their synchronized update is an
invariant.

`map_in_root` always executes `invlpg` after installing a leaf, even when
editing a non-active root. This is harmless for the current demonstration but
not a complete address-space-tagged invalidation design.

## 5. Locate the user image and stack

The linker places the embedded program near `0x40000000`. The loader accepts
segments beginning at or above that address and below the reserved stack
range. The stack top is `0x40020000`; four pages are mapped immediately below
it with user and write permission. The initial stack pointer equals the top,
so the first push enters the highest mapped page.

There is no guard page policy explicitly tested, no stack growth, no
argument/environment block, and no randomized layout. The gap between image
and stack is a fixed teaching layout, not ASLR.

The kernel's heap also begins at `0x50000000`, outside the accepted user
segment range, and its mappings are supervisor-only. The same numerical
address can have different meanings under different roots in a general
kernel; in this project, shared low kernel mappings constrain that freedom.

## 6. Use page faults as permission evidence

An x86 page-fault error code distinguishes not-present from protection and
user from supervisor access. A CPL3 access to a present supervisor-only page
should produce a protection fault with the user bit set in the error code.
ToyOS does not include a dedicated Week 8 profile that intentionally performs
that user instruction, so do not claim it was dynamically tested.

The invalid-user-pointer profile is different: while in the kernel, it asks
the software page-table walker to translate `0x1234`. The walker rejects the
missing user-accessible chain and returns `false`; no architectural #PF is
required. This is preferable for syscall pointer validation because the
kernel controls the failure and returns an error rather than blindly
dereferencing.

## 7. Compare designs without erasing architecture

xv6-riscv creates per-process page tables, maps user pages, and uses a
trampoline/trapframe arrangement for transitions. RISC-V PTE_U and `satp`
play related roles, but the walk shape and trap machinery differ.

Linux x86-64 has richer split-address-space policy, demand paging, VMAs,
copy-on-write, PCID-aware TLB management, hardened user-access routines, and
SMP synchronization. Configuration and mitigation choices can change how
kernel mappings are exposed in user page tables. Inspect a current kernel
rather than treating one historical layout as permanent.

ToyOS demonstrates the minimum causal chain: create a root, preserve kernel
execution reachability, map user leaves with user permission throughout the
walk, load CR3, and enter CPL3. It does not prove adversarial isolation.

## Practical challenge

Use `challenges/day-002-address-space.sh` from the repository root.

**Purpose:** audit private paging-structure ownership, shared kernel mappings,
U/S propagation, and CR3 activation.

**Predict:** the normal profile will emit the private-PML4/ELF marker and exit
with status 33; user leaves require U/S at every level.

**Evidence:** the wrapper shows current source, checks the canonical VM, and
prints symbols/disassembly from `build/week8/toyos.elf`.

## Mastery prompts

Draw one user and one kernel translation from the process root. Mark each
table/frame as private or shared. Then explain why “new PML4” alone proves
neither confidentiality nor correct kernel reachability.
