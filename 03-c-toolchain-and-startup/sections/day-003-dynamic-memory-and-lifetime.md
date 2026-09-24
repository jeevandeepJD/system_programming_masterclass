# Day 3 — Dynamic Memory and Lifetime

**Tracker topic:** Week 17 — Dynamic Memory and Lifetime

**Target time:** approximately 3 hours
**Rhythm:** 55 minutes lifetime model · 45 minutes allocation interfaces · 55 minutes arena/diagnostics · 25 minutes mastery

> A pointer can still contain the same address bits after `free`. Why does an
> access that worked one instruction earlier become invalid?

## Why this day exists

Automatic local storage is excellent when lifetime follows a function call.
Real systems also create objects whose number and lifetime depend on input:
connections, syntax trees, packets, caches, and jobs. Their storage must
outlive one scope and eventually be reclaimed.

C separates mechanism from policy. `malloc` supplies storage; it does not
know the intended type, owner, graph, or cleanup time. This flexibility made
C practical for operating systems and runtimes, and it makes ownership bugs
possible.

```text
program needs runtime-sized or independently lived object
    → allocator reserves suitably aligned storage
    → program establishes object state
    → ownership rules govern aliases and transfer
    → last required use ends
    → cleanup releases resources exactly once
```

## Exact tracker objectives

- understand heap allocation interfaces;
- distinguish ownership from reachability;
- recognize leaks, double frees, and use-after-free;
- design cleanup paths;
- cover `malloc`, `calloc`, `realloc`, `free`, allocator metadata, ownership
  conventions, RAII alternatives in C, and error unwinding;
- implement a simple arena allocator;
- run Valgrind or sanitizers;
- refactor code to have explicit ownership rules.

**Mastery checkpoint:** for every allocated object, identify its owner,
lifetime, and cleanup path.

---

## 1. Storage duration is not “where bytes look like they are”

C defines storage duration—the minimum potential lifetime of storage—rather
than a universal stack/heap architecture:

- **automatic:** commonly block locals, usually lasting through block
  execution;
- **static:** lasts for the entire program execution;
- **thread:** one instance per thread for the thread's execution;
- **allocated:** obtained from an allocation function until deallocated.

Function-local automatic objects are commonly placed in a machine stack
frame, but the compiler may keep them only in registers, optimize them away,
or reuse stack slots. Variable-length arrays, `alloca` extensions, coroutine
runtimes, split stacks, and escape analysis make slogans unreliable.

Likewise, “the heap grows upward” is not a C guarantee or a complete Linux
model. A libc allocator may obtain virtual memory through `brk`, `mmap`, or
other mechanisms; maintain arenas, bins, caches, and metadata; and return
blocks from previously acquired regions.

Use precise layers:

```text
C allocated storage
    ↓ implemented by
libc allocator policy and metadata
    ↓ requests/mappings through
Linux virtual-memory interfaces
    ↓ backed lazily by
physical pages / swap / file mappings
```

### Lifetime and scope differ

**Scope** controls where an identifier is visible in source. **Lifetime**
controls when an object exists. A pointer returned from a function can be in
scope for the caller while its pointee is already dead:

```c
int *bad(void) {
    int local = 42;
    return &local;       /* local lifetime ends on return */
}
```

The address bits might still name mapped stack memory. That does not extend
the object's lifetime.

---

## 2. Ownership, aliasing, and reachability

C has no built-in ownership checker, so APIs need conventions.

- An **owner** is responsible for releasing a resource.
- A **borrower** may use it within an agreed interval but must not release or
  retain it beyond that interval.
- An **alias** is another access path to the same object.
- **Reachability** means some pointer path can still name the allocation.

Ownership and reachability are different:

```text
reachable but no owner     → eventual leak or ambiguous cleanup
owned but stale alias      → use-after-free risk
unreachable allocation     → definite leak
two claimed owners         → double-free risk
```

A linked graph can be fully reachable but have no documented component
responsible for destruction. A reference-counted object can have many aliases
and a precise shared-ownership rule. A cache may intentionally keep reachable
objects until shutdown.

Write contracts in API names/comments:

```text
create/open/new      returns owned resource
borrow/view/get      returns non-owning access unless documented otherwise
take/adopt           transfers ownership in
release/destroy/free consumes ownership
clone/dup            creates another owned resource
```

Names are conventions, not language enforcement. Tests and review must verify
them.

---

## 3. The allocation family

### `malloc`

```c
int *values = malloc(count * sizeof *values);
```

On success it returns suitably aligned storage of the requested size. The
bytes have indeterminate values. On failure it returns null.

Before multiplication, check overflow:

```c
if (count > SIZE_MAX / sizeof *values)
    return NULL;
```

Prefer `sizeof *values`; it follows the pointed-to type if the declaration
changes.

For a zero-size request, C permits behavior that requires care; do not build
an ownership protocol around dereferencing a result or assuming one universal
non-null policy. Normalize zero counts explicitly where useful.

### `calloc`

```c
int *values = calloc(count, sizeof *values);
```

It checks the conceptual element multiplication according to its interface
and initializes all bytes to zero. All-bits-zero represents integer zero, but
do not generalize that representation to every possible pointer or
floating-point representation in every C implementation.

### `realloc`

`realloc` can resize allocated storage, perhaps moving it. The safest common
shape is:

```c
void *temporary = realloc(buffer, new_size);
if (temporary == NULL) {
    /* old buffer remains owned and valid */
    handle_failure();
} else {
    buffer = temporary;
}
```

Do not assign directly to the sole owner:

```c
buffer = realloc(buffer, new_size); /* leaks old block if failure returns NULL */
```

On success, old pointers into the prior allocation become invalid even if the
numeric base address happens not to change; use the returned pointer. Bytes
up to the smaller old/new size are preserved. Newly added bytes are not
initialized.

### `free`

`free(NULL)` is a no-op. Otherwise, pass exactly a pointer value accepted by
the deallocation contract: normally the current base pointer returned by the
allocation family and not already freed.

After `free(p)`:

- the allocation's lifetime ends;
- aliases become dangling;
- reading, writing, or freeing through them is invalid;
- assigning `p = NULL` protects only that one variable, not other aliases.

The allocator may retain the physical pages or block for later reuse. “Free”
means the program relinquished the allocation; it does not promise immediate
zeroing or return to the OS.

---

## 4. Four failure classes

### Leak

An allocation remains unreleased after the program no longer needs it.
Long-running services turn small leaks into resource exhaustion.

### Use-after-free

An access occurs after lifetime ended. Reuse can make the stale pointer read
another object's data or corrupt allocator metadata.

### Double free

The same allocation is deallocated twice without an intervening new valid
ownership event. Allocator checks may abort, but the language does not promise
a friendly diagnostic.

### Interior/foreign free

Calling `free(p + 1)`, freeing automatic storage, or freeing a pointer from a
different allocation API violates the deallocator's contract.

Sanitizers and Valgrind improve detection. They do not make invalid programs
defined.

---

## 5. Allocator concepts without pretending to implement libc

A general allocator tracks free and used regions. Common design ideas include:

- block metadata, either adjacent to user storage or out-of-band;
- size classes/bins for quick reuse;
- splitting a large free block;
- coalescing adjacent free blocks;
- per-thread caches and multiple arenas for concurrency;
- alignment and minimum block sizes;
- fragmentation—free bytes exist but not in a useful shape;
- requesting/releasing larger virtual-memory regions from/to the kernel.

The pointer returned to the caller usually addresses usable storage, not the
allocator's complete block. Writing before or after it can corrupt metadata
or neighboring objects.

Linux's userspace allocator is not the kernel page allocator. Inside the
kernel, APIs such as `kmalloc`, slab caches, and page allocation have distinct
contexts and constraints. User code must not infer kernel allocation behavior
from glibc internals.

### Arena allocation

An arena obtains one backing region and serves aligned slices by bumping an
offset:

```text
base
  ↓
 +----------+---------+-------------+----------------------+
 | object A | padding | object B    | unused capacity      |
 +----------+---------+-------------+----------------------+
                         ↑ next offset
```

Advantages:

- very cheap allocation;
- no per-object free;
- one bulk cleanup;
- clear “all objects die together” lifetime.

Tradeoffs:

- cannot reclaim arbitrary individual objects;
- arena lifetime must dominate every borrowed pointer;
- large/long-lived object mixtures can waste storage;
- alignment and overflow must be correct.

This is an ownership strategy, not a replacement for every allocator.

---

## 6. Cleanup paths and RAII alternatives in C

C does not have standard C++-style destructors, but cleanup can be systematic.

### Single-exit cleanup ladder

```c
int process(void)
{
    int result = -1;
    FILE *file = NULL;
    unsigned char *buffer = NULL;

    file = fopen("input", "rb");
    if (file == NULL)
        goto out;

    buffer = malloc(4096);
    if (buffer == NULL)
        goto out;

    /* work */
    result = 0;

out:
    free(buffer);
    if (file != NULL)
        fclose(file);
    return result;
}
```

`goto` here expresses error unwinding, not arbitrary control flow. Every
resource is initialized to an empty state; cleanup runs in reverse acquisition
order; cleanup operations tolerate absence when possible.

Other patterns include:

- dedicated `*_init`/`*_destroy` pairs;
- owner structs collecting related resources;
- GCC/Clang cleanup attributes as nonportable extensions;
- scope macros, with care about control flow;
- arena/region lifetime;
- reference counting;
- generated defer mechanisms.

The standard-C baseline remains explicit cleanup.

### Design the cleanup before the happy path

For each acquisition, ask:

```text
Who owns it after success?
What happens if the next operation fails?
Can cleanup itself fail?
In what reverse order must resources be released?
Which aliases must stop being used?
```

This is especially important in kernel code, where locks, references, DMA
mappings, and device resources have strict context and order requirements.

---

## 7. Observe and build

Use:

- `03-c-toolchain-and-startup/challenges/day-003-arena-lab.c`
- `03-c-toolchain-and-startup/challenges/day-003-memory-errors.c`

Build the safe arena:

```bash
gcc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g \
  03-c-toolchain-and-startup/challenges/day-003-arena-lab.c \
  -o /tmp/week3-day3-arena

/tmp/week3-day3-arena
```

Predict offsets and alignment before running. Verify that out-of-capacity
allocation returns null without changing arena state.

Build the diagnostic fixture:

```bash
gcc -std=c17 -Wall -Wextra -Wpedantic -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  03-c-toolchain-and-startup/challenges/day-003-memory-errors.c \
  -o /tmp/week3-day3-errors

/tmp/week3-day3-errors safe
/tmp/week3-day3-errors leak
/tmp/week3-day3-errors uaf
/tmp/week3-day3-errors double-free
```

Each invalid mode runs only when explicitly selected. Expect nonzero exits for
detected UAF/double-free. Do not automate invalid modes in a script with
`set -e` unless you deliberately capture their expected failures.

If Valgrind is installed, build without ASan and run one mode at a time:

```bash
gcc -std=c17 -Wall -Wextra -Wpedantic -O0 -g \
  03-c-toolchain-and-startup/challenges/day-003-memory-errors.c \
  -o /tmp/week3-day3-valgrind

valgrind --leak-check=full --show-leak-kinds=all \
  /tmp/week3-day3-valgrind leak
```

Do not combine Valgrind and ASan in one run; they instrument memory through
different runtimes and are best used separately.

### Refactor task

Take this vague interface:

```c
char *load_text(const char *path);
```

Write a complete contract:

- ownership of the returned pointer;
- how length is returned;
- termination guarantee;
- empty file behavior;
- allocation-failure behavior;
- maximum/overflow policy;
- which function releases it.

Then implement it with one cleanup path and tests for missing, empty, and
small files.

---

## 8. Mastery — Explain · Draw · Observe · Build

### Explain

- Why are scope, storage duration, and lifetime different?
- Why is allocated storage not simply “RAM on the heap”?
- Why does reachability not prove ownership?
- Why must `realloc` use a temporary owner?
- Why can `free` finish while RSS does not immediately fall?
- Why does nulling one pointer not repair stale aliases?

### Draw

Draw:

1. pointer aliases around one allocation, marking owner and borrowers;
2. an error-unwinding ladder in reverse acquisition order;
3. libc allocator → Linux mappings → pages;
4. arena base, aligned objects, offset, and bulk lifetime boundary.

### Observe

Save the arena output, one ASan report, and one leak report. Annotate:

- allocation site;
- lifetime-ending event;
- invalid access or missing cleanup;
- ownership rule that would prevent it.

### Build

Complete the arena and `load_text` contract. For every allocated object in
your implementation, write:

```text
object | owner | borrowers | lifetime end | cleanup path | failure path
```

## Why notebook

1. Why does dynamic size require a lifetime policy, not merely `malloc`?
2. Why can a stale pointer retain plausible address bits?
3. Why is allocator metadata vulnerable to out-of-bounds writes?
4. Why is `calloc` not just a spelling convenience for every type?
5. Why is individual `free` absent from a bump arena?
6. Why should cleanup proceed in reverse acquisition order?
7. Why can sanitizers miss a bug in a particular execution?
8. Why do kernel allocators have context-specific APIs?

## References used selectively

- ISO/IEC 9899:2024 (C23), clauses 6.2.4 (storage durations), 7.24.3
  (memory management functions), and 7.24.2 (`memcpy`):
  <https://www.iso.org/standard/82075.html>
- WG14 working draft N3220:
  <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3220.pdf>
- Linux man-pages, `malloc(3)`, `free(3)`, `realloc(3)`, `calloc(3)`,
  `brk(2)`, and `mmap(2)`:
  <https://man7.org/linux/man-pages/man3/malloc.3.html>
- GCC and Clang sanitizer documentation:
  <https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html>
  <https://clang.llvm.org/docs/AddressSanitizer.html>
- Valgrind Memcheck manual:
  <https://valgrind.org/docs/manual/mc-manual.html>
- Linux kernel documentation, memory allocation guide:
  <https://docs.kernel.org/core-api/memory-allocation.html>
- Kaiwan N. Billimoria, *Hands-On System Programming with Linux* (2018),
  chapters on process virtual address space, dynamic memory, and memory
  debugging. Local catalog path:
  `references/library/books/02-os-and-linux/hands-on-system-programming-with-linux-kaiwan-billimoria-2018.pdf`
- Kaiwan N. Billimoria, legacy memory-management training, Part 2, conceptual
  process mappings only; implementation details require current verification:
  `references/library/training/kaiwantech/memory-management-legacy/02-kernel-and-process-memory-segments.pdf`

**Next bridge:** lifetime rules tell us when an object may be accessed.
Qualifiers, atomics, and the abstract machine determine what the compiler may
assume about those accesses—especially when devices or other threads are
involved.
