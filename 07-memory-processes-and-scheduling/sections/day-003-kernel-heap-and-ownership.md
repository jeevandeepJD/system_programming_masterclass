# Day 3 — Kernel Heap, Bump Allocation, and Ownership

**Target time:** approximately 3 hours

> Allocation answers “where?” Ownership answers “who may use it, for how long,
> and who must eventually release it?” ToyOS currently answers only part one.

## 1. Build the heap from lower layers

In `projects/toyos/src/memory.c`, `memory_init` obtains eight physical frames,
maps them at `HEAP_BASE` (`0x50000000`), then initializes `heap_next` and
`heap_end`. This order is important: returning a virtual pointer before
backing it with page tables would make the allocator's first metadata access
fault.

The heap is 32 KiB. It is virtual contiguous because the mapper installs
consecutive virtual pages; its physical frames need not be contiguous.

## 2. Read the bump allocator carefully

`kalloc` rejects zero bytes, aligns the next pointer to 16 bytes, checks both
the aligned start and remaining capacity, advances `heap_next`, and returns
the old aligned start. The subtraction form `bytes > heap_end - aligned`
avoids the overflow hazard of testing `aligned + bytes > heap_end`.

It does not clear each allocation, although newly allocated physical frames
were zeroed earlier. It does not record size, type, owner, or call site.
Alignment is sufficient for ToyOS task records and stacks, but it is policy,
not a guarantee suitable for every future object.

## 3. Understand the attractive limitation

A bump allocator is tiny, deterministic, and difficult to corrupt because it
has no free-list links. Allocation is constant-time. These are excellent
bootstrap properties.

The price is permanent consumption. `kalloc` has no `kfree`, reuse,
coalescing, guard pages, red zones, poisoning, or leak accounting. Repeated
task creation would exhaust the heap even after tasks finish. A failed
multi-step constructor also cannot roll back earlier allocations.

## 4. State ownership and lifetime in plain language

`create_task` in `projects/toyos/src/tasks.c` receives a task record and an
8192-byte stack. The scheduler's static `tasks[]` array retains pointers to
both for the kernel lifetime. No code frees either. That is acceptable only
because the demonstration creates exactly two permanent tasks.

For every pointer, write:

```text
allocator: kalloc
owner: scheduler/task subsystem
lifetime: kernel lifetime in this milestone
aliases: tasks[id], current, local construction pointer
release: none; bump heap cannot reclaim
locking: none; single CPU, initialization before interrupts
```

This makes hidden assumptions reviewable.

## 5. C debugging hazards

Kernel allocators amplify ordinary C mistakes. An incorrect `size_t`
calculation can under-allocate; pointer arithmetic can escape the object;
misalignment can violate ABI or hardware requirements; a stale alias can
outlive a future freed object. Compile-time warnings do not prove runtime
bounds.

When debugging, inspect `heap_next`, `heap_end`, requested `bytes`, and the
caller's expected object size. Use hexadecimal addresses and verify mappings.
Do not print from allocator code after introducing locks unless the logging
path is proven non-allocating and lock-safe.

## 6. Plan growth without pretending it exists

A next allocator might place headers in blocks, maintain size classes, or
allocate whole pages for large objects. Then ToyOS would need double-free and
use-after-free defenses, lock ownership, interrupt-context rules, and a
clear relationship between virtual heap pages and physical-frame reclamation.

Linux uses multiple allocators and policies rather than one universal heap;
source landmarks begin in `mm/`. xv6-riscv's `kernel/kalloc.c` is primarily a
physical-page allocator, so it is not a direct replacement for this byte
heap. Record source revisions before deeper claims.

## 7. Practical challenge

Use `challenges/day-003-kernel-heap.sh` from the repository root.

**Purpose:** audit heap construction, arithmetic, and current lifetime users.

**Predict:** compute capacity, alignment of successive small allocations, and
why task allocations are never reclaimed.

**Evidence:** the wrapper shows heap constants and `kalloc`, checks the
canonical heap self-test, and identifies all source call sites. The test is a
single successful allocation, not an exhaustion or concurrency proof.

## Mastery prompts

Explain why physical zeroing is not identical to allocation initialization,
design an ownership record for one task stack, and name the first new
invariant needed before adding `kfree`.
