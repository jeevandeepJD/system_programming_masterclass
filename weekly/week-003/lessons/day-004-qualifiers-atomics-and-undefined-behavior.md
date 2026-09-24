# Day 4 — Qualifiers, Atomics, and Undefined Behavior

**Tracker topic:** Week 18 — Qualifiers, Atomics, and Undefined Behavior

**Target time:** approximately 2–3 hours
**Rhythm:** 40 minutes qualifiers · 45 minutes behavior/optimization · 45
minutes atomics · 35–50 minutes labs and mastery

> If the CPU can execute a load, why may the compiler remove it? If two CPU
> cores can both see one address, why is an ordinary C data race undefined?

## Why this day exists

Day 3 established that an object may be accessed only during its lifetime.
That is necessary, but not sufficient. The compiler also reasons about the
object's type, qualifiers, aliases, side effects, and cross-thread ordering.

C is not portable assembly. It specifies an **abstract machine** and permits
any translation that preserves the observable behavior of a program whose
behavior is defined. This freedom enables optimization. It also means that a
source operation cannot be used for a job its language semantics do not
perform.

```text
source-level contract
  ├─ lifetime and type rules
  ├─ qualifier promises
  ├─ sequencing and observable side effects
  └─ atomic synchronization
          ↓ compiler maps the contract
target ISA instructions
          ↓ architecture-specific memory system
caches, interconnect, devices, and cores
```

The layers connect, but they are not interchangeable. A strong hardware
memory model does not define a C data race. A `volatile` load is not an atomic
operation. One instruction on one target is not a portable C guarantee.

## Tracker contract

By the end, you should be able to:

- state the precise roles of `const`, `volatile`, and `restrict`;
- explain why `volatile` is not thread synchronization;
- distinguish undefined, unspecified, and implementation-defined behavior;
- connect observable behavior to compiler optimization;
- recognize signed-overflow, aliasing, sequencing, race, and lifetime UB;
- use C atomics for a relaxed counter and release/acquire publication;
- distinguish the C memory model from an ISA or device-memory model.

**Mastery checkpoint:** explain how the compiler's freedom under the C
abstract machine affects systems code.

---

## 1. Qualifiers are contracts, not decorations

### `const`: no modification through this access path

```c
int checksum(const unsigned char *data, size_t length);
```

Here `const` says the function does not modify the bytes through `data`.
It improves an API contract and lets the function accept genuinely
nonmodifiable objects. It does **not** necessarily mean:

- the object lives in read-only physical memory;
- no other non-const alias can modify the object;
- the value is a translation-time constant;
- a load must happen every time the identifier appears.

```c
int value = 1;
const int *view = &value;
value = 2;                 /* valid: value itself was not defined const */
printf("%d\n", *view);     /* observes 2 */
```

By contrast, casting away `const` and modifying an object that was defined
with a const-qualified type has undefined behavior:

```c
const int fixed = 1;
int *forced = (int *)&fixed;
*forced = 2;               /* undefined behavior */
```

Read declarations from the identifier outward:

```c
const int *p;        /* pointer to const int */
int *const p2 = ...; /* const pointer to int */
```

### `volatile`: accesses are externally significant

A volatile-qualified object tells the implementation that accesses are
observable side effects and must be evaluated according to the abstract
machine's volatile rules. Typical legitimate uses include:

- device registers in a platform where the implementation and device API
  define volatile access as appropriate;
- objects changed by a signal handler, using the narrow types and operations
  the signal rules permit;
- low-level implementation interfaces that explicitly require it.

It does not provide:

- atomicity for a multi-byte or read-modify-write operation;
- mutual exclusion;
- a cross-thread happens-before relation;
- a portable compiler or CPU memory fence;
- cache coherence beyond what the platform already defines.

```c
volatile int ready;
int payload;

/* Thread A */ payload = 42; ready = 1;
/* Thread B */ while (!ready) {} printf("%d\n", payload);
```

This is not a C synchronization protocol. The volatile accesses remain
volatile, but the ordinary conflicting cross-thread accesses have no atomic
synchronization. If they form a data race, behavior is undefined.

For memory-mapped I/O, `volatile` may be one ingredient, but device ordering,
access width, barriers, endianness, and mapping attributes come from the
platform. Linux kernel helpers such as `readl()`/`writel()` embody kernel and
architecture contracts; they are not merely ISO C volatile expressions.

### `restrict`: an aliasing promise made by the programmer

For a pointer used to access an object during a block execution, `restrict`
can promise that relevant accesses to that object are based on the designated
pointer rather than competing independent access paths. That lets a compiler
avoid repeatedly assuming that stores through one parameter change values
read through another.

```c
void add(size_t n, int *restrict out,
         const int *restrict left, const int *restrict right)
{
    for (size_t i = 0; i < n; ++i)
        out[i] = left[i] + right[i];
}
```

Calling this with appropriately separate arrays honors the contract. Calling
it so that the restricted access paths overlap in a way forbidden by the
contract produces undefined behavior. `restrict` does not check pointers at
runtime and does not magically make them non-overlapping.

The exact standard wording is more nuanced than “these pointers never
alias.” Use that slogan only as an initial intuition; reason about which
objects are accessed, whether they are modified, and which pointer
expressions those accesses are based on.

---

## 2. Four behavior categories

These labels answer different specification questions.

### Defined behavior

The standard imposes requirements. Unsigned arithmetic is reduced modulo one
more than the type's maximum value. A conforming implementation must preserve
that result.

### Implementation-defined behavior

The implementation chooses one allowed behavior and documents the choice.
Examples include whether plain `char` has the same range as `signed char` or
`unsigned char`, and many properties of integer types.

This is not automatically erroneous. It is a portability dependency to find,
document, or avoid.

### Unspecified behavior

The standard allows two or more possibilities and does not require the
implementation to document which occurs in a particular instance. One common
example is which function argument is evaluated first when no rule imposes an
order.

Unspecified does not mean “anything can happen.” The result must still be
one of the permitted possibilities, assuming no separate undefined behavior
is triggered.

### Undefined behavior (UB)

The standard imposes no requirements after the program executes an undefined
operation. Examples relevant today include:

- signed integer overflow;
- an out-of-bounds access;
- use after an object's lifetime ends;
- a forbidden access through an incompatible type;
- violating an applicable `restrict` association;
- unsequenced conflicting side effects, such as `i = i++ + 1`;
- a data race between threads.

UB is not a predictable fallback result, a guaranteed crash, or necessarily
a hardware trap. A test may appear to work until optimization, input,
compiler, or surrounding code changes.

### A compact classification drill

| Situation | Category |
|---|---|
| `UINT_MAX + 1U` | defined modulo arithmetic |
| whether plain `char` is signed | implementation-defined |
| order of many argument evaluations | unspecified |
| `INT_MAX + 1` as `int` | undefined |
| read after `free` | undefined |
| racing ordinary reads/writes | undefined |

The classification belongs to the C specification. The CPU may use the same
addition instruction for a defined unsigned addition and an undefined signed
overflow.

---

## 3. Observable behavior and optimization

The compiler need not preserve source text step for step. It must preserve
the required observable behavior of a defined program. Depending on the C
version and execution environment, that includes specified interactions such
as volatile accesses, file/stream effects, and atomic operations.

```c
int square_and_discard(int x)
{
    int result = x * x;
    return 0;
}
```

This is not harmless for every `x`: the signed multiplication can overflow.
If the compiler proves the result is unused, it may remove the calculation,
and then no overflowing evaluation occurs at runtime. Source appearance alone
does not prove an operation survives translation.

Now consider:

```c
int greater_after_increment(int x)
{
    return x + 1 > x;
}
```

For every execution in which `x + 1` is defined, the comparison is true.
The compiler may return true directly. “But the machine instruction wraps at
`INT_MAX`” applies a CPU fact where the C contract gave no wrapping result.
Use an unsigned type or an explicit checked operation when wrapping or
overflow detection is the intent.

### Aliasing example

Accessing stored data through an incompatible lvalue type can violate C's
aliasing/type-access rules:

```c
float value = 1.0f;
unsigned int bits = *(unsigned int *)&value; /* not a portable bit copy */
```

Use `memcpy` to copy an object representation into a same-sized destination,
or use character-type access to inspect representation bytes:

```c
uint32_t bits;
_Static_assert(sizeof bits == sizeof value, "lab assumption");
memcpy(&bits, &value, sizeof bits);
```

This tells the optimizer the real operation instead of hiding it behind an
invalid type claim.

### Lifetime example

An address is not an eternal permission token. After `free(p)`, or after an
automatic object leaves its lifetime, dereferencing a stale pointer is
undefined even if:

- its numeric bits have not changed;
- the virtual page is still mapped;
- the bytes still look familiar;
- the allocator has not reused the region.

The compiler reasons from object lifetime, while the MMU translates virtual
addresses. Those are different layers.

### Sanitizers are observers, not semantics

AddressSanitizer and UndefinedBehaviorSanitizer instrument selected
operations. They catch many executed bugs, not every possible UB. A clean run
does not prove a program is defined; an optimizer may also remove an operation
before instrumentation can observe it. Use sanitizers alongside contracts,
warnings, tests, and review.

---

## 4. C atomics: indivisibility plus ordering

Include `<stdatomic.h>` and use an atomic type:

```c
atomic_uint jobs_done;
atomic_init(&jobs_done, 0U);
atomic_fetch_add_explicit(&jobs_done, 1U, memory_order_relaxed);
```

Atomic operations prevent torn/conflicting access to that atomic object and
participate in the language's ordering model. Atomic does not necessarily
mean one machine instruction or lock-free execution. Ask
`atomic_is_lock_free` when that property matters; the implementation may use
library support or locks.

### Data race

In simplified form, two conflicting actions in different threads constitute
a data race when:

- they access the same memory location;
- at least one modifies it;
- they are not atomic;
- neither happens before the other.

A data race is undefined behavior. Cache coherence may eventually propagate
bytes, but coherence is not a substitute for the C happens-before relation.

### `memory_order_relaxed`

Relaxed operations remain atomic and each atomic object has a modification
order, but relaxed ordering does not publish surrounding ordinary data.

Use it when only the atomic value itself matters, such as an independent
statistics counter:

```c
atomic_fetch_add_explicit(&requests, 1U, memory_order_relaxed);
```

### Release/acquire publication

```text
producer                                  consumer
ordinary write: payload = 2026
release store: ready = true    ───────▶   acquire load observes true
                                            ordinary read: payload

producer actions sequenced before release
      happen before
consumer actions sequenced after matching acquire
```

When an acquire operation reads the value from the relevant release sequence,
the release synchronizes with the acquire. Earlier producer actions then
happen before later consumer actions. This is why the non-atomic payload in
the Day 4 lab can be read safely after the matching acquire.

### `memory_order_seq_cst`

Sequentially consistent operations provide the strongest standard ordering:
in addition to acquire/release effects as applicable, sequentially consistent
operations participate in a single total order consistent with required
thread order. It is a useful default while learning, but it does not turn a
multi-step algorithm into a transaction.

Learn in this order:

1. make shared access race-free;
2. state the invariant and required happens-before edges;
3. begin with mutexes or sequential consistency where appropriate;
4. weaken ordering only with a proof and measurements.

Avoid `memory_order_consume` in introductory designs; its specification and
implementation history make acquire the practical teaching baseline.

---

## 5. C model, compiler, CPU, and kernel

One source operation can map differently by target:

```text
C acquire load
  → compiler ordering constraints
  → perhaps an ordinary load on a sufficiently strong ISA
  → perhaps a load-acquire instruction or barrier on another ISA
```

That difference does not make acquire “free” in the language model. It means
the target ISA already supplies some required ordering.

Keep these statements separate:

- **C rule:** a data-race-free program uses language/library synchronization.
- **Compiler mapping:** preserves the C contract while optimizing.
- **ISA rule:** defines architectural load/store ordering and atomic
  instructions.
- **microarchitecture:** implements the ISA with caches, buffers, and
  coherence.
- **kernel/device rule:** supplies APIs for locks, MMIO, DMA, interrupts, and
  architecture barriers.

Linux kernel code uses a kernel memory model and primitives including locks,
atomics, barriers, `READ_ONCE()`, and `WRITE_ONCE()`. Their meanings come from
kernel documentation and supported compiler behavior, not directly from ISO
C atomics. Conversely, copying kernel idioms into portable userspace without
their contracts is unsafe.

`volatile` remains useful for its intended observable-access role. It is not
a weaker spelling of `_Atomic`, and `_Atomic` is not a general device-register
API.

---

## 6. Observe

### Qualifiers and explicit UB modes

Build the defined default:

```bash
gcc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g \
  weekly/week-003/challenges/day-004-qualifiers-ub-lab.c \
  -o /tmp/week3-day4-qualifiers

/tmp/week3-day4-qualifiers
```

Compare generated code:

```bash
gcc -std=c17 -Wall -Wextra -Wpedantic -O0 -S \
  weekly/week-003/challenges/day-004-qualifiers-ub-lab.c \
  -o /tmp/day4-O0.s
gcc -std=c17 -Wall -Wextra -Wpedantic -O2 -S \
  weekly/week-003/challenges/day-004-qualifiers-ub-lab.c \
  -o /tmp/day4-O2.s
diff -u /tmp/day4-O0.s /tmp/day4-O2.s
```

The unsafe modes are opt-in only. Rebuild with diagnostics before selecting
one:

```bash
gcc -std=c17 -Wall -Wextra -Wpedantic -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  weekly/week-003/challenges/day-004-qualifiers-ub-lab.c \
  -o /tmp/week3-day4-ub

/tmp/week3-day4-ub overflow
/tmp/week3-day4-ub lifetime
```

`restrict-overlap` deliberately breaks a language contract, but a sanitizer
is not required to diagnose that class. Never infer validity from one output.

### Safe atomics

```bash
gcc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g -pthread \
  weekly/week-003/challenges/day-004-atomics-lab.c \
  -o /tmp/week3-day4-atomics

/tmp/week3-day4-atomics
```

Predict first:

1. What exact counter value is required?
2. Why is relaxed enough for that counter?
3. Which operation publishes `payload`?
4. Would replacing `ready` with `volatile` preserve the C guarantee?
5. Must `atomic_uint` report lock-free?

Optional: run the safe atomic lab under ThreadSanitizer where the installed
compiler/runtime supports it:

```bash
clang -std=c17 -Wall -Wextra -Wpedantic -O1 -g -pthread \
  -fsanitize=thread \
  weekly/week-003/challenges/day-004-atomics-lab.c \
  -o /tmp/week3-day4-tsan
/tmp/week3-day4-tsan
```

Runtime support varies by platform; tool availability is not part of the C
guarantee.

---

## 7. Mastery — Explain · Draw · Observe · Build

### Explain

- Explain `const`, `volatile`, and `restrict` without using “makes it safe.”
- Explain why volatile polling is not thread synchronization.
- Distinguish undefined, unspecified, and implementation-defined behavior.
- Explain why signed machine addition wrapping does not define signed C
  overflow.
- Explain why an atomic operation need not be lock-free.
- Explain the difference between atomicity and ordering.

### Draw

Draw two diagrams:

1. C abstract machine → optimizer → ISA → microarchitecture → device/kernel
   API. Label which layer owns each guarantee.
2. Producer payload write → release store → acquire load → payload read.
   Mark **sequenced-before**, **synchronizes-with**, and **happens-before**.

### Observe

Save:

- warning-clean GCC and Clang build commands for both labs;
- one `-O0`/`-O2` assembly comparison;
- one sanitizer report from an explicitly selected UB mode;
- atomic output showing the exact counter and publication value.

For each observation, state what it demonstrates and what it does **not**
prove.

### Build

Extend the atomic lab with a fixed-size one-producer/one-consumer mailbox:

- payload writes happen before consumption;
- no ordinary shared access races;
- shutdown is represented explicitly;
- every memory order has a one-sentence justification.

First implement it with a mutex and condition variable. Then implement the
single-slot version with release/acquire atomics. Compare correctness
arguments before performance.

## Why notebook

1. Why can `const int *` still observe a changed value?
2. Why can volatile be correct for an implementation-defined device access
   but wrong for thread publication?
3. Why is a violated `restrict` promise the caller's correctness bug?
4. Why does UB allow optimization before any bad CPU instruction executes?
5. Why can an address remain mapped after its C object lifetime ends?
6. Why does relaxed ordering suit a standalone counter but not payload
   publication?
7. Why can acquire compile to different instructions on x86-64 and AArch64?
8. Why do kernel memory-ordering primitives require their own documentation?

## References used selectively

Current language and compiler references:

- ISO/IEC 9899:2024 (C23), the current published C standard:
  <https://www.iso.org/standard/82075.html>
- WG14 N3220 working draft, especially 5.1.2.4 (multi-threaded executions and
  data races), 6.2.4 (object lifetime), 6.7.4 (type qualifiers), and 7.17
  (atomics):
  <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3220.pdf>
- GCC 16.2 manuals, “When is a Volatile Object Accessed?”, restricted
  pointers, C implementation-defined behavior, optimizer options, and
  instrumentation:
  <https://gcc.gnu.org/onlinedocs/gcc/Volatiles.html>
  <https://gcc.gnu.org/onlinedocs/gcc/Restricted-Pointers.html>
  <https://gcc.gnu.org/onlinedocs/gcc/C-Implementation.html>
  <https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html>
  <https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html>
- Clang documentation for the language extensions and sanitizers:
  <https://clang.llvm.org/docs/LanguageExtensions.html>
  <https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html>
  <https://clang.llvm.org/docs/AddressSanitizer.html>
  <https://clang.llvm.org/docs/ThreadSanitizer.html>

Current systems references:

- Linux kernel documentation, “Why the `volatile` type class should not be
  used” (with its documented exceptions):
  <https://docs.kernel.org/process/volatile-considered-harmful.html>
- Linux kernel memory-barrier documentation:
  <https://docs.kernel.org/core-api/wrappers/memory-barriers.html>
- Linux kernel MMIO access documentation:
  <https://docs.kernel.org/driver-api/device-io.html>
- Linux Kernel Memory Model explanation and tooling:
  <https://docs.kernel.org/dev-tools/lkmm/index.html>

**Next bridge:** today the compiler transformed one translation unit under C
rules. Day 5 follows the artifacts through preprocessing, compilation,
assembly, symbols, sections, and relocations.
