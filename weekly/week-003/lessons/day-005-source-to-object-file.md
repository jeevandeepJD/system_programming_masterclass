# Day 5 — From Source to Object File

**Curriculum alignment:** Stage 6 — Compilation, Linking, ELF, and Program
Startup · Tracker Week 19

**Target time:** approximately 2½–3 hours

- Why translation is staged: 25 minutes
- Preprocessor, front end, optimizer, and back end: 55 minutes
- Assembly, ELF sections, symbols, and relocations: 45 minutes
- Pipeline lab and mastery evidence: 55 minutes

> You wrote `printf("%d\n", transform(5));`. Before a linker has produced a
> program, what concrete information can already exist—and what must still be
> unknown?

That question repairs a misleading picture: “the compiler turns C into an
executable.” In ordinary Linux work, several tools transform different
representations under different contracts. The compiler driver coordinates
them, which is convenient, but convenience should not erase the boundaries.

Today we stop after each boundary:

```text
source + headers
    ↓ preprocessing
translation unit
    ↓ parsing, semantic analysis, optimization, code generation
assembly text
    ↓ assembly
relocatable ELF object
```

The object already contains machine instructions and data. It is not yet a
complete process image: some addresses and external definitions remain for a
linker to resolve.

---

## 1. Why engineers separated the stages

Early programmers entered numeric instructions or assembly mnemonics. Higher
level languages made one source program portable across machines, but that
created two distinct problems:

1. understand the language and choose machine operations;
2. encode those operations using one target ISA and object format.

Assemblers already solved the second problem. Early compilers could therefore
emit assembly and reuse the assembler. Separate compilation solved another
practical constraint: a large program should not need complete recompilation
when one source file changes. Each source file becomes an object; a later tool
combines objects.

Unix reinforced this compositional design. Small tools and stable textual or
binary interfaces made each representation observable. Modern GCC and Clang
perform far more internally than the old pipeline, but `-E`, `-S`, and `-c`
still expose useful stopping points.

### The compiler driver is a coordinator

Running:

```bash
gcc hello.c -o hello
```

does not imply one indivisible operation. The driver chooses preprocessors,
compiler components, assembler, linker, startup objects, default libraries,
search directories, target options, and output names. Ask it to show the
commands without running them:

```bash
gcc -### hello.c -o hello
clang -### hello.c -o hello
```

Exact commands vary by distribution and compiler build. That variation is
evidence that the driver is policy around several contracts.

---

## 2. Translation units begin with preprocessing

A C source file is not, by itself, the complete input parsed as C. After
comments are replaced and preprocessing directives are handled, the compiler
sees a **preprocessing translation unit**.

```c
#include <stdio.h>
#define SCALE(v) ((v) * 3)
printf("%d\n", SCALE(input));
```

`#include` performs token-level inclusion of a header. `#define` establishes
a macro substitution rule. Conditional directives choose tokens:

```c
#if defined(DEBUG)
    debug_log(value);
#endif
```

Observe the result:

```bash
gcc -E -dD source.c > source.i
gcc -E -P source.c > source-without-line-markers.i
```

`-E` stops after preprocessing. `-P` makes an easier reading copy but removes
line markers useful to later diagnostics. A header normally supplies
declarations, types, macros, and inline definitions—not a magical reference
to another compiled file.

### Why macro bugs feel different

The compiler proper checks expanded tokens, not the author's intended macro
abstraction:

```c
#define SQUARE(x) x * x
SQUARE(a + b)             /* expands to a + b * a + b */
```

Parentheses repair this case, but evaluating an argument more than once
remains dangerous. Use the preprocessed output when a diagnostic appears to
refer to code you did not seem to write.

### Translation unit is not object file

A translation unit is the language-level result of preprocessing one primary
source file and its included content. An object file is a target-specific ELF
container produced later. One often leads to one, but they are not synonyms.

---

## 3. Inside compilation: tokens to machine choices

Compiler implementations differ, and many optimization passes blur a simple
linear story. This model is still useful:

```text
characters
  → lexer: tokens
  → parser: syntax tree / AST
  → semantic analysis: types, scopes, constraints
  → intermediate representation (IR)
  → optimization and lowering
  → target instruction selection and register allocation
  → assembly or machine-code emission
```

### Lexer and parser

The lexer recognizes tokens such as identifiers, keywords, literals, and
operators. The parser checks grammatical structure. In `a + b * c`, the tree
records multiplication beneath addition, preserving precedence.

An **abstract syntax tree** describes source-language structure without every
punctuation token. Semantic analysis then asks questions grammar cannot:

- was this identifier declared in this scope?
- may this operand be dereferenced?
- what conversions apply?
- is this call compatible with its declaration?

Syntax can be valid while constraints fail:

```c
int *pointer;
pointer = 42;             /* parseable, but diagnostically incompatible */
```

### Why an IR exists

An intermediate representation gives many source languages and many target
architectures a shared middle ground. It makes control flow, data flow, and
operations explicit enough for analysis and transformation.

Do not equate LLVM IR, GCC's GIMPLE, an AST, assembly, and machine code.
They answer different questions. IR is an implementation representation, not
the C abstract machine and not an ISA contract.

Clang can expose one form:

```bash
clang -S -emit-llvm -O0 source.c -o source.ll
clang -Xclang -ast-dump -fsyntax-only source.c
```

These commands are compiler-specific observation windows, not standardized C
stages.

### Optimization and code generation

The optimizer preserves the behavior the language requires, not the visual
shape of source. Under C's rules it may fold constants, remove unreachable or
dead work, inline calls, and rearrange operations. Undefined behavior can
remove constraints the programmer assumed existed.

Compare:

```bash
gcc -S -O0 source.c -o source-O0.s
gcc -S -O2 source.c -o source-O2.s
diff -u source-O0.s source-O2.s
```

The back end chooses target instructions, registers, addressing modes, and
instruction scheduling. ABI rules constrain calling convention and object
layout; the ISA constrains encodings; microarchitecture can influence cost
choices.

---

## 4. The assembler produces relocatable ELF

Assembly text names instructions, labels, directives, and sections. The
assembler turns it into a **relocatable object file**, commonly ELF type
`ET_REL` on Linux:

```bash
gcc -c source.s -o source.o
file source.o
readelf -h source.o
```

The `.o` file is binary, but it is not merely “machine code.” It is a
structured handoff package.

### Sections group material by purpose

Typical sections include:

- `.text`: executable instructions;
- `.rodata`: read-only constants;
- `.data`: initialized writable objects;
- `.bss`: zero-initialized storage described mostly by size, not stored bytes;
- `.symtab` and `.strtab`: full linking symbol metadata and names;
- `.rela.text` or similar: relocations applying to instructions;
- `.debug_*`: optional debugger information;
- `.note.GNU-stack`: toolchain metadata concerning stack policy.

Inspect them:

```bash
readelf -SW source.o
objdump -s -j .rodata source.o
size -A source.o
```

Section placement can vary with options. With `-ffunction-sections`, functions
may receive separate text sections. Names are conventions interpreted by the
toolchain, not physical memory compartments already mapped into a process.

### Symbols name definitions and unresolved references

A symbol-table entry can describe a function or object, its binding, section,
size, and value relative to that section. A defined local `static` function
and an externally visible function have different binding. An external call
may remain undefined:

```bash
nm -C source.o
readelf -sW source.o
```

Common `nm` letters include `T/t` for text, `D/d` for initialized data,
`B/b` for zero-initialized data, `R/r` for read-only data, and `U` for an
undefined reference. Case often distinguishes global from local binding.
Treat these as display conventions; confirm details with `readelf`.

### Relocations record unfinished address calculations

Suppose an instruction calls `external_adjust`, but that function will arrive
from another object or library. The assembler cannot know the final distance.
It emits:

1. an instruction encoding with a placeholder/addend;
2. a symbol reference;
3. a relocation record saying how a linker should compute and place the
   final field.

On x86-64, a direct relative call commonly uses a relocation such as
`R_X86_64_PLT32`; accesses may use PC-relative or GOT-related relocation
types. The exact type depends on code model, visibility, PIC options, and
toolchain. Observe rather than memorize one result:

```bash
readelf -Wr source.o
objdump -dr source.o
```

`objdump -dr` is powerful because it places relocation annotations beside
the instruction bytes they qualify.

---

## 5. Lab: stop at every stage

From the repository root:

```bash
python3 weekly/week-003/challenges/day-005-object-lab.py
```

The lab creates a private temporary directory, runs the available `gcc` or
`clang`, and removes generated artifacts unless `--keep` is requested. The
source intentionally refers to `printf`, `external_adjust`, and
`shared_counter` without defining all of them. Compilation must succeed;
linking that object alone must fail.

Before running, predict:

1. which macro text disappears into expanded C;
2. which strings and initialized objects occupy data sections;
3. which names will show as defined, local, global, or undefined;
4. which instructions need relocation records;
5. why `.bss` storage need not consume the same number of file bytes.

Then rerun manually:

```bash
gcc -E weekly/week-003/challenges/day-005-object-demo.c |
  less
gcc -S -O0 weekly/week-003/challenges/day-005-object-demo.c -o /tmp/demo.s
gcc -c -O0 weekly/week-003/challenges/day-005-object-demo.c -o /tmp/demo.o
readelf -SWsWr /tmp/demo.o
objdump -dr /tmp/demo.o
```

Delete the `/tmp` artifacts when finished.

### Build the missing provider

Write `/tmp/provider.c` defining:

```c
int shared_counter = 4;
int external_adjust(int value) { return value - 1; }
```

Compile it separately, link both objects, and predict the output before
running. This is a manual multi-file project: two translation units, two
objects, one final executable.

---

## 6. Linux and kernel connections

Linux user programs, shared libraries, the kernel, and loadable modules all
depend on object metadata, but they do not all use identical link/runtime
rules.

- Kernel compilation creates many `.o` files and archives before the final
  `vmlinux` link.
- A loadable module is an ELF relocatable object with module-specific
  metadata and relocations that the kernel module loader processes.
- BPF toolchains can emit ELF objects whose sections and relocation metadata
  are consumed by loaders with domain-specific meanings.
- Debuggers map addresses back to source using optional DWARF sections.

Do not infer that the kernel invokes the normal userspace dynamic linker for
modules. The shared object format enables reuse; the consumer and policy are
different.

---

## 7. Mastery: Explain · Draw · Observe · Build

The tracker checkpoint is exact:

> **Explain what information exists in a `.o` file before linking.**

A complete answer names machine instructions and data bytes, section
organization, symbol definitions and unresolved references, relocation
records and addends, attributes/notes, and optional debug metadata. It also
states what is missing: final virtual addresses, resolution of every external
symbol, startup objects/libraries, and the final loadable segment plan.

### Explain

Without notes:

- describe preprocessing, compilation, and assembly;
- distinguish source file, translation unit, assembly, and object file;
- explain lexer, parser/AST, semantic analysis, IR, optimization, and code
  generation without claiming every compiler uses one fixed sequence;
- explain why undefined symbols can be valid in `.o` and errors at final link.

### Draw

Draw the pipeline with the input, output, responsible tool, and one preserved
piece of information at every arrow. Beside it, draw an object container with
`.text`, `.rodata`, `.data`, `.bss`, symbols, and relocations. Connect one
call instruction to its symbol and relocation.

### Observe

Save:

- one preprocessed macro expansion;
- one source expression and its `-O0`/`-O2` assembly;
- one symbol-table entry;
- one relocation beside the instruction it repairs.

### Build

Complete the provider object, link the two-file program, then change
`external_adjust` to another translation unit. Rebuild only the changed
object and relink. Explain why the unchanged source did not need
recompilation.

### Why? notebook

1. Why does `#include` not mean “link this library”?
2. Why can syntactically valid C still fail semantic analysis?
3. Why is IR neither an ISA nor the C abstract machine?
4. Why does `.bss` occupy memory without storing equivalent zero bytes?
5. Why can an object contain real instructions but still not be runnable?
6. Why must a relocation identify both a location and a calculation rule?
7. Why are section names useful before any process mapping exists?

---

## References used selectively

Local:

- Bryant and O'Hallaron, *Computer Systems: A Programmer's Perspective*, 3rd
  ed., Chapter 7, §§7.1–7.7, PDF pages 705–725, for compiler-driver stages,
  relocatable objects, symbols, resolution, and relocation:
  `source-materials/library/books/01-foundations-and-architecture/computer-systems-programmers-perspective-bryant-ohallaron-3e-global.pdf`

Current primary references:

- GCC 16.2 manuals, “Options Controlling the Kind of Output” and
  “Overall Options” (`-E`, `-S`, `-c`, `-v`, `-###`):
  <https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html>
- Clang documentation, command-line reference and AST introduction:
  <https://clang.llvm.org/docs/ClangCommandLineReference.html> and
  <https://clang.llvm.org/docs/IntroductionToTheClangAST.html>
- GNU Binutils manuals for `as`, `readelf`, `nm`, and `objdump`:
  <https://sourceware.org/binutils/docs/>
- System V ABI generic ELF specification, object files, sections, symbols,
  and relocation:
  <https://gabi.xinuos.com/>

**Next bridge:** the object deliberately leaves questions open. Day 6 asks
how a linker resolves those names, performs those calculations, and chooses
between copying library code now and binding shared code later.
