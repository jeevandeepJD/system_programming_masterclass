# Week 3 — C Memory, Compilation, Linking, and Program Startup

**Format:** seven daily sections collected into one weekly module

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> A C expression begins with typed objects and lifetime rules. A running
> process begins with mapped machine code, startup state, and a kernel-to-user
> handoff. This week connects those views without skipping the compiler,
> object file, linker, ELF loader, or C runtime between them.

“Week” names a mastery module, not a calendar deadline. Spend longer when a
sanitizer report, relocation, or startup trace exposes a weak link. Familiar
material may take less time, but completion still requires evidence across
Explain, Draw, Observe, and Build.

## Why these seven days belong together

The first four days define what a C program is allowed to mean. The final
three show how that meaning becomes a Linux process:

```text
typed C objects, pointers, aggregates, and callbacks
        ↓ lifetime, ownership, qualifiers, atomics, and defined behavior
translation unit
        ↓ compiler and assembler
relocatable ELF: sections, symbols, machine code, relocations
        ↓ static linker
executable ELF: resolved layout plus dynamic-link requirements
        ↓ execve and kernel ELF loading
mapped executable, interpreter, initial stack, and auxiliary vector
        ↓ user-space dynamic linker and libc startup
relocations → constructors → main → exit/atexit → kernel termination
```

The connections matter more than the list. A pointer names an object whose
lifetime and type constrain valid accesses. The optimizer relies on those
constraints while producing instructions. Separate translation leaves
symbols and address calculations unfinished. The linker composes objects.
ELF program headers then describe a process image; Linux and the user-space
dynamic linker perform different parts of startup before libc calls `main`.

## Daily roadmap and exact files

### Day 1 — Pointers, Arrays, and Strings

Define an address precisely, derive typed pointer arithmetic, distinguish
arrays from pointers and array decay, reason about multidimensional layout,
and treat C strings as a length/lifetime safety problem rather than as a
built-in type.

Lesson:

```text
weekly/week-003/lessons/day-001-pointers-arrays-and-strings.md
```

Challenges:

```text
weekly/week-003/challenges/day-001-pointer-and-array-lab.c
weekly/week-003/challenges/day-001-string-functions-lab.c
```

**Evidence:** predicted and observed addresses/strides, array-versus-pointer
size results, ASLR comparison, completed string primitives, and annotated
AddressSanitizer reports.

### Day 2 — Structures, Unions, and Function Pointers

Predict structure padding and alignment, use tagged unions safely, understand
callbacks and context pointers, build operations tables and opaque handles,
and connect intrusive containers to kernel-style interfaces.

Lesson:

```text
weekly/week-003/lessons/day-002-structures-unions-and-function-pointers.md
```

Challenge:

```text
weekly/week-003/challenges/day-002-layout-callback-lab.c
```

**Evidence:** hand-predicted and observed member offsets, one disassembled
member access, one indirect call, a callback registry, and a miniature
operations table.

### Day 3 — Dynamic Memory and Lifetime

Separate scope, storage duration, object lifetime, reachability, and
ownership. Use the allocation family with overflow and failure handling,
design cleanup paths, inspect memory errors, and build an arena whose bulk
lifetime is explicit.

Lesson:

```text
weekly/week-003/lessons/day-003-dynamic-memory-and-lifetime.md
```

Challenges:

```text
weekly/week-003/challenges/day-003-arena-lab.c
weekly/week-003/challenges/day-003-memory-errors.c
```

**Evidence:** arena offsets and capacity behavior, one sanitizer report, one
leak report where available, and an ownership table naming every allocated
object's owner, borrowers, lifetime end, and cleanup path.

### Day 4 — Qualifiers, Atomics, and Undefined Behavior

Use `const`, `volatile`, and `restrict` under their actual contracts;
distinguish device observation from inter-thread synchronization; reason
about sequence and undefined behavior; and introduce C atomics as operations
with memory-order guarantees rather than “volatile but stronger.”

Lesson:

```text
weekly/week-003/lessons/day-004-qualifiers-atomics-and-undefined-behavior.md
```

Challenges:

```text
weekly/week-003/challenges/day-004-qualifiers-ub-lab.c
weekly/week-003/challenges/day-004-atomics-lab.c
```

**Evidence:** `-O0`/`-O2` assembly comparisons, sanitizer diagnostics for
selected invalid modes, and repeated volatile-versus-atomic observations with
the compiler, CPU, and run count recorded.

### Day 5 — From Source to Object File

Stop GCC or Clang after preprocessing, compilation, and assembly. Distinguish
translation units, AST/IR concepts, assembly, and ELF relocatable objects;
then inspect sections, symbols, machine instructions, and relocation records
before final linking.

Lesson:

```text
weekly/week-003/lessons/day-005-source-to-object-file.md
```

Challenges:

```text
weekly/week-003/challenges/day-005-object-demo.c
weekly/week-003/challenges/day-005-object-lab.py
```

**Evidence:** one macro expansion, one optimization-driven assembly change,
one defined and one undefined symbol, one instruction paired with its
relocation, and a separately compiled provider that completes the program.

### Day 6 — Linking and Libraries

Resolve strong, weak, local, and undefined symbols; apply relocations after
layout; compare archive selection with shared-object dependencies; and
inspect PIC, GOT/PLT behavior, SONAME, RUNPATH, and dynamic binding without
collapsing build-time and runtime linking.

Lesson:

```text
weekly/week-003/lessons/day-006-linking-and-libraries.md
```

Challenge:

```text
weekly/week-003/challenges/day-006-linking-lab.sh
```

**Evidence:** archive and shared builds, `DT_NEEDED`/SONAME/RUNPATH output,
one dynamic relocation, one PLT/GOT observation, and diagnosed undefined and
duplicate symbol failures.

### Day 7 — ELF Loading and `main`

Distinguish ELF sections from loadable segments; cross the successful
`execve` boundary; trace kernel mappings and the `PT_INTERP` handoff; then
follow the user-space dynamic linker, relocations, crt/libc startup,
`argc`/`argv`/`envp`/auxv, constructors, `main`, `exit`, and `atexit`.

Lesson:

```text
weekly/week-003/lessons/day-007-elf-loading-and-main.md
```

Challenges:

```text
weekly/week-003/challenges/day-007-startup-probe.c
weekly/week-003/challenges/day-007-elf-startup-lab.sh
```

**Evidence:** ELF entry and program headers, section-to-segment mapping,
`PT_INTERP`, `_start` disassembly, labelled `/proc/self/maps`, selected auxv
values, constructor/`main`/`atexit` order, and a bounded trusted `LD_DEBUG`
trace.

## Adjustable workload guide

The nominal budget is approximately 21 focused hours:

```text
Day 1   3 h   address model, arrays/strings, sanitizers
Day 2   3 h   layout, unions, callbacks, operations tables
Day 3   3 h   ownership, allocation, arena, diagnostics
Day 4   3 h   qualifiers, optimizer freedom, atomics, UB
Day 5   3 h   translation stages and relocatable ELF
Day 6   3 h   symbol resolution, relocation, libraries, GOT/PLT
Day 7   3 h   execve, mappings, dynamic loader, libc startup and exit
```

This is a planning estimate, not a completion rule:

- split sanitizer, GDB, or dynamic-loader work across sessions if attention
  drops;
- repeat an experiment when observation contradicts prediction;
- shorten exposition already understood, but still preserve evidence;
- record unavailable static libraries, debugger behavior, or sanitizer
  runtimes as environment facts rather than silently skipping them;
- never mark a day mastered only because its Markdown was read.

## Integrated weekly evidence

### Explain

Without notes, tell one connected story:

1. a pointer is a typed way to name storage, and arrays/strings add layout and
   termination conventions;
2. structures, unions, callbacks, and operations tables organize state and
   runtime-selected behavior;
3. ownership and lifetime decide when an address may validly be used;
4. qualifiers, atomics, and the C abstract machine decide what observations
   and transformations are permitted;
5. preprocessing, compilation, and assembly create an ELF object containing
   code/data, symbols, and unfinished relocations;
6. the linker resolves names, lays out output, applies static relocations, and
   records remaining dynamic requirements;
7. Linux uses program headers to replace a process image and enter user code;
8. the user-space dynamic linker and libc complete startup before `main`;
9. normal return from `main` passes through libc cleanup and a kernel
   termination boundary.

At each step name whether the governing rule comes from ISO C, the compiler,
the ABI, ELF, the static linker, the Linux kernel, the dynamic linker, libc,
or application policy.

### Draw

Produce one source-to-execution diagram containing:

- C objects, pointers, owners, and lifetime boundaries;
- one structure layout and one callback through an operations table;
- source → translation unit → assembly → relocatable object;
- object sections, symbol table, and a relocation attached to an instruction;
- linker selection/layout and archive-versus-shared paths;
- executable sections grouped into `PT_LOAD` segments;
- process mappings for executable, interpreter, libc, heap, and stack;
- the initial `argc`/`argv`/`envp`/auxv stack;
- kernel and user-space responsibility lanes through `main` and exit.

Use arrows labelled with the responsible component. “System” or “loader”
without a kernel/user-space distinction is not enough.

### Observe

Keep a compact, reproducible evidence bundle:

- pointer/array outputs and sanitizer reports;
- structure offsets and one indirect callback instruction;
- arena behavior and one lifetime-error diagnostic;
- qualifier/atomic assembly or repeated-run results;
- preprocessed source, object symbols, sections, and relocations;
- archive/shared dependency and binding evidence;
- ELF program headers and section-to-segment mapping;
- `/proc/self/maps`, auxv, constructor/`atexit` output, and `LD_DEBUG`;
- compiler, linker/binutils, libc, kernel, and architecture versions.

Sanitizer reports and loader traces can contain addresses that vary under
ASLR. Preserve enough surrounding information to explain the relationship,
not to memorize one address.

### Build

Complete or extend the challenge programs so the final code demonstrates:

- safe pointer-plus-length handling and string bounds;
- a tagged union and callback/operations-table interface;
- explicit ownership and an arena lifetime;
- an atomic communication path with stated memory ordering;
- a multi-translation-unit program stopped and inspected at object stage;
- static archive and shared-object forms of one interface;
- a startup probe showing constructor, `main`, auxv/maps, `atexit`, and the
  expected exit status.

Every extension should retain the loop:

```text
predict → build/run → preserve evidence → explain the responsible contract
```

## Integrated source-to-execution mastery checkpoint

Start with a two-file C program in which `main`:

1. allocates an array of structures;
2. fills it through a callback selected from an operations table;
3. calls a function provided by a shared library;
4. registers an `atexit` handler;
5. returns a nonzero status.

Then explain the complete route:

1. What are the array object's size, alignment, owner, aliases, and lifetime?
2. Which pointer operations are defined, and where must a count accompany a
   pointer?
3. How is the callback type checked, and what machine-level event performs
   the indirect call?
4. Which accesses require atomics if another thread participates, and why is
   `volatile` insufficient?
5. What does each source file become after preprocessing, compilation, and
   assembly?
6. Which symbols are defined or undefined in each `.o`, and which
   relocations remain?
7. How does the static linker resolve local objects while preserving the
   shared-library requirement?
8. Which ELF sections contribute to which loadable segments?
9. After successful `execve`, what does Linux map and place on the initial
   stack?
10. Why does execution commonly enter the interpreter before executable
    `_start`?
11. Who maps the shared library and applies its runtime relocations?
12. How do `_start`, libc startup, constructors, and the ABI lead to `main`?
13. What happens to the allocation and registered handler when `main`
    returns, and where does kernel responsibility resume?

Mastery is demonstrated when every arrow is explicit:

```text
C meaning and lifetime
  → compiler representation and optimization
  → object code, symbols, and relocations
  → linker composition
  → ELF load plan
  → kernel exec setup
  → user-space dynamic linking and libc startup
  → main
  → orderly exit and kernel termination
```

If an answer says “the compiler,” “the linker,” or “the loader” without
identifying which artifact changed and which component had authority, return
to that boundary and inspect the evidence again.

## End-of-module reflection

Record in your own words:

- strongest understanding;
- remaining confusion;
- one prediction contradicted by actual output;
- connection to Linux, kernel, driver, or virtualization work;
- one boundary you can now explain more precisely;
- next revision action;
- whether to advance, repeat one lab, or extend this module.

Do not auto-fill confidence, completion, hours, or mastery. Those remain the
learner's evidence-based assessment.
