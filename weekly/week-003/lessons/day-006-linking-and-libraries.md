# Day 6 — Linking and Libraries

**Curriculum alignment:** Stage 6 — Compilation, Linking, ELF, and Program
Startup · Tracker Week 20

**Target time:** approximately 2½–3 hours

- Why separate objects need a linker: 25 minutes
- Resolution, archives, and relocation: 50 minutes
- Shared objects, PIC, GOT, and PLT: 50 minutes
- Search/versioning risks, lab, and mastery: 50 minutes

> Yesterday, `main.o` contained a call instruction, an undefined symbol named
> `external_adjust`, and a relocation. Today, who decides which bytes that
> name denotes—and when?

Linking is the act of composing separately produced objects into a new object
or executable while preserving the meaning of names and addresses. Libraries
change where definitions come from and when binding happens, but they do not
abolish symbol resolution or relocation.

```text
objects + archives + shared-object requirements + linker policy
    ↓ resolve names
    ↓ choose and lay out input sections
    ↓ apply static relocations / preserve dynamic relocations
executable or shared object
```

---

## 1. The engineering need: compile separately, compose later

Separate compilation makes change affordable. A project with a thousand
source files can rebuild one changed translation unit and relink. Libraries
make reused implementations distributable without pasting source into every
program.

That design creates obligations:

- two objects may define the same external name;
- one object may require a name no input provides;
- machine instructions may contain address-dependent fields;
- an archive should contribute only members actually needed;
- a shared-library definition may not have a final runtime address yet.

The linker turns those obligations into either a coherent result or a useful
failure.

### The linker does not understand your whole C program

A traditional ELF linker primarily sees sections, symbols, relocations, and
options—not C scopes and expressions. Link-time optimization can preserve a
compiler IR across translation units, but that is an additional compiler-
linker protocol. Start with the ordinary object model.

---

## 2. Symbol resolution: make each reference choose a definition

At a high level, resolution associates symbol references with definitions.
ELF symbol binding includes local, global, and weak forms.

- **Local** symbols are confined to one object for linking purposes.
- A **global/strong** definition can satisfy external references.
- A **weak** definition can serve as a fallback and is generally overridden
  by a strong definition.

The practical rules are nuanced by symbol visibility, common symbols,
linker options, language conventions, and dynamic interposition. Avoid the
folk rule “global variables are always strong.” Modern GCC defaults to
`-fno-common`; multiple tentative definitions that old toolchains merged may
now produce a duplicate-definition error.

Diagnose the inputs:

```bash
nm -A *.o
readelf -sW one.o
```

Typical failures:

```text
undefined reference to `compute'
multiple definition of `configuration'
```

The first means no selected input supplied a usable definition. The second
means incompatible selected inputs supplied more than one.

### Name identity includes ABI assumptions

Matching a symbol name is not enough for correctness. The producer and
consumer must agree on calling convention, parameter/return representation,
object size/alignment, and data layout. C headers carry declarations so both
translation units compile against one contract. ELF resolution cannot detect
every mismatched prototype.

---

## 3. Archives are indexed bags of object files

A static archive is commonly created with:

```bash
ar rcs libmathops.a add.o multiply.o
```

It is not one monolithic compiled function. It is a container of members plus
an index. While scanning the link command, the linker extracts archive
members needed to satisfy unresolved symbols. This explains why order can
matter with traditional one-pass archive processing:

```bash
gcc main.o -L. -lmathops       # conventional useful order
gcc -lmathops main.o           # may leave main's needs unseen when scanned
```

Circular archive dependencies may require `--start-group/--end-group`, but
those rescans cost time and can hide poor dependency structure. Prefer clear
acyclic library interfaces where practical.

### What “static linking” means here

With an archive, selected object members are copied into the link result and
their relocations are processed. This does **not** imply that every byte of
every archive member or every system facility becomes self-contained.

`-static` requests a much broader fully static link using static forms of
default libraries. It can fail when static libraries are absent, enlarge the
binary, and interact poorly with facilities designed around NSS, plugins,
locale data, or dynamic loading. The lesson lab links a private archive but
does not require a fully static glibc executable.

---

## 4. Relocation: calculate addresses after layout

After selecting input sections, the linker assigns output locations. A
relocation tells it:

```text
place P: where to patch
symbol S: which definition is involved
addend A: constant adjustment
type: formula and encoded-field constraints
```

A PC-relative relocation may conceptually compute `S + A - P`; an absolute
one may compute `S + A`. The actual ABI defines each formula, field width,
overflow check, and semantics.

```bash
readelf -Wr main.o
ld --verbose                 # inspect default script; output is long
readelf -SW program
objdump -dr program
```

Relocation is not “moving the function.” It is repairing address-dependent
fields after selection and layout. Some relocations are completed by the
static linker. Others are emitted into the dynamic executable for the runtime
loader.

---

## 5. Shared objects postpone part of the binding

Build position-independent code and a shared object:

```bash
gcc -fPIC -c mathops.c -o mathops.pic.o
gcc -shared -Wl,-soname,libmathops.so.1 \
    -o libmathops.so.1.0 mathops.pic.o
```

An executable dynamically linked against it normally records a dependency
such as `DT_NEEDED: libmathops.so.1`, not a copy of all library code.

At execution time, the ELF interpreter/dynamic linker maps the required
objects, resolves dynamic symbols under its lookup rules, and applies
remaining relocations. Sharing read-only code pages across processes saves
memory and lets compatible library updates serve multiple programs.

The costs include startup work, a larger compatibility surface, runtime
search policy, and the possibility of loading an unintended library.

### PIC: code that tolerates different load addresses

Position-independent code avoids embedding unnecessary fixed absolute
addresses in executable instructions. On x86-64 it can use RIP-relative
addressing for nearby data. External/preemptible definitions need additional
indirection because another ELF object may supply the chosen definition.

PIC is not “code with no addresses.” It is code whose address calculations
remain valid when the object is mapped at different bases, with loader-fixed
tables where needed.

### GOT and PLT at a high level

The **Global Offset Table (GOT)** contains address slots used by
position-independent references. The dynamic linker writes resolved runtime
addresses into relevant entries.

The **Procedure Linkage Table (PLT)** provides call stubs for external
functions. Depending on toolchain options and binding policy, a call can pass
through a PLT entry whose GOT state directs it to the dynamic resolver on
first use and to the resolved function later.

```text
call site → PLT entry → GOT slot → function
                         │
                         └→ resolver before binding
```

Not every external call must use a classic PLT. `-fno-plt`, symbol visibility,
direct binding, architecture, and linker optimizations alter the shape.
Modern systems may use eager binding (`-z now` or `LD_BIND_NOW=1`). Observe
your binary:

```bash
objdump -d -j .plt program
readelf -Wr program
readelf -dW program
```

### Security connection: RELRO

GOT entries must be writable while relocations are applied. RELRO lets the
loader make relocation-related regions read-only afterward. Partial and full
RELRO differ; `-Wl,-z,relro,-z,now` enables eager binding so more GOT state can
be protected. This is hardening, not a substitute for memory safety.

---

## 6. Search paths, versions, and RPATH risks

Three names are easily collapsed:

- **link name:** `libmathops.so`, used by `-lmathops` at build time;
- **SONAME:** `libmathops.so.1`, the runtime compatibility identity;
- **real file:** `libmathops.so.1.0`, one installed implementation.

The SONAME major component should change when ABI compatibility is broken.
Package managers usually maintain the symlink arrangement. Versioned symbols
can support finer compatibility, but they do not repair arbitrary ABI changes.

Inspect requirements and resolution:

```bash
readelf -dW program | grep -E 'NEEDED|RPATH|RUNPATH'
ldd program
ld.so --list program        # where supported
```

Use `ldd` only on trusted binaries. Some historical/platform
implementations may execute the target or its interpreter; `readelf -d` is
the safer first inspection for unknown files.

### RUNPATH/RPATH are security policy

Embedding `$ORIGIN` can make a relocatable private application bundle:

```bash
-Wl,-rpath,'$ORIGIN/../lib'
```

But a writable library directory lets an attacker replace code that the
program will load. Empty path components and relative/current-directory
searches are especially risky. Set-user-ID/set-group-ID execution invokes
secure-execution restrictions, but do not treat those restrictions as a
reason to ship unsafe search paths.

Prefer system package paths, a tightly controlled application directory, or
explicit deployment policy. Never recommend global `LD_LIBRARY_PATH` as a
permanent fix. It changes resolution for every launched dynamic program in
that environment and is ignored/restricted in secure execution.

`DT_RPATH` and `DT_RUNPATH` also differ in precedence and transitivity. Modern
GNU linkers generally emit RUNPATH for new dtags. Check the loader manual for
the exact search order rather than memorizing a simplified list.

---

## 7. Lab: build static and shared forms

Run:

```bash
bash weekly/week-003/challenges/day-006-linking-lab.sh
```

The script creates a temporary multi-file project, then:

1. compiles client and provider separately;
2. displays the client's unresolved symbols and relocations;
3. creates `libmathops.a` and links an archive-backed executable;
4. creates PIC, `libmathops.so.1.0`, and SONAME symlinks;
5. links a shared-object executable with experiment-only `$ORIGIN` RUNPATH;
6. inspects dependencies, PLT, and dynamic relocations;
7. intentionally triggers an undefined-symbol diagnostic;
8. removes the temporary directory.

Before running, predict which executable records `DT_NEEDED` for the private
library and which contains copied archive member code.

### Trigger duplicate definitions

In a temporary directory, add two files:

```c
/* a.c */ int collision(void) { return 1; }
/* b.c */ int collision(void) { return 2; }
```

Compile both and link them with a caller. Record the diagnostic. Then repair
the design—not by hiding the error with a linker flag, but by making one
definition private with `static`, deleting the duplicate, or assigning
distinct responsibilities.

### Compare size honestly

Use `size`, `ls -l`, and `readelf -d`, but state what each measures. A small
dynamically linked executable depends on bytes stored elsewhere. File size is
not total runtime resident memory; shared pages may be counted in multiple
process mappings while occupying physical memory once.

---

## 8. Linux and kernel connections

- Linux distribution ABI stability depends on SONAME and symbol/version
  discipline.
- Kernel `EXPORT_SYMBOL*` creates a controlled namespace for modules, but
  module symbol resolution is performed by the kernel loader, not glibc's
  dynamic linker.
- Kernel builds use archives and linker scripts heavily to order sections and
  collect init tables.
- Static PIE, PIE, shared objects, and kernel modules are all ELF uses with
  different relocation and runtime policies.

The common mechanism is metadata-directed composition. The authority that
loads, resolves, and permits symbols differs.

---

## 9. Mastery: Explain · Draw · Observe · Build

The tracker checkpoint:

> **Trace how an external function call becomes bound to executable code.**

Give two traces.

**Archive trace:** source declaration → undefined object symbol + relocation →
archive index selects provider member → static linker resolves symbol, lays
out code, applies relocation → call reaches copied provider code.

**Shared trace:** source declaration → dynamic symbol/call mechanism → static
link records `DT_NEEDED` and dynamic relocation/PLT-GOT state → loader maps
the SONAME-selected object → dynamic lookup chooses a definition → relocation
or lazy-binding update installs the runtime address → call reaches mapped
shared code.

### Explain

- explain symbol resolution and relocation;
- distinguish archive-backed, fully static, and dynamic linking;
- distinguish link name, SONAME, and real filename;
- explain PIC, GOT, and PLT without declaring one observed encoding universal;
- diagnose one undefined and one duplicate-symbol error.

### Draw

Draw one executable assembled from `main.o` and one selected archive member.
Then draw one process with an executable, loader, libc, and private shared
object mapped at independent bases. Mark static versus runtime relocations.

### Observe

Capture:

- `nm` before and after archive resolution;
- `DT_NEEDED`, SONAME, and RUNPATH;
- one GOT/PLT-related relocation;
- `ldd` or `ld.so --list` output for the trusted lab binary.

### Build

Add `triple()` to the lab library. Rebuild both library forms. Change the
implementation without changing its ABI and show that the shared executable
uses the replacement without relinking. Then deliberately change the
prototype and explain why a matching symbol name does not guarantee safety.

### Why? notebook

1. Why can archive order affect resolution?
2. Why is relocation more precise than “fixing addresses”?
3. Why does PIC improve shareability?
4. Why can lazy binding need writable state?
5. Why is a SONAME an ABI promise rather than cosmetic filename decoration?
6. Why is `$ORIGIN` useful and dangerous?
7. Why is a dynamically linked binary's small file size an incomplete
   comparison?

---

## References used selectively

Local:

- Bryant and O'Hallaron, *Computer Systems: A Programmer's Perspective*, 3rd
  ed., Chapter 7, §§7.2–7.14, PDF pages 708–754, for static linking,
  resolution, relocation, archives, shared libraries, PIC, library
  interposition, and loading:
  `source-materials/library/books/01-foundations-and-architecture/computer-systems-programmers-perspective-bryant-ohallaron-3e-global.pdf`

Current primary references:

- GNU `ld` manual, linker scripts, archive search, shared options, rpath, and
  `-z` hardening controls: <https://sourceware.org/binutils/docs/ld/>
- System V ABI generic ELF specification, symbol tables, relocation, dynamic
  linking, GOT, and PLT: <https://gabi.xinuos.com/>
- Linux man-pages `ld.so(8)`, including dependency search order,
  `$ORIGIN`, secure-execution mode, and dynamic-linker diagnostics:
  <https://man7.org/linux/man-pages/man8/ld.so.8.html>
- GCC options for code generation (`-fpic`, `-fPIC`, `-fpie`, `-fPIE`):
  <https://gcc.gnu.org/onlinedocs/gcc/Code-Gen-Options.html>

**Next bridge:** the linker can produce an executable and name its required
interpreter. Day 7 follows `execve` from that file through kernel mappings,
the dynamic linker, libc startup, constructors, `main`, and process exit.
