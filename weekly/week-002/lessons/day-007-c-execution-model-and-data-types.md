# Day 7 — C's Execution Model and Data Types

**Curriculum alignment:** Stage 5 — C from the Machine's Perspective · C
Execution Model and Data Types

**Target time:** approximately 2–3 hours

- C's abstract machine and environments: 30 minutes
- objects, values, types, bytes, and integers: 45 minutes
- conversions, expressions, and floating-point orientation: 35 minutes
- compile/run/disassemble lab: 45–60 minutes
- scope, lifetime preview, and mastery evidence: 20 minutes

> The processor executes instructions, not C statements. The C standard
> describes an abstract machine, not your particular x86-64 laptop. So what
> exactly did you ask for when you wrote `int x = -1;`, and which parts may a
> compiler choose?

This week moved from assembly instructions through caches, layout, and byte
order. Today we climb back to C—but with enough machine knowledge to avoid a
dangerous shortcut:

> A C program constrains observable behavior. It does not prescribe one
> instruction per source line, one register per variable, or even that every
> source object occupies memory throughout execution.

That distinction is the point of the lesson.

## Tracker contract

By the end, you should be able to:

- connect C types to representation and generated machine operations;
- describe the C abstract machine and distinguish translation from execution;
- distinguish objects, values, types, representations, and identifiers;
- reason about integer ranges, rank, promotions, and common conversions at an
  introductory level;
- explain what C means by a byte and why `char` is special;
- recognize implementation-defined choices and avoid assuming every choice is
  universal;
- predict selected expression results without relying on accidental evaluation
  order;
- preview scope, storage duration, and lifetime;
- inspect generated assembly without expecting a line-by-line transcription.

The tracker mastery checkpoint is:

> **Explain why C source does not fully specify every machine-level detail.**

Today's sharper evidence question is:

> **Given a C expression, explain how its types affect the width,
> interpretation, extension, comparison, and instruction family that a
> compiler may choose.**

---

## 1. Why C needs a machine that does not physically exist

If C directly described one 1970s processor, programs would become obsolete
with that processor. If it described only pure mathematics, it would be a poor
language for device registers, packed bytes, operating systems, and memory
allocators.

C instead defines an **abstract machine**. It is a specification model with:

- objects that hold values;
- types that constrain values and operations;
- expressions whose evaluations may compute values or produce side effects;
- control flow;
- rules for observable interactions such as input/output and access to
  volatile objects;
- areas where an implementation must choose and document behavior;
- areas where the program itself has no defined meaning.

A compiler's job is to preserve the behavior required by that model for the
target implementation. The physical route may be radically different:

```text
C source constraints
        ↓ front end checks syntax, types, and semantics
intermediate representation
        ↓ optimization preserves required observable behavior
target instructions, data, and metadata
        ↓ assembler + linker + loader
process executing on an ISA and operating system
```

The middle can delete variables, fold expressions at translation time,
replace loops with another computation, keep values only in registers, or
emit no instructions for a source statement whose result cannot matter.

This is the **as-if rule** in practical form: an implementation may transform
the program however it likes *as if* the required observable behavior had
occurred. It is not permission to change a defined result. It is freedom over
how that result is obtained.

### Three categories you must keep separate

**Defined behavior** has a meaning imposed by the language. Unsigned integer
addition wraps modulo one more than the maximum value.

**Implementation-defined behavior** is a choice the implementation must make
and document. Whether plain `char` behaves like `signed char` or `unsigned
char` is one example.

**Undefined behavior** imposes no requirements on the implementation. Signed
integer overflow is a classic example. It is not a portable wrapping request.

There is also **unspecified behavior**, where the implementation may choose
among allowed possibilities without documenting which choice it made—for
example, the order in which many function arguments are evaluated.

Those categories are language rules, not CPU exception classes. A processor
may naturally wrap a signed `add`, yet the optimizer may reason that a
well-defined C execution never overflows and transform surrounding code.

---

## 2. Translation environment and execution environment

The C model distinguishes two broad environments.

### Translation environment

This is where source becomes an executable form:

```text
source files
  → preprocessing directives and macro expansion
  → translation units
  → compiler analysis and code generation
  → assembler output / object files
  → linking
```

Some facts are resolved here:

- `_Static_assert` either succeeds or rejects the translation;
- `sizeof(int)` is known for the chosen target;
- constant expressions may be folded;
- diagnostics are issued for required constraint violations;
- implementation choices such as integer widths affect generated code.

This is not necessarily the same machine as the one that executes the
program. Cross-compilers make that obvious: an x86-64 host can translate C
for RISC-V.

### Execution environment

This is where the translated program runs. The C standard discusses two broad
forms:

- a **hosted** environment, normally with an operating system and a standard
  library, where program startup reaches `main`;
- a **freestanding** environment, common in kernels, boot code, and embedded
  systems, where startup and available library facilities are
  implementation-defined.

Linux userspace is hosted. The Linux kernel is not an ordinary hosted C
program: it supplies its own entry paths, linker script, runtime support, and
restricted dialect/build assumptions.

The translation/execution split prevents a common mistake: `sizeof(long)` is
a property of the **target C implementation**, not necessarily the computer
running the compiler process.

---

## 3. Object, value, type, identifier, and representation

These words are related but not interchangeable.

Consider:

```c
int count = 5;
```

- `count` is an **identifier**, a source-level name.
- The declaration creates an **object** during its applicable lifetime.
- The object's **type** is `int`.
- Its stored **value** is initially `5`.
- Its **object representation** is the sequence of `unsigned char`-sized
  units that stores that value.

An object is a region of data storage whose contents can represent values.
The compiler may optimize away any literal memory region when doing so
preserves required behavior. The abstract object still helps define the
program even if no stack slot survives into the assembly.

### Values are interpretations, not free-floating bit meanings

The same bit pattern can participate in different interpretations:

```text
0xffffffff
  as uint32_t on a common implementation → 4294967295
  as a 32-bit signed two's-complement value → -1
  as four character bytes                → encoding-dependent data
  as part of a float representation      → a floating encoding
```

Do not say “the bits are signed.” Bits are bits. A C type and an operation
determine how those bits are interpreted and which result is required.

Not every type must use every possible bit pattern as an ordinary value, and
padding bits or trap representations matter on some types and
implementations. Today's lab avoids manufacturing typed values from arbitrary
bytes. It observes existing objects through `unsigned char` and uses `memcpy`
to copy representations safely.

### Type affects more than storage size

A type helps determine:

- the set of values;
- representation and alignment requirements;
- which operations are permitted;
- how operands are converted;
- how arithmetic and comparisons are interpreted;
- pointer scaling and aliasing rules;
- often, but not uniquely, which machine instruction widths and families are
  useful.

This is why “`int` is four bytes” is not an adequate definition of `int`.

---

## 4. C bytes and the character types

In C, `sizeof(char)`, `sizeof(signed char)`, and `sizeof(unsigned char)` are
all exactly `1`. That does **not** say one C byte is always eight bits.

`CHAR_BIT`, from `<limits.h>`, tells you how many bits are in a byte for the
implementation. It is at least 8. On the x86-64 Linux system used for this
course it will almost certainly be 8, but the lesson asks you to measure
rather than universalize.

```c
printf("CHAR_BIT=%d\n", CHAR_BIT);
```

Consequences:

- `sizeof object` reports a count of C bytes;
- `sizeof(char)` cannot reveal the number of bits in a byte;
- an N-byte object contains `N * CHAR_BIT` bits in its object
  representation;
- `uint8_t` exists only when the implementation provides an unsigned integer
  type of exactly 8 bits with no padding.

There are three distinct character types:

- `char`;
- `signed char`;
- `unsigned char`.

Plain `char` has the same range/representation/behavior as one of the other
two on a given implementation, but it remains a distinct type. Use `char`
for character data, and use `unsigned char` when inspecting raw object
representations byte by byte.

That last permission is crucial. Converting an object's address to
`unsigned char *` lets code inspect each byte of its representation:

```c
const unsigned char *bytes = object;
for (size_t i = 0; i < sizeof value; ++i)
    printf("%02x ", bytes[i]);
```

The resulting byte order is evidence about this implementation. It is not a
portable serialization format.

---

## 5. Integer types: range first, encoding second

C's standard signed integer types, in increasing rank, are:

```text
signed char < short < int < long < long long
```

Each has a corresponding unsigned type. The exact width of most is chosen by
the implementation, subject to minimum ranges and ordering constraints.
Common x86-64 Linux data models use:

```text
char       8 bits
short     16 bits
int       32 bits
long      64 bits
long long 64 bits
pointer   64 bits
```

That is commonly called LP64. It is not universal: 64-bit Windows commonly
uses 32-bit `long` in an LLP64 model.

Ask the implementation:

```c
sizeof(short)
sizeof(int)
sizeof(long)
INT_MIN, INT_MAX, UINT_MAX
```

Use `<limits.h>` for fundamental integer limits and `<stdint.h>` when an exact
or minimum width is genuinely part of the requirement:

- `int32_t`: exactly 32 bits, if provided;
- `uint32_t`: exactly 32 bits, if provided;
- `int_least32_t`: smallest available type with at least 32 value bits;
- `int_fast32_t`: implementation's fast type with at least 32 value bits;
- `intptr_t`: integer capable of round-tripping `void *`, if provided.

Exact-width names are useful for file formats, registers, and protocols, but
do not automatically solve endianness, alignment, padding, or serialization.

### Unsigned arithmetic is modular

For an unsigned type with maximum value `U_MAX`, arithmetic is reduced modulo
`U_MAX + 1`:

```c
uint32_t x = UINT32_MAX;
x = x + 1;                 /* defined result: 0 */
```

That supports counters, masks, and low-level arithmetic. It does not mean
every accidental wrap is logically correct.

### Signed overflow is different

If a signed arithmetic result is not representable in its type, behavior is
undefined. Do not reason:

```text
the target add instruction wraps
therefore signed C overflow wraps
```

The compiler reasons from C semantics before choosing the target instruction.
If modular behavior is required, express the relevant operation in a suitable
unsigned type and convert deliberately.

---

## 6. Integer rank, promotions, and usual arithmetic conversions

Most surprising integer expressions become manageable if you ask two
questions in order:

1. What type does each operand have *after promotion*?
2. What common type do the binary operands use?

### Integer promotions

Types with rank below `int`—such as `unsigned char` and `short`—usually do not
remain narrow during arithmetic.

If `int` can represent all values of the original type, the operand promotes
to `int`; otherwise it promotes to `unsigned int`.

On the course machine:

```c
unsigned char a = 250;
unsigned char b = 10;
int sum = a + b;            /* 260, not an 8-bit wrap */
```

Both operands promote to `int` before addition. Narrowing occurs only if the
result is later converted back:

```c
unsigned char narrowed = a + b;  /* conversion yields implementation's
                                    unsigned-char modulo result */
```

Integer promotions also apply to unary `+`, unary `-`, `~`, shift operands,
and variadic arguments in relevant contexts. This is why generated code may
load a byte and then compute with a 32-bit register.

### Usual arithmetic conversions

For many binary arithmetic and comparison operators, C finds a common real
type. At an introductory level:

1. floating operands may cause integer operands to convert to a floating
   type;
2. otherwise integer promotions happen;
3. if promoted integer types match, use that type;
4. same signedness generally selects the higher-rank type;
5. mixed signed/unsigned cases depend on rank and representable ranges.

The famous example:

```c
int s = -1;
unsigned int u = 1;
printf("%d\n", s < u);
```

When `int` and `unsigned int` have the same rank, `s` converts to `unsigned
int`. On the common 32-bit `unsigned int` implementation, `-1` becomes
`UINT_MAX`, so the comparison is false.

Nothing changed the stored object `s`. The conversion applies to the value
used by this expression.

### A disciplined prediction method

For every nontrivial expression, write:

```text
declared operand types
  → promotions
  → common type
  → operation in that type
  → result type/value
  → assignment conversion, if any
```

Do this before running the Day 7 lab. It is slower than guessing once and
much faster than debugging a signed/unsigned bug later.

---

## 7. How integer type appears in assembly

C does not mandate instruction selection, but type gives the compiler facts
it must preserve.

### Width

An access through `unsigned char *` reads one C byte. A common 32-bit `int`
load reads four bytes. A 64-bit `long` operation on LP64 commonly uses a
64-bit instruction width.

### Extension

When a narrow value becomes wider:

- an unsigned value needs zero extension;
- a signed value needs sign extension.

On x86-64, likely instruction families include `movzx`/`movzbl` for zero
extension and `movsx`/`movsbl` for sign extension. Sometimes an ordinary
32-bit register write supplies the needed zeroing for free. The exact
sequence is a compiler choice.

### Comparison

The subtraction-like flag setting may be identical, but the branch condition
depends on interpretation:

```text
signed less-than     → condition based on sign/overflow relation
unsigned below       → condition based on carry/borrow
```

At higher optimization, the compiler may replace a branch with `setcc`, a
conditional move, vector instructions, or a compile-time constant.

### Division and shifts

Signed and unsigned division commonly use different instructions or setup.
Right shift of unsigned values is logical; right shift of a negative signed
value is implementation-defined in the C version used by this lab, even
though x86 offers both arithmetic and logical right shifts.

This is the useful claim:

> Type constrains the required meaning, and that meaning influences generated
> operations.

This is the misleading claim:

> Every C type maps permanently to one assembly instruction.

---

## 8. Floating point: enough orientation to read the boundary

C supplies `float`, `double`, and `long double`. Their formats, precision,
range, and evaluation details are properties of the implementation. Many
current systems use IEC 60559 / IEEE 754 binary formats for `float` and
`double`, but portable C code should query what it needs from `<float.h>`.

Useful macros include:

- `FLT_RADIX`: radix used for floating representation;
- `FLT_MANT_DIG`, `DBL_MANT_DIG`: precision in radix digits;
- `FLT_MIN`, `FLT_MAX`, `DBL_MIN`, `DBL_MAX`: range-related limits;
- `FLT_EVAL_METHOD`: possible wider evaluation behavior.

Binary floating point cannot represent most decimal fractions exactly.
`0.1` is therefore usually a nearby binary value, not mathematical one tenth.
That is a representation issue, not random compiler error.

Floating arithmetic also has rounding modes, infinities, NaNs, signed zero,
exceptions, contraction, and optimization controls. Those deserve their own
study. For today, retain four facts:

1. integer-to-floating conversion can lose precision;
2. floating-to-integer conversion requires careful range reasoning;
3. equality after arithmetic is often the wrong comparison model;
4. generated code commonly uses floating/vector registers and instruction
   families rather than integer arithmetic instructions.

The challenge prints precision characteristics and the bytes of `1.0f`. Treat
those bytes as an observation. The program copies them with `memcpy` into
`uint32_t`; it does not violate aliasing rules by pretending a `float *` is a
`uint32_t *`.

---

## 9. Expressions, operators, and evaluation caveats

An **operator** such as `+`, `*`, `&&`, or `=` participates in an
**expression**. Expressions may compute values and may also have side
effects.

Three different ideas are often confused:

- **precedence** controls grouping;
- **associativity** resolves grouping among operators at the same precedence;
- **evaluation order** controls when subexpressions are evaluated.

For example:

```c
a + b * c
```

groups as `a + (b * c)` because of precedence. That alone does not establish
an execution order between every subexpression.

Likewise:

```c
f() + g()
```

does not generally promise whether `f` or `g` is called first. If both depend
on shared side effects, the program may become unspecified or undefined
depending on what they do.

Avoid puzzle code such as:

```c
i = i++ + 1;       /* undefined behavior */
printf("%d %d\n", next(), next()); /* argument order is not specified */
```

Write separate full expressions when order matters:

```c
int first = next();
int second = next();
printf("%d %d\n", first, second);
```

Short-circuit operators do impose useful sequencing:

```c
p != NULL && p->ready
```

The right operand of `&&` is evaluated only if the left operand compares
nonzero. The conditional operator and comma operator also have sequencing
rules; the comma separating function arguments is not the comma operator.

Week 3 returns to qualifiers, sequencing, optimization, atomics, and
undefined behavior. Today's goal is simply to stop treating source order as
a universal execution schedule.

---

## 10. Scope, storage duration, and lifetime are three questions

These terms are frequently collapsed into “where the variable lives.”

### Scope: where can the name be used?

Scope is about identifiers in source:

- block scope for a local declaration;
- function scope for labels;
- function-prototype scope for parameter names in a prototype;
- file scope for declarations outside functions.

### Storage duration: how long is storage reserved?

At this level:

- **automatic:** normally associated with block entry/exit;
- **static:** storage lasts for the entire program execution;
- **thread:** one instance per thread for the thread's execution;
- **allocated:** storage is controlled through allocation/deallocation.

`static` is context-sensitive. At block scope it gives static storage
duration; at file scope it also affects linkage. Those are different
properties.

### Lifetime: when does an object exist?

Lifetime is the portion of execution during which an object exists and
retains a valid stored value under the language rules. Accessing an object
outside its lifetime is not repaired by the address bits still looking
plausible.

```c
int *bad(void)
{
    int local = 7;
    return &local;           /* pointer outlives the automatic object */
}
```

The compiler may place `local` on a stack, in a register, or nowhere
materialized. The bug is defined at the C lifetime level, not by whether the
old stack bytes happen to remain.

Pointers, allocation, ownership, and lifetime get full treatment next week.
For now, practice asking separately:

```text
Can this identifier be named here?          → scope
What rule provides its storage?             → storage duration
Does the referred-to object exist now?      → lifetime
Can another translation unit name it?       → linkage (a fourth property)
```

---

## 11. Lab — types, limits, conversions, bytes, and assembly

Use the existing challenge:

```text
weekly/week-002/challenges/day-007-c-machine-model-lab.c
```

### Phase A: predict

Before compiling, record:

1. `CHAR_BIT` and each printed `sizeof`;
2. whether `-1 < 1u` is true;
3. the result of `UINT32_MAX + 1`;
4. the result of adding `(unsigned char)250` and `(unsigned char)10`;
5. the byte order expected for `0x11223344`;
6. the expected bytes of `1.0f`, clearly labelled as an implementation
   prediction rather than a C guarantee.

For items 2–4, write the promotion/conversion chain, not only the answer.

### Phase B: compile and run

```bash
cd /home/jd/Desktop/masterclass

gcc -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion \
  -O0 -g weekly/week-002/challenges/day-007-c-machine-model-lab.c \
  -o /tmp/day7-c-model-O0

/tmp/day7-c-model-O0
```

Record compiler version and target:

```bash
gcc --version | sed -n '1p'
gcc -dumpmachine
```

Warnings are teaching evidence. Do not silence one with a cast until you can
state which conversion the cast requests and why it is valid.

### Phase C: compare source with generated assembly

Generate readable compiler output:

```bash
gcc -std=c17 -Wall -Wextra -O0 -fno-asynchronous-unwind-tables \
  -S -masm=intel \
  weekly/week-002/challenges/day-007-c-machine-model-lab.c \
  -o /tmp/day7-c-model-O0.s

gcc -std=c17 -Wall -Wextra -O2 -fno-asynchronous-unwind-tables \
  -S -masm=intel \
  weekly/week-002/challenges/day-007-c-machine-model-lab.c \
  -o /tmp/day7-c-model-O2.s

diff -u /tmp/day7-c-model-O0.s /tmp/day7-c-model-O2.s || true
```

Find `mixed_compare`, `wrap_add`, and `promoted_sum` at `-O0`. Then inspect
`main` in both files.

At `-O2`, static helpers may be inlined, constant-propagated, or removed. That
is not a failed lab. It is direct evidence that function boundaries and
source statements do not each require a surviving instruction sequence.

For each operation you do find, annotate:

- operand C types after promotions;
- operation width;
- whether extension is signed or zero-filling;
- signed or unsigned condition used by a comparison;
- where the result appears according to the ABI;
- whether the compiler computed the result during translation.

### Phase D: inspect instructions in the executable

```bash
gcc -std=c17 -Wall -Wextra -O2 -g \
  weekly/week-002/challenges/day-007-c-machine-model-lab.c \
  -o /tmp/day7-c-model-O2

objdump -d -Mintel --disassemble=main /tmp/day7-c-model-O2
```

Compare this with the `.s` file. The executable contains final addresses and
encodings; compiler-generated `.s` is still assembly source for the
assembler.

### Phase E: change one type at a time

Make a temporary copy outside the repository:

```bash
cp weekly/week-002/challenges/day-007-c-machine-model-lab.c \
  /tmp/day7-c-model-experiment.c
```

Try controlled changes in `/tmp`:

1. change `mixed_compare`'s second parameter from `unsigned int` to `int`;
2. change `promoted_sum` parameters from `unsigned char` to `signed char` and
   choose safely representable inputs;
3. add functions that widen `signed char` and `unsigned char` to `int`;
4. add `int32_t` and `int64_t` addition functions;
5. add one `float` addition and one `double` addition.

For every change:

```text
predict C result
  → predict assembly-level consequence
  → compile at -O0 and -O2
  → annotate what changed
  → explain which difference came from type and which from optimization
```

Do not add signed-overflow or out-of-range floating-to-integer experiments
and then infer portable behavior from one run. A single observed result does
not define undefined behavior.

---

## 12. Reading the results without overclaiming

Suppose the compiler emits no addition for `wrap_add(UINT32_MAX)` in optimized
`main`, only materializing zero for `printf`. The wrong conclusion is:

> Unsigned addition uses no CPU instruction.

The defensible conclusion is:

> For this whole program, compiler, target, flags, and visible call site, the
> required unsigned result was known during translation, so no runtime
> addition was needed.

Likewise, seeing four bytes for `int` does not prove C defines `int` as four
bytes. Seeing little-endian order does not prove every C machine is
little-endian. Seeing IEEE-like `float` bytes does not make that encoding the
definition of C `float`.

Attach scope to evidence:

```text
language guarantee
implementation documentation
compiler/target/flag observation
one runtime measurement
```

That habit is one of the dividing lines between systems reasoning and machine
folklore.

---

## 13. Connections across the week

```text
C type and expression
    ↓ language defines required values and side effects
compiler chooses widths, extensions, comparisons, and instruction families
    ↓ ABI places arguments/results in registers or memory
ISA instructions transform architectural state
    ↓ loads/stores use addresses and object representations
alignment and byte order shape layout/interpretation
    ↓ caches move lines rather than C objects
memory hierarchy affects cost without changing defined C results
```

The compiler is not replacing C types with hardware types one-for-one. It is
proving that some sequence—or no runtime sequence at all—implements the
required behavior.

This also explains why kernel code is careful about types:

- fixed-width integer types communicate hardware/protocol widths;
- unsigned masks express modular bit operations;
- signed/unsigned comparisons can silently change domains;
- object lifetime constrains valid pointer use even when addresses remain;
- compiler flags and kernel-specific rules form part of the implementation
  contract.

---

## 14. Mastery — Explain · Draw · Observe · Build

### Explain

Without notes:

- why C uses an abstract machine;
- translation environment versus execution environment;
- hosted versus freestanding execution;
- object versus identifier versus value versus type versus representation;
- why a C byte is not defined as eight bits;
- integer range and rank;
- integer promotions and the usual arithmetic conversions in `-1 < 1u`;
- unsigned wrap versus signed overflow;
- precedence versus evaluation order;
- scope versus storage duration versus lifetime;
- why one C line need not correspond to one instruction.

### Draw

Draw both:

```text
source → preprocessing → translation unit → compiler → assembly/object
       → linker → executable → loader → executing process
```

and:

```text
declared types → promotions → common type → required operation/result
              → optimizer → target operation width/condition/extension
```

Mark which arrows belong to the language model, implementation, ABI, ISA, OS,
and microarchitecture.

### Observe

Preserve:

- compiler version and target triple;
- type sizes and limits;
- predicted and observed conversion results;
- raw bytes with a note that they are implementation observations;
- annotated `-O0` and `-O2` snippets;
- one helper that disappears or changes shape under optimization.

### Build

Create a one-page conversion sheet based on rules, not memorized examples:

1. list integer ranks;
2. state the integer-promotion test;
3. sketch the signed/unsigned common-type decision;
4. include one safe example of each;
5. attach one observed assembly consequence: width, extension, or condition.

Then write two tiny functions that differ only in parameter signedness.
Predict the comparison condition, compile them, and explain the generated
operations.

### Why? notebook

1. Why can the compiler remove an object that C says exists?
2. Why is `sizeof(char)` always one while `CHAR_BIT` can vary?
3. Why do two `unsigned char` operands often produce an `int` result?
4. Why can `-1 < 1u` be false without changing the stored value of `-1`?
5. Why is unsigned overflow defined while signed overflow is not?
6. Why can signed and unsigned addition use the same machine instruction but
   comparisons use different conditions?
7. Why is examining a `float` through `unsigned char` or `memcpy` safer than
   pointer-casting it to `uint32_t *`?
8. Why can `-O2` erase the function you planned to inspect?
9. Why does source order not always specify evaluation order?
10. Why is an old stack address not evidence that an automatic object's
    lifetime continues?
11. Why does a cross-compiler's `sizeof(long)` describe the target rather
    than the host?
12. Why can two different assembly sequences both correctly implement the
    same C expression?

---

## End model

```text
C specifies a typed abstract execution
  → implementation choices fill in widths, ranges, and representations
  → promotions/conversions determine the domain of each operation
  → the optimizer preserves required observable behavior
  → the backend chooses target operations and the ABI carries values
  → the CPU executes instructions, not source lines
```

If you can take `-1 < 1u`, derive its C result, identify the unsigned
comparison in unoptimized assembly, and explain why optimized assembly may
contain no comparison at all, you are reasoning across the right boundaries.

## References used selectively

Primary language reference:

- ISO/IEC 9899:2018 (C17), especially clauses 5.1 (conceptual models),
  5.2.4.2 (numerical limits), 6.2.1–6.2.6 (scope, linkage, storage duration,
  types, representation), 6.3 (conversions), and 6.5 (expressions).

Local companions:

- Bryant and O'Hallaron, *Computer Systems: A Programmer's Perspective*, 3rd
  ed., §§2.1–2.4 and Chapter 3 for representations, arithmetic, and the
  source-to-machine connection. Local PDF:
  `source-materials/library/books/01-foundations-and-architecture/computer-systems-programmers-perspective-bryant-ohallaron-3e-global.pdf`
- Kerrisk, *The Linux Programming Interface*, Chapter 6 for a Linux process
  memory-layout perspective, kept distinct from C's language-level storage
  duration and lifetime rules.

Current implementation references:

- GCC manual, “C Implementation-Defined Behavior” and “Options Controlling C
  Dialect”: <https://gcc.gnu.org/onlinedocs/gcc/C-Implementation.html>
- cppreference C language pages on arithmetic types, conversions, object, and
  lifetime, used as a navigational companion rather than the normative
  standard: <https://en.cppreference.com/w/c/language>

**Next bridge:** types tell the compiler what values and operations mean.
Next week, pointers, arrays, strings, allocation, and ownership ask when an
address designates a live object—and what happens when it does not.
