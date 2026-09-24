# Day 2 — Structures, Unions, and Function Pointers

**Tracker topic:** Week 16 — Structures, Unions, and Function Pointers

**Target time:** approximately 2 hours 50 minutes
**Rhythm:** 60 minutes layout and ABI · 35 minutes unions · 45 minutes callbacks · 30 minutes mastery

> A driver is not only data, and it is not only code. It is state plus a table
> of operations that another subsystem can call without knowing the concrete
> device. How can C express that contract?

## Why this day exists

Programs quickly outgrew flat sequences of identical values. A process,
packet, file, or device needs a record whose fields have different meanings.
Compilers and ABIs also need an agreed physical layout so separately compiled
code can exchange those records. Structures solve that problem.

Some storage must represent one of several alternatives. Unions provide
overlapping member storage, but do not magically record which alternative is
valid. Software must carry that invariant.

Finally, reusable systems code needs to invoke behavior selected at runtime.
Function pointers and callback tables provide C's direct mechanism:

```text
record related state
    → lay members out under alignment rules
    → represent a tagged alternative when needed
    → store addresses of compatible functions
    → select behavior through an operations table
```

## Exact tracker objectives

- predict structure layout;
- use unions safely;
- understand callbacks;
- model kernel-style interfaces;
- cover structures and padding, union type-punning caveats, function pointers,
  callbacks, opaque handles, and intrusive lists;
- implement a callback registry, generic linked list, and miniature operations
  table.

**Mastery checkpoint:** explain how function-pointer tables enable
polymorphism in C and in the Linux kernel.

---

## 1. Structure layout answers an ABI problem

```c
struct record {
    char tag;
    uint32_t count;
    uint16_t flags;
};
```

Members appear in declaration order and have distinct storage. An
implementation may insert unnamed padding between members and after the last
member. It may not insert padding before the first member.

Why padding? A type has an **alignment requirement**. On a common x86-64 ABI,
`uint32_t` wants an address divisible by 4. If `count` immediately followed
the one-byte `tag`, it would begin at offset 1. The implementation commonly
inserts three padding bytes:

```text
offset  0       1 2 3       4 5 6 7       8 9       10 11
       +------+-------+----------------+----------+---------+
       | tag  | pad   | count          | flags    | tail pad|
       +------+-------+----------------+----------+---------+
```

Typical result for this ABI:

```text
offsetof(tag)   = 0
offsetof(count) = 4
offsetof(flags) = 8
sizeof(record)  = 12
alignof(record) = 4
```

Tail padding makes every element of `struct record array[N]` correctly
aligned:

```text
address of array[i] = base + i × sizeof(struct record)
```

These numbers are observations for a target ABI, not universal C constants.
Use `sizeof`, `_Alignof`/`alignof`, and `offsetof`; do not guess serialized
formats from one compiler run.

### Reordering can change size

Placing larger-alignment members first often reduces holes:

```c
struct compact {
    uint32_t count;
    uint16_t flags;
    char tag;
};
```

But member order may be externally fixed by an ABI, protocol, hardware
register layout, or file format. Smaller is not automatically more correct.
Reordering public structures can break binary compatibility.

### Padding bytes are not ordinary semantic fields

Whole-structure assignment copies the value as C defines it, but raw padding
bytes can hold unspecified values. Comparing two structures with `memcmp`
can report inequality even when all members compare equal. Writing a struct
directly to disk or the network also bakes in:

- padding;
- endianness;
- integer widths;
- alignment/layout choices;
- possible uninitialized-byte disclosure.

Serialize fields explicitly into a defined byte format.

### Packed is a contract with costs

Compiler attributes such as `__attribute__((packed))` are extensions, not a
portable cure. Packed members can be misaligned. Some architectures trap;
others perform slower accesses; compilers may need byte operations. Linux
uses packed layouts when an external binary contract requires them, not as a
default optimization.

---

## 2. Structures connect to machine addressing

For:

```c
uint32_t get_count(const struct record *r) {
    return r->count;
}
```

`r->count` means `(*r).count`. The compiler knows `count`'s offset from the
structure layout and can emit a load at:

```text
address in r + offsetof(struct record, count)
```

The ABI is the bridge between translation units. If a shared library and an
application disagree about packing options or structure definitions, the
same member name can lead to different offsets and corrupted communication.

Linux exposes many internal structures but carefully versioned userspace
interfaces should use fixed-width fields and explicit compatibility rules.
Kernel layout is not a stable userspace ABI merely because headers can be
read.

### Flexible array members

A final incomplete array member can describe trailing storage:

```c
struct message {
    size_t length;
    unsigned char payload[];
};
```

Allocate enough for header plus payload with overflow checks. `sizeof(struct
message)` excludes payload elements and may include layout padding. This is
not the historical “struct hack” of a one-element array.

---

## 3. Unions overlap storage

```c
union payload {
    uint32_t number;
    float real;
    unsigned char bytes[4];
};
```

All members begin at the same address. The union is large and aligned enough
for its largest/strictest member, subject to implementation rules. Only one
member's value is stored at a time in the straightforward model.

The union does not remember which member is intended. Build a **tagged
union**:

```c
enum value_kind { VALUE_NUMBER, VALUE_REAL };

struct value {
    enum value_kind kind;
    union {
        uint32_t number;
        float real;
    } as;
};
```

Every write must update the member and tag consistently; every read must
switch on the tag. The invariant is software policy.

### Valid interpretation and type-punning caveats

C permits carefully specified inspection of a union member other than the one
most recently stored, with constraints and implementation-sensitive results
for some representations. Common-initial-sequence rules allow limited
inspection for compatible structure members inside a union. None of this
means “unions make every cast safe.”

For converting object representations to bytes, character types have special
permission to inspect representation. For bit-preserving conversion between
unrelated scalar types, `memcpy` into a destination object is usually the
clearest portable technique, while still requiring that the copied bits form
a valid representation for that type.

Avoid teaching:

```text
write any member → read any other member → portable reinterpretation
```

Also distinguish a union's overlapping storage from strict-aliasing rules
applied through unrelated pointer types. A pointer cast does not create an
object of the target type.

---

## 4. Function pointers are typed code-entry references

```c
int add(int a, int b) { return a + b; }
int (*operation)(int, int) = add;
int result = operation(2, 3);
```

Read the declaration inside out: `operation` is a pointer to a function that
takes two `int` arguments and returns `int`.

The function designator `add` converts to a pointer to the function in most
expressions. Calling through the pointer requires a compatible function type.
A cast can silence a diagnostic but cannot repair an incompatible calling
sequence. Wrong parameter or return types create undefined behavior.

Use a typedef to make interfaces readable:

```c
typedef int (*binary_operation)(int, int);
```

Object pointers and function pointers are distinct categories in portable C.
POSIX `dlsym` lives at an implementation boundary with POSIX guarantees;
do not generalize its idioms into ISO C.

### Callback: invert control deliberately

A callback API receives behavior to invoke later or for each item:

```c
typedef void (*visitor_fn)(int value, void *context);

void visit(const int *items, size_t count,
           visitor_fn visitor, void *context);
```

The `void *context` carries caller-owned state without globals. Its lifetime
must include every callback invocation. The callback's contract should state:

- when and how often it runs;
- whether it may unregister itself;
- thread/reentrancy rules;
- ownership of arguments;
- how errors or cancellation propagate.

Function type compatibility is only the first safety condition.

---

## 5. Operations tables: polymorphism made explicit

```c
struct stream_ops {
    int (*read)(void *state, unsigned char *dst, size_t capacity);
    int (*close)(void *state);
};

struct stream {
    void *state;
    const struct stream_ops *ops;
};
```

One concrete stream can use memory-backed functions; another can use
file-backed functions. Generic code performs:

```c
stream->ops->read(stream->state, buffer, sizeof buffer);
```

Trace the call:

```text
stream object
  ├── state ─────────→ concrete private state
  └── ops ───────────→ immutable operation table
                         └── read function pointer
                               ↓ indirect call
```

This is runtime polymorphism without language-level classes. It works because
each implementation honors the same function signatures and semantic
contract.

Linux relies on this shape extensively: `struct file_operations`, network
device operations, filesystem inode operations, clocks, buses, and many
subsystems separate a common framework from implementation-specific behavior.
The indirect call is only the mechanism; lifetime, locking, module ownership,
and error contracts make it safe.

### Opaque handles

A public header can declare:

```c
struct registry;
struct registry *registry_create(void);
void registry_destroy(struct registry *);
```

Clients can carry the pointer but cannot access members because the structure
definition remains private to the implementation. This reduces coupling and
allows layout changes without recompiling code that only uses the API—subject
to the library's ABI strategy.

### Intrusive lists

An intrusive list embeds a linkage node inside the owned object:

```c
struct task {
    int id;
    struct list_node link;
};
```

Given a `link` pointer, `container_of`-style logic recovers the enclosing
`task` from the member offset. This avoids a separate wrapper allocation and
supports one object in multiple lists through multiple link members.

The tradeoff is stronger coupling and stricter lifetime discipline. Removing
the node does not necessarily destroy the enclosing object; freeing the
object while linked leaves dangling list pointers.

---

## 6. Observe and build

Use `weekly/week-003/challenges/day-002-layout-callback-lab.c`.

```bash
gcc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g \
  weekly/week-003/challenges/day-002-layout-callback-lab.c \
  -o /tmp/week3-day2

/tmp/week3-day2 layout
/tmp/week3-day2 union
/tmp/week3-day2 callbacks
```

Before `layout`, draw offsets for all members and predict total size and
alignment. Then compare with `offsetof`, `sizeof`, and `_Alignof`.

Inspect member addressing and indirect calls:

```bash
objdump -d -Mintel --disassemble=get_count /tmp/week3-day2
objdump -d -Mintel --disassemble=apply_operation /tmp/week3-day2
```

Find:

- the displacement matching `offsetof(count)`;
- the indirect call through a register or memory operand;
- the ABI registers carrying callback arguments.

### Build tasks

1. Add one callback operation without changing `apply_operation`.
2. Add a fixed-capacity callback registry with duplicate-name rejection.
3. Complete the provided intrusive-list traversal and unlink one node.
4. Add a tagged-union string alternative with explicit ownership: either a
   borrowed immutable view or an owned allocation, but not an unnamed mix.

---

## 7. Mastery — Explain · Draw · Observe · Build

### Explain

- Why can structure padding be needed between and after members?
- Why is `memcmp` not a general structure-value comparison?
- Why is a union insufficient without a tag for variant data?
- Why must a callback type and its semantic contract both match?
- How does an operations table enable C polymorphism?
- Why can changing a public structure layout break an ABI?

### Draw

Draw one page containing:

1. a structure with offsets, alignment boundaries, and tail padding;
2. a tagged union with legal states;
3. object → ops table → selected function → concrete state;
4. an intrusive node embedded in two enclosing objects.

### Observe

Record actual layout values, one member-load instruction, and one indirect
call. State which facts come from ISO C and which come from this compiler/ABI.

### Build

Demonstrate the registry and miniature operations table. Test:

- valid lookup;
- unknown operation;
- full registry;
- duplicate name;
- callback receiving caller context.

## Why notebook

1. Why does an array of structures require tail padding?
2. Why should protocols not send raw in-memory structs?
3. Why is packed layout an external-contract tool rather than a default?
4. Why does a tagged union make invalid states easier to detect?
5. Why is casting an incompatible function pointer not a fix?
6. Why does callback context reduce global state?
7. Why are kernel operations tables more than “a list of addresses”?
8. Why does an intrusive list transfer lifetime responsibility to the owner?

## References used selectively

- ISO/IEC 9899:2024 (C23), clauses 6.2.5, 6.2.6, 6.5, 6.7.3, 6.7.2.1
  (structure/union specifiers), 6.7.7 (declarators), and 7.24.2 (`memcpy`):
  <https://www.iso.org/standard/82075.html>
- WG14 working draft N3220:
  <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3220.pdf>
- System V AMD64 ABI, data representation and calling sequence:
  <https://gitlab.com/x86-psABIs/x86-64-ABI>
- GCC, “Common Type Attributes,” including `packed` and alignment extensions:
  <https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html>
- Linux kernel documentation, “Data structures and low-level utilities,”
  linked lists and `container_of`:
  <https://docs.kernel.org/core-api/kernel-api.html>
- Linux kernel source, `include/linux/fs.h`, `struct file_operations`, as a
  current operations-table example:
  <https://github.com/torvalds/linux/blob/master/include/linux/fs.h>
- Randal E. Bryant and David R. O'Hallaron, *Computer Systems: A Programmer's
  Perspective*, 3rd ed., Chapter 3 §§3.10–3.11 on heterogeneous data
  structures and alignment. Local catalog path:
  `source-materials/library/books/01-foundations-and-architecture/computer-systems-programmers-perspective-bryant-ohallaron-3e-global.pdf`

**Next bridge:** a record can point to state, but who owns that state, when
does its lifetime begin, and which path releases it? Day 3 makes allocation
and cleanup contracts explicit.
