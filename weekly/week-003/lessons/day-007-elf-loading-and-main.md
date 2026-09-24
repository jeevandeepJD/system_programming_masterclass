# Day 7 — ELF Loading and `main`

**Curriculum alignment:** Stage 6 — Compilation, Linking, ELF, and Program
Startup · Tracker Week 21

**Target time:** approximately 2½–3 hours

- ELF sections, segments, and the `execve` boundary: 45 minutes
- Kernel mappings, interpreter handoff, and relocations: 50 minutes
- Initial stack, libc startup, constructors, `main`, and exit: 45 minutes
- Read-only inspection lab and mastery trace: 40 minutes

> The shell has called `execve("./program", argv, envp)`, and the call
> succeeds. Which instruction runs next—and who arranged for it to run?

The answer is usually not `main`, and for a dynamically linked program it is
not initially the executable's `_start` either. Linux replaces the calling
process image, maps what the ELF program headers request, constructs an
initial user stack, and enters the ELF interpreter named by `PT_INTERP`.
That interpreter is the user-space dynamic linker. It maps dependencies,
applies dynamic relocations, runs initialization machinery, and eventually
transfers control through the executable's entry path toward `main`.

This lesson keeps those responsibilities separate:

```text
kernel                         user space
───────────────────────────    ───────────────────────────────────────
accept execve
read ELF/program headers
replace address space
map loadable image + loader
build initial stack
set registers and enter  ───→  dynamic linker entry
                                map dependencies
                                resolve/apply relocations
                                executable _start
                                libc startup
                                constructors
                                main
                                exit handlers and termination syscall
```

That line is the central discipline for today. “The loader does it” is too
vague until we say whether we mean kernel ELF loading, the user-space dynamic
linker, or libc startup.

---

## 1. Sections explain the file; segments explain the process image

Day 5 inspected sections in a relocatable object. They remain useful in an
executable:

- `.text` groups instructions;
- `.rodata` groups read-only constants;
- `.data` groups initialized writable objects;
- `.bss` describes zero-initialized storage;
- `.symtab`, `.strtab`, and `.debug_*` support linking and debugging.

But the kernel does not create one mapping per section. It follows the
executable's **program header table**. A program header describes a segment:
a range of the file and the memory image the runtime needs.

Compare the two views:

```text
section header table                 program header table
────────────────────                 ────────────────────
linker/debugger organization         loading/runtime organization
many named sections                  fewer typed segments
.text, .rodata, .data, ...           PT_LOAD, PT_INTERP, PT_DYNAMIC, ...
may be absent in a stripped file     required information for loading
not the kernel's mapping plan        directly drives the image plan
```

Run:

```bash
readelf -SW /tmp/startup-probe
readelf -lW /tmp/startup-probe
```

The second command ends with a section-to-segment mapping. Several sections
can share one segment because they need compatible file placement and memory
permissions.

### `PT_LOAD`: the mapping recipe

A loadable segment gives the kernel values such as:

```text
p_offset   first byte in the file
p_vaddr    virtual address relative to the image's load bias
p_filesz   bytes supplied by the file
p_memsz    bytes required in memory
p_flags    readable, writable, executable
p_align    file/virtual alignment constraint
```

When `p_memsz` exceeds `p_filesz`, the extra memory is zero-filled. This is
how `.bss` can occupy memory without equal-sized zero bytes in the file.

Do not translate `PF_R`, `PF_W`, and `PF_X` into an oversimplified promise
that every line in `/proc/<pid>/maps` will exactly match one program header.
Mappings are page-granular. Segment boundaries can share pages; the dynamic
linker can later tighten permissions, notably for GNU RELRO; and kernel,
toolchain, and architecture details affect the visible split. The program
headers are the requested image, while `/proc/.../maps` is observed runtime
state.

### Other program headers that matter today

- `PT_INTERP` stores the pathname of the requested ELF interpreter, commonly
  `/lib64/ld-linux-x86-64.so.2` on x86-64 Fedora.
- `PT_DYNAMIC` identifies dynamic-linking metadata such as `DT_NEEDED`.
- `PT_PHDR` describes the program header table in the memory image.
- `PT_GNU_STACK` communicates requested stack execute permission.
- `PT_GNU_RELRO` marks a range intended to become read-only after relocation.

These are ABI/toolchain contracts layered on ELF. Not every ELF file has each
one, and an ELF object need not be a Linux userspace executable.

---

## 2. `execve` is replacement, not creation

The system call is conceptually:

```c
execve(path, argv, envp);
```

It does not create a second process. A shell commonly creates a child first
with `fork`/`clone`, and that child calls `execve`, but the exec operation
itself replaces the caller's program image.

If `execve` fails, it returns `-1` to the old program with `errno` set. If it
succeeds, it does not return to the old instruction stream. The process keeps
an identity such as its PID, while large portions of its userspace state are
replaced according to exec semantics: mappings, code, stack, signal
dispositions, credentials under applicable rules, close-on-exec file
descriptors, and other process attributes.

### What Linux does

At a deliberately architecture-neutral level, the kernel:

1. validates the executable and execution permissions;
2. selects a binary-format handler, for ELF normally the kernel ELF loader;
3. reads ELF headers and program headers;
4. prepares a new memory-management image;
5. establishes mappings for the executable's `PT_LOAD` ranges;
6. if `PT_INTERP` exists, opens and maps that interpreter too;
7. copies argument and environment strings and constructs the initial stack;
8. adds an **auxiliary vector** with facts needed by libc and the loader;
9. commits the new image and initializes user-mode register state;
10. returns to user mode at the selected entry address.

There is extensive error handling, credential policy, architecture setup,
randomization, and security bookkeeping behind that list. The important
boundary is simpler:

> The kernel creates the initial executable environment. It does not call the
> C function `main`, search shared libraries using libc code, or execute C
> constructors.

### Where execution begins

For an ELF file without `PT_INTERP`, the selected entry is the executable's
ELF `e_entry`, subject to the image's load bias.

For a normal dynamically linked ELF file, Linux also records the
executable's entry for the interpreter and begins user execution at the
interpreter's entry. The dynamic linker later transfers control toward the
program entry. `readelf -h` shows the executable's `e_entry`; it does not by
itself prove that this is the first userspace instruction after a dynamic
exec.

This distinction answers a common but imprecise statement:

```text
execve succeeds → kernel jumps to main          wrong
execve succeeds → kernel runs the dynamic linker in kernel mode  wrong
execve succeeds → kernel enters prepared user code, often ld.so  precise
```

---

## 3. The initial stack is a kernel-to-userspace data structure

At process entry, the ordinary C calling convention has not called anything.
The ABI defines an initial stack layout from which startup code can recover
the process inputs. Conceptually:

```text
high addresses
┌─────────────────────────────────────┐
│ argument and environment strings    │
│ platform / random bytes / names     │
├─────────────────────────────────────┤
│ auxiliary vector: type,value pairs  │
│ ...                                 │
│ AT_NULL, 0                          │
├─────────────────────────────────────┤
│ envp[0], envp[1], ... , NULL        │
├─────────────────────────────────────┤
│ argv[0], argv[1], ... , NULL        │
├─────────────────────────────────────┤
│ argc                                │ ← initial stack pointer region
└─────────────────────────────────────┘
low addresses
```

Exact padding, alignment, register state, and pointer size are ABI-specific.
Treat the diagram as relationships, not a byte-perfect universal layout.

### `argc`, `argv`, and `envp`

- `argc` counts argument strings.
- `argv` points to an array of pointers terminated by a null pointer.
- `envp` similarly names environment strings, conventionally `NAME=value`.

ISO C standardizes `main` forms with zero or two parameters. A third
`char **envp` parameter is a widely supported Unix extension; portable code
can use `getenv` or the external environment interfaces instead.

`argv[0]` is supplied by the caller and need not be a trustworthy canonical
path. `execve` receives separate `path` and `argv` arguments, so software can
choose a surprising `argv[0]`.

### The auxiliary vector

The auxv is a sequence of key/value entries provided by the kernel. Useful
examples include:

- `AT_PHDR`, `AT_PHENT`, `AT_PHNUM`: the main program's in-memory program
  header table;
- `AT_ENTRY`: the main executable's entry address;
- `AT_BASE`: the interpreter's load base when applicable;
- `AT_PAGESZ`: system page size;
- `AT_RANDOM`: address of random bytes used by runtime hardening;
- `AT_SECURE`: whether secure-execution rules apply;
- `AT_EXECFN`: the executed pathname string.

The dynamic linker and libc consume these facts without making a syscall for
each one. Use `getauxval` in normal C code:

```c
unsigned long page_size = getauxval(AT_PAGESZ);
```

`/proc/self/auxv` is binary, not text. `LD_SHOW_AUXV=1` or an appropriate
inspection tool can display it for a trusted test program, but `getauxval`
is clearer in the lab.

---

## 4. `PT_INTERP` hands control to the user-space dynamic linker

Day 6 stopped with an executable containing `DT_NEEDED` entries and dynamic
relocations. `PT_INTERP` tells Linux which program will finish that work.

Inspect it:

```bash
readelf -lW /tmp/startup-probe | grep -A1 INTERP
readelf -dW /tmp/startup-probe
```

The interpreter—often called `ld.so`, the runtime loader, or the dynamic
linker—runs in user mode inside the new process. Broadly, it:

1. discovers the main program and its dynamic metadata;
2. finds required shared objects under loader search and security rules;
3. maps those objects;
4. builds a symbol lookup scope;
5. applies required dynamic relocations;
6. performs TLS and runtime setup;
7. applies protection transitions such as RELRO;
8. arranges initialization and transfers control to the executable entry.

Exact sequencing is implementation- and architecture-dependent. glibc's
loader is not the only possible interpreter, and musl or another libc can
organize startup differently.

### Relocation now has real runtime addresses

ASLR and position-independent objects mean link time cannot know every load
base. The dynamic linker computes values using the actual mappings and writes
relocation results into designated slots.

Common categories include:

- **relative relocations**, based on an object's load base;
- **symbol-based relocations**, requiring dynamic symbol lookup;
- **PLT/GOT function binding**, either eager or lazy;
- **TLS relocations**, tied to thread-local storage layout.

Relocations do not mean the loader rewrites every instruction. Well-formed
PIC uses relative addressing and tables so that most executable code pages
remain unchanged and shareable.

### Lazy versus eager binding

Some function addresses may be resolved on first call through PLT/GOT state.
With eager binding—requested by `LD_BIND_NOW=1` or link options such as
`-z now`—they are resolved during startup. Full RELRO commonly combines eager
binding with making relevant relocation state read-only.

Do not set loader environment variables globally. Apply them to one trusted
command:

```bash
LD_BIND_NOW=1 /tmp/startup-probe
LD_DEBUG=libs,reloc /tmp/startup-probe 2>/tmp/startup-loader.log
```

Never use `LD_PRELOAD`, `LD_LIBRARY_PATH`, or `LD_DEBUG` casually with
untrusted executables or privileged execution. Loader variables alter code
selection or reveal detailed paths; secure-execution mode restricts many of
them.

---

## 5. `_start` is an entry point, not a C function

The executable's ELF header names an address. The linker normally obtains
the code there from C runtime startup objects—often called **crt objects**—
supplied by the toolchain/libc.

```bash
readelf -h /tmp/startup-probe | grep 'Entry point'
readelf -sW /tmp/startup-probe | grep -E '(_start| main$)'
objdump -d --disassemble=_start /tmp/startup-probe
```

On glibc x86-64, the observed `_start` commonly:

- clears or establishes ABI-required frame state;
- extracts `argc` and the `argv` base from the initial stack;
- aligns the stack;
- passes `main` and startup information to `__libc_start_main`;
- does not expect to return normally.

Do not call `_start` “the first line of every C program.” It is toolchain
startup code at an ELF entry, its details differ across libc, architecture,
static/dynamic mode, PIE, and toolchain release, and hand-written assembly
programs can provide their own entry.

### Why `main` needs a runtime before it

Before calling `main`, libc startup may need to:

- initialize libc internal state and threading/TLS support;
- establish stack-protector and other runtime state;
- register finalization machinery;
- coordinate executable and dependency initialization;
- call preinitialization and initialization arrays;
- establish a path from `main`'s return value to process termination.

Some of this overlaps or coordinates with work done by the dynamic linker.
The precise internal division is libc-specific. The stable public story is
that `main` is reached only after ELF entry code and runtime initialization.

---

## 6. Constructors run before `main`, but ordering is a contract problem

The lab probe defines:

```c
__attribute__((constructor))
static void before_main(void) { /* ... */ }
```

The compiler/linker place an address into an initialization array, normally
`.init_array`. Startup machinery walks applicable arrays before `main`.

Inspect:

```bash
readelf -x .init_array /tmp/startup-probe
readelf -sW /tmp/startup-probe | grep before_main
```

Constructor attributes are compiler extensions. Initialization order among
dependencies and the main executable follows ELF/libc rules, but relying on
fine-grained order among unrelated constructors is fragile. A constructor:

- runs before the program's ordinary `main` body;
- executes in user mode;
- can call functions only to the extent the runtime is ready;
- can fail, block, allocate, or take locks before application control begins.

Keep constructors small and avoid hidden global-order dependencies.

---

## 7. `main` returns; libc turns that into orderly process exit

For a hosted C program, returning `status` from the initial invocation of
`main` is equivalent in effect to calling `exit(status)`. `exit` performs
user-space termination work, including:

- invoking functions registered with `atexit` in reverse registration order;
- flushing and closing C standard I/O streams as specified;
- running applicable finalization machinery;
- finally asking the kernel to terminate the process.

`_exit`/`_Exit` bypass the normal `atexit` and stdio-flush path and proceed
toward immediate termination. This matters after `fork`: a child that cannot
`exec` often uses `_exit` so it does not flush a copied stdio buffer or run
parent-owned cleanup twice.

Keep the layers separate:

```text
return from main
  → libc exit path in user space
  → atexit/finalization/stdio cleanup
  → exit_group or equivalent termination system call
  → kernel records status, releases process resources, notifies parent
```

The shell generally displays only the low eight bits of a normal exit status.
Signals follow a different wait-status encoding; “exit code 139” is a shell
convention commonly representing termination by signal 11, not a program
calling `return 139`.

---

## 8. The lab: inspect without weakening the machine

Run from the repository root:

```bash
bash weekly/week-003/challenges/day-007-elf-startup-lab.sh
```

The script builds in a private temporary directory, inspects only its own
trusted executable, runs it, captures loader diagnostics to a temporary log,
checks the expected status, and removes the artifacts.

Before running, predict:

1. Is the binary `ET_EXEC` or `ET_DYN` on this compiler configuration?
2. How many `PT_LOAD` segments will appear, and which are writable or
   executable?
3. Which sections share each loadable segment?
4. What pathname will `PT_INTERP` request?
5. Will the constructor message appear before the first `main` message?
6. What values will `argc`, `AT_ENTRY`, `AT_PHDR`, and `AT_PAGESZ` describe?
7. Which mappings belong to the executable, interpreter, libc, heap, stack,
   vDSO, and vvar?
8. Will the `atexit` handler run before the shell observes status 23?

### Read the runtime map as evidence

The probe prints the first entries of `/proc/self/maps`. For a fuller view,
temporarily increase its display count or pause it under GDB and inspect:

```bash
gdb --args /tmp/startup-probe alpha
(gdb) starti
(gdb) info files
(gdb) info proc mappings
```

With a dynamically linked program, `starti` commonly stops in the
interpreter, not at the executable's `_start`. Then:

```text
(gdb) break _start
(gdb) continue
(gdb) break main
(gdb) continue
```

Record the three instruction addresses and identify their mappings. Debugger
behavior can vary depending on GDB and loader integration, so preserve actual
output rather than forcing it to match the diagram.

### Observe loader decisions safely

The script uses:

```bash
LD_DEBUG=libs,reloc trusted-program
```

Only stderr receives the loader trace. The lab captures it and prints a
bounded excerpt. Read the order:

```text
find dependency
  → relocation processing
  → dependency initialization
  → program initialization
  → transfer control
  → finalization
```

`LD_DEBUG` is glibc-loader behavior, not an ELF or Linux-kernel guarantee.
If another libc ignores it or formats it differently, that is an environment
result to record.

### Optional static comparison

If static libc development files are installed:

```bash
gcc -static -O0 -g \
  weekly/week-003/challenges/day-007-startup-probe.c \
  -o /tmp/startup-probe-static
readelf -lW /tmp/startup-probe-static
```

Predict that `PT_INTERP` and shared-library `DT_NEEDED` entries are absent.
Do not install packages merely to force this comparison. A static glibc
binary still contains substantial libc startup machinery, relocations of its
own kinds may remain, and “static” does not mean “the kernel calls `main`.”

---

## 9. Put the whole route on one page

Trace one successful command:

```text
shell
  │ prepares argv and envp
  │ calls execve
  ▼
Linux kernel
  │ validates ELF and reads program headers
  │ replaces mappings
  │ maps executable PT_LOAD ranges
  │ maps PT_INTERP
  │ creates initial stack: argc/argv/envp/auxv
  │ selects interpreter entry and returns to user mode
  ▼
dynamic linker (user mode)
  │ maps DT_NEEDED objects
  │ resolves symbols and applies dynamic relocations
  │ establishes protections and initialization state
  │ transfers toward executable e_entry
  ▼
_start / crt code (user mode)
  │ decodes startup state and invokes libc startup
  ▼
libc startup (user mode)
  │ runtime initialization and constructor coordination
  ▼
constructors (user mode)
  ▼
main(argc, argv, envp) (user mode)
  │ returns 23
  ▼
exit (user mode)
  │ atexit handlers, stdio/finalization
  │ termination syscall
  ▼
Linux kernel
  │ records status and makes it waitable
  ▼
shell observes 23
```

There may be no scheduler switch to a different process at every arrow.
“Kernel” and “user” here describe privilege and responsibility, not
necessarily different PIDs. After successful exec, this is one process with
a replaced userspace image.

---

## 10. Mastery: Explain · Draw · Observe · Build

The tracker checkpoint is exact:

> **Explain everything that happens after `execve` succeeds and before
> `main` begins.**

A complete answer must distinguish:

1. kernel validation, image replacement, `PT_LOAD` mappings, interpreter
   mapping, stack/auxv construction, and user-mode entry;
2. user-space dependency mapping, dynamic symbol lookup, relocations, and
   protection changes;
3. executable `_start`, crt/libc startup, and constructors;
4. the eventual ABI call to `main`.

### Explain

Without notes:

- distinguish sections from segments;
- explain why `PT_LOAD` rather than `.text` directly drives loading;
- explain why successful `execve` does not return;
- identify the first user code for a dynamic executable;
- state what the kernel does **not** do;
- explain why `AT_ENTRY` can name the executable entry even when the
  interpreter executes first;
- trace `main` returning through `exit`, `atexit`, and the kernel termination
  boundary.

### Draw

Draw:

1. file offsets and sections grouped into loadable segments;
2. the process mappings for executable, interpreter, libc, stack, and vDSO;
3. the initial stack with `argc`, `argv`, `envp`, auxv, and strings;
4. responsibility lanes for kernel, dynamic linker, crt/libc, and application.

### Observe

Preserve:

- ELF type, entry, interpreter, and all program headers;
- the section-to-segment mapping;
- `_start` disassembly and `main` symbol;
- constructor and `atexit` ordering;
- selected auxv values;
- `/proc/self/maps` lines labelled by owner and permissions;
- a bounded `LD_DEBUG=libs,reloc` trace;
- optionally, GDB stops in interpreter entry, `_start`, and `main`.

### Build

Extend the probe in one of these ways:

1. walk `envp` to the word after its terminating null and decode raw auxv
   entries, comparing them with `getauxval`; or
2. add two constructors and two `atexit` handlers, predict the order, and
   test normal `return`, `exit`, and `_Exit` paths; or
3. use `fork` plus one successful and one failed `execve`, proving that only
   failure returns to the old child image.

Keep every experiment confined to a temporary directory. Do not alter the
system interpreter, global loader configuration, ASLR policy, or installed
libraries.

### Why? notebook

1. Why can an executable retain section headers that the kernel does not need
   to map it?
2. Why does one `PT_LOAD` commonly contain several sections?
3. Why is `execve` named like a call if success never returns?
4. Why is the dynamic linker ordinary user-mode code despite controlling
   other mapped code?
5. Why does the kernel supply auxv instead of making libc rediscover every
   fact?
6. Why is `_start` not called using the normal C function-call convention?
7. Why must constructors precede `main`, and why is complex constructor
   ordering risky?
8. Why does `_Exit` skip `atexit` handlers?
9. Why can `/proc/self/maps` differ across runs while `readelf -l` does not?
10. Why is `LD_DEBUG` evidence about glibc loader policy rather than kernel
    ELF loading?

---

## References used selectively

Local:

- Bryant and O'Hallaron, *Computer Systems: A Programmer's Perspective*, 3rd
  ed., Chapter 7 §§7.9–7.14, PDF pages 730–754, for executable object files,
  loading, dynamic linking, PIC, and library interposition:
  `source-materials/library/books/01-foundations-and-architecture/computer-systems-programmers-perspective-bryant-ohallaron-3e-global.pdf`
- Kerrisk, *The Linux Programming Interface*, Chapter 6 and Chapter 27, for
  process image layout, argument/environment handling, and program execution:
  `source-materials/library/books/02-os-and-linux/linux-programming-interface-michael-kerrisk.pdf`

Primary references:

- System V ABI generic ELF specification, program loading and dynamic
  linking: <https://gabi.xinuos.com/>
- System V AMD64 ABI, “Process Initialization” and initial stack/auxiliary
  vector: <https://gitlab.com/x86-psABIs/x86-64-ABI>
- Linux man-pages `execve(2)`, `getauxval(3)`, `proc_pid_maps(5)`, `exit(3)`,
  `_exit(2)`, and `ld.so(8)`:
  <https://man7.org/linux/man-pages/man2/execve.2.html>
  <https://man7.org/linux/man-pages/man8/ld.so.8.html>
- Linux kernel source, `fs/binfmt_elf.c`, for the current ELF binary-format
  handler. Implementation details are version-specific:
  <https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/fs/binfmt_elf.c>
- glibc source, `elf/rtld.c`, `csu/libc-start.c`, and architecture startup
  assembly, for one current libc's user-space implementation:
  <https://sourceware.org/git/?p=glibc.git>

**End-of-week bridge:** Week 3 began with C objects and addresses, crossed
lifetime and compiler rules, then followed source through objects and linking.
It now ends at the moment those linked bytes become a running process. The
next module can study operating-system abstractions without leaving this
chain behind.
