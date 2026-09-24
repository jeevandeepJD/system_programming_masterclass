# Day 1 — Addresses You Have Already Seen

**Curriculum alignment:** Stage 5 — C from the Machine's Perspective ·
Pointers, Arrays, and Strings

**Target time:** approximately 3 hours

- Retrieval and the address model: about 30 minutes
- Pointer arithmetic and array decay: about 45 minutes
- Lab A, prediction and observation: about 30 minutes
- Strings, and why they are shaped this way: about 35 minutes
- Lab B, implementing the primitives: about 30 minutes
- Mastery work: about 20 minutes

> Last week you watched a C function turn into `mov`, `lea`, `push`, and
> `call`. You saw a stack frame built and torn down. You saw
> `mov 0x8(%rbp),%eax` reach into memory and bring a value back.
>
> So here is today's only real question:
>
> ```text
> what new thing does a "pointer" add to a machine
> that could already do that?
> ```
>
> The honest answer is: almost nothing. The machine was already using
> addresses. C is just letting you hold one.

---

## 1. You already know what a pointer is; you have not been allowed to say so

Recall the shape of a memory access you disassembled last week:

```text
mov    0x8(%rbp), %eax
```

Three things are present in that one instruction.

1. `%rbp` holds a **number**.
2. That number plus 8 identifies a **location** in memory.
3. The instruction reads four bytes **from** that location.

The number in `%rbp` is an address. It is an ordinary 64-bit integer as
far as the register file is concerned — the same register could hold a
loop counter next instruction. What makes it an address is not its bits
but the fact that this instruction used it as one.

You also know the memory model behind it. Physical DRAM is organised
into rows and columns; the memory controller receives a number and
returns the contents of the cell that number selects. Between the CPU
and DRAM sit caches, and last week you measured what happens when your
access pattern ignores them. Above all of that sits the virtual-address
translation you have not studied yet, which is why every process
believes it owns the same address space.

Here is the point. Every one of those layers takes **a number that
identifies a location**. C's contribution is a type system that lets a
program compute and store those numbers safely, and a set of rules for
what you may do with them.

So define the word carefully:

> A **pointer** is a value whose type says "this is the address of an
> object of type `T`." On this machine its representation is a 64-bit
> address, but its *type* is what gives it meaning.

The distinction between representation and type will decide the rest of
today. Two pointers with identical bit patterns can be different C
objects. One pointer can be perfectly valid at 10:00 and undefined at
10:01 without a single bit changing.

### Why the language needed the concept at all

Three problems forced pointers into every systems language.

**Problem one: a function cannot change what it cannot reach.** C passes
arguments by value. `void twice(int x) { x *= 2; }` modifies a copy that
lives in the callee's frame. If the caller wants its own variable
changed, it must hand over a way to find that variable — its address.

**Problem two: copying is expensive and sometimes wrong.** Passing a
4-kilobyte structure by value means copying 4 kilobytes onto the stack
on every call. Passing its address means copying eight bytes. And if two
parts of a program must agree about *the same* object rather than about
equal values, only an address can express that.

**Problem three: data structures need to refer to each other.** A linked
list, a tree, a hash table with chaining, and a kernel's list of
runnable tasks all need "this record is connected to that record." At
machine level the only way to say that is to store the other record's
address.

None of this is C-specific. It is what the hardware forces on anybody
who wants to build data structures in a flat, numbered memory.

### What a 64-bit address actually is on this machine

Print `sizeof(void *)` here and you get 8. But not all 64 bits are
usable. x86-64 in its common configuration uses 48-bit virtual
addresses, sign-extended into the top bits, which is why user-space
addresses on this VM look like:

```text
0x00007ffe07cf6dfc     a stack address
0x0000000000403074     a .bss address in a non-PIE executable
0x00007f627db3c000     a shared-library mapping
```

The kernel occupies the other half, with the top bits all ones. An
address with a mismatched top half is *non-canonical* and faults on use.
You do not need that detail today, but it explains why pointer values on
Linux cluster into recognisable neighbourhoods rather than spreading
across the full 64-bit range.

---

## 2. Why `p + 1` does not add one

Write a function that advances a pointer and look at what the compiler
emits. Three types, three different constants:

```c
int        *bump_int(int *p)         { return p + 1; }
char       *bump_char(char *p)       { return p + 1; }
struct big *bump_big(struct big *p)  { return p + 1; }   /* 12 bytes */
```

Compiled with `gcc -O2 -S` on this machine:

```text
bump_int:     leaq   4(%rdi), %rax
bump_char:    leaq   1(%rdi), %rax
bump_big:     leaq  12(%rdi), %rax
```

The `+ 1` in the source became `+4`, `+1`, and `+12`. The compiler
multiplied by `sizeof(*p)` before emitting the instruction.

This is not a convenience feature bolted on afterwards. It falls out of
what pointer arithmetic is *for*. Pointers move through arrays, arrays
store elements end to end, and the interesting question is almost always
"give me the next element," not "give me the next byte." The language
made the common case the default.

The inverse operation is symmetric. Subtracting two pointers of the same
type yields how many **elements** apart they are:

```c
long diff(int *a, int *b) { return a - b; }
```

```text
diff:   subq   %rsi, %rdi
        movq   %rdi, %rax
        sarq   $2, %rax
```

Subtract the byte addresses, then arithmetic-shift right by 2 — divide by
`sizeof(int)`. The type of that result is `ptrdiff_t`, which is signed,
because `b` may be after `a`.

### Indexing is arithmetic in disguise

The C standard defines `E1[E2]` as `(*((E1)+(E2)))`. That definition,
not a special array-access instruction, is the whole of indexing. It has
one famous consequence:

```c
arr[2]  ==  *(arr + 2)  ==  *(2 + arr)  ==  2[arr]
```

All four are the same expression after the definition is applied. Lab A
prints all four and gets 30 from each. Never write the fourth form, but
do let it convince you that the subscript operator is sugar.

And when the index is a constant, the "arithmetic" happens at compile
time:

```c
int index3(int *p) { return p[3]; }
```

```text
index3: movl   12(%rdi), %eax
```

There is no multiply and no add at run time. The displacement field of
the addressing mode you met last week absorbed the whole computation.
This is the moment where last week's `disp(base,index,scale)` form stops
being trivia: it exists precisely because array indexing is the most
common thing programs do to memory.

### The rules that keep this honest

Pointer arithmetic is only defined inside an object, plus one position
past its end:

```text
int a[5];

a, a+1, a+2, a+3, a+4    valid, and dereferenceable
a+5                      valid to FORM and compare, not to dereference
a+6                      undefined behaviour, even if you never deref it
a-1                      undefined behaviour
```

The one-past-the-end allowance exists so that loop conditions like
`for (p = a; p != a + 5; ++p)` are legal. It is the smallest concession
that makes iteration expressible.

Why is `a + 6` undefined rather than merely useless? Because the standard
declines to assume memory is flat and unbounded. Segmented
architectures existed; computing an address outside an object could
overflow a segment offset. More relevantly today, the rule lets the
optimiser reason: if the compiler knows `p` points into `a`, it knows
`p - a` fits in a small range, and that enables loop transformations.
You saw the same bargain last week with signed overflow, and you will
see it again on Day 4.

---

## 3. An array is not a pointer, and the difference is where the length lives

This is the most persistently misunderstood corner of C, so let us build
it rather than assert it.

An array is an object. `int a[5]` is twenty contiguous bytes with one
address and a size the compiler knows. `sizeof a` is 20.

A pointer is also an object — eight bytes holding an address. `sizeof p`
is 8.

They are different types with different sizes. What confuses everyone is
a conversion rule:

> In most expression contexts, an expression of array type is converted
> to a pointer to its first element. This is called **decay**.

"Most" has exceptions, and the exceptions are precisely the places where
the array's identity still matters: the operand of `sizeof`, the operand
of `&`, and a string literal used to initialise a `char` array.

Lab A prints all three spellings of the same location:

```text
arr        = 0x7ffca1ec4cf0   sizeof = 20
&arr[0]    = 0x7ffca1ec4cf0   sizeof = 4
&arr       = 0x7ffca1ec4cf0   sizeof = 8
arr + 1    = 0x7ffca1ec4cf4   (one int later)
&arr + 1   = 0x7ffca1ec4d04   (one whole array later)
```

Three identical addresses, three different types:

```text
arr      decays to  int *          → +1 moves 4 bytes
&arr[0]  is         int *          → same thing, written explicitly
&arr     is         int (*)[5]     → +1 moves 20 bytes
```

`0x...cf0 + 20 = 0x...d04`. The pointer type, not the address, decided
the stride.

### Decay is a design decision with a cost

Why would a language throw away the length like that?

Partly history: C inherited from BCPL and B, languages in which a vector
was essentially a pointer to a block. Partly efficiency, and this part
still holds: if arrays were passed by value, calling
`process(big_array)` would copy the whole thing. Decay made the cheap
behaviour the default behaviour, in an era when a PDP-11 had 64 KiB of
address space per process.

The cost is permanent and you pay it every day:

```text
  inside caller: sizeof(arr) = 20, count = 5
  inside callee: sizeof(values) = 8 (a pointer)
  inside callee: caller had to pass count = 5 separately
```

A function receiving `int *values` cannot recover the length. Not "finds
it difficult" — the information is not present. Every bounds bug in C
descends from this one fact. Every API you design must answer it
explicitly, by passing a count, by using a sentinel, or by wrapping the
pointer and length in a structure.

That last option is what modern C leans toward:

```c
struct int_span {
    int   *data;
    size_t count;
};
```

You will see the same shape in Rust's slices, Go's slices, C++'s
`std::span`, and in the kernel's many `(buf, len)` argument pairs. The
idea everyone converged on is: *keep the length adjacent to the
pointer*. C simply never made it a language feature.

### Two-dimensional arrays are one-dimensional with extra typing

```c
int grid[3][4];
```

This is "an array of 3 elements, each of which is an array of 4 `int`."
Lab A confirms the layout:

```text
sizeof(grid)       = 48
sizeof(grid[0])    = 16
sizeof(grid[0][0]) = 4
&grid[0][0] = 0x7ffca1ec4cd0
&grid[1][0] = 0x7ffca1ec4ce0  (row stride = 16 bytes)
flat walk: 0 1 2 3 4 5 6 7 8 9 10 11
```

Rows are contiguous and adjacent. `grid[r][c]` is
`*(int *)((char *)grid + r*16 + c*4)`. That is row-major order, and it
is why iterating rows-then-columns is fast and columns-then-rows is slow
— the cache behaviour you measured last week is a direct consequence of
this layout, not a separate fact to memorise.

Note also what `grid[0]` decays to: `int *`, not `int **`. There is no
array of row pointers anywhere in memory. An `int **` would be a
genuinely different data structure with an extra indirection, and
passing `grid` where `int **` is expected is a type error precisely
because the memory layouts differ.

---

## 4. Lab A — read the machine's own answers

Open:

`03-c-toolchain-and-startup/challenges/day-001-pointer-and-array-lab.c`

Before compiling, write down your predictions for these eight questions.
Do it on paper. Guessing and then reading is worth very little;
committing and then being wrong is worth a lot.

1. What is `sizeof(char *)` compared with `sizeof(double *)`?
2. For a `struct sample { char tag; int value; char *name; }`, how many
   bytes does `p + 1` advance? (You will explain *why* tomorrow.)
3. Which of `arr`, `&arr[0]`, `&arr` print the same address, and which
   have different `sizeof`?
4. How far apart are `arr + 1` and `&arr + 1`?
5. What does `sizeof` report for an array parameter inside a callee?
6. What is the byte stride between `&grid[0][0]` and `&grid[1][0]`?
7. Which printed addresses change between two runs of the program, and
   which stay identical?
8. Is the address of a string literal near the stack or near the other
   constants?

Build and run:

```bash
cd 03-c-toolchain-and-startup/challenges
gcc -std=c17 -Wall -Wextra -O0 -g \
  day-001-pointer-and-array-lab.c -o /tmp/pointer-and-array-lab

/tmp/pointer-and-array-lab
/tmp/pointer-and-array-lab      # run it a second time
```

Question 7 is the one people get wrong. On this VM, `/proc/sys/kernel/
randomize_va_space` contains `2`, so the stack, heap, and shared-library
mappings move on every execution while the static addresses of a
non-PIE executable do not. Confirm it:

```bash
cat /proc/sys/kernel/randomize_va_space
setarch -R /tmp/pointer-and-array-lab | head -20
```

Under `setarch -R`, ASLR is disabled for that child only, and the stack
addresses become reproducible. Use this whenever a lab needs the same
address twice; do not disable ASLR system-wide.

### A finding worth knowing about this machine

Section 7 of the lab prints one address from each storage region. On a
default build here, you will see static objects at `0x40xxxx`, which
tells you something specific: **the plain `gcc` on this system does not
default to PIE.** Check it:

```bash
readelf -h /tmp/pointer-and-array-lab | grep Type
#   Type:                              EXEC (Executable file)

readelf -h /usr/bin/ls | grep Type
#   Type:                              DYN (Position-Independent Executable file)
```

Fedora's *packaged binaries* are hardened, because the distribution's
build system injects `-fPIE -pie -fstack-protector-strong
-D_FORTIFY_SOURCE=3 -Wl,-z,relro,-z,now` through its RPM build flags.
The compiler itself, invoked directly on this VM, is configured upstream
style and does none of that. You can ask for it explicitly:

```bash
gcc -std=c17 -Wall -Wextra -O2 -fPIE -pie -fstack-protector-strong \
    -D_FORTIFY_SOURCE=3 -Wl,-z,relro,-z,now \
    day-001-pointer-and-array-lab.c -o /tmp/hardened
readelf -h /tmp/hardened | grep Type
#   Type:                              DYN (Position-Independent Executable file)
```

Keep this distinction: **toolchain defaults** and **distribution policy**
are different things. It will matter a great deal on Days 6 and 7, when
the presence or absence of PIE and full RELRO changes what the PLT and
GOT look like.

---

## 5. Strings: a data structure chosen in 1970 that you still pay for

C has no string type. It has a convention:

> A **string** is a contiguous sequence of characters terminated by the
> first null character. A "pointer to a string" is a pointer to its
> first character.

Nothing in the language enforces this. `char buf[8]` is eight bytes.
Whether those bytes constitute a string depends entirely on whether a
zero byte appears among them.

### Why terminate rather than count?

The alternative was well known. Pascal-style strings put a length byte
first:

```text
length-prefixed:   [5]['h']['e']['l']['l']['o']
NUL-terminated:    ['h']['e']['l']['l']['o'][0]
```

Both use six bytes here. The trade-offs differ sharply:

```text
                        length-prefixed        NUL-terminated
length of a string      O(1), read the byte    O(n), scan for zero
maximum length          255 with one byte      unbounded
a suffix of a string    needs a new header     just a pointer further in
copying                 length known up front  discovered while copying
appending               update the header      find the end first
```

That third row was the decisive one. In a NUL-terminated world,
`s + 4` is a perfectly good string with no allocation and no copying. In
a length-prefixed world it is not a string at all. On a machine where
memory was the scarcest resource, being able to name substrings for free
mattered.

The second row mattered too: a one-byte length caps you at 255
characters, and a larger length field costs space on every string in a
64 KiB address space.

Both choices were defensible in 1970. One of them turned out to make an
entire class of security vulnerability structurally easy, because every
string operation is a loop whose termination depends on data rather than
on a count the caller supplied.

### What the convention costs, concretely

```c
size_t n = strlen(s);
```

is a loop. Calling it inside another loop turns an O(n) algorithm into
O(n²), which is a real and common performance bug.

```c
char dst[8];
strcpy(dst, src);
```

writes until it finds a zero in `src`. If `src` is longer than 7
characters, it writes past `dst`. `strcpy` has no parameter that could
prevent this; the interface cannot be used safely without the caller
already knowing the answer.

Worse, the failure is silent. Nothing returns an error. The write
succeeds, into whatever happened to follow `dst`.

### The functions that replaced them, and their exact contracts

Be precise about these, because their differences are the entire point.

**`strncpy(dst, src, n)`** — *not* a safe `strcpy`. It writes exactly `n`
bytes: it pads with zeros if `src` is short, and it does **not**
terminate if `src` is `n` or more characters. It was designed for
fixed-width record fields in early Unix directory entries, not for
strings. Using it as "safe strcpy" produces unterminated buffers.

**`snprintf(dst, size, "%s", src)`** — always terminates when `size > 0`.
Returns the length it *would* have written, so `ret >= size` detects
truncation. Available everywhere. This is the portable default.

**`strlcpy(dst, src, size)`** — always terminates, returns `strlen(src)`,
so `ret >= size` detects truncation. Originally from OpenBSD; added to
glibc in version 2.38, so it is available on this system's glibc 2.43,
but it is not in ISO C.

**`memcpy(dst, src, n)`** — copies exactly `n` bytes and knows nothing
about terminators. When you already know the length, this is the honest
function to call, and it is the fastest.

You will implement the `strlcpy` contract yourself in Lab B. Implementing
it is the fastest way to understand why "returns the source length"
rather than "returns how much it copied" is the right design: the caller
needs to know whether data was lost, and the amount copied cannot tell
them that.

### Literals are not arrays, even when they look like one

```c
const char *literal  = "string literal";
char        writable[] = "string literal";
```

The first makes `literal` point into `.rodata`. The second creates a
15-byte array on the stack and copies the literal into it. Lab A shows
both addresses and then writes to the array:

```text
  literal points at       0x401837
  writable[] lives at     0x7ffca1ec4d01
  after writable[0]='S':  "String literal"  (literal is still "string literal")
```

Writing through `literal` is undefined behaviour. On Linux it usually
segfaults, because `.rodata` lands in a read-only mapping — which is a
*loader* decision you will watch being made on Day 7, not a language
guarantee. Declare literal pointers `const char *` so the compiler
catches the mistake instead of the MMU.

---

## 6. Lab B — implement the primitives, then break them on purpose

Open:

`03-c-toolchain-and-startup/challenges/day-001-string-functions-lab.c`

Part one has five TODO functions: `my_strlen`, `my_strchr`,
`my_strlcpy`, `my_strrev`, and `count_occurrences`. The tests are fixed;
do not edit them.

```bash
gcc -std=c17 -Wall -Wextra -O0 -g \
  day-001-string-functions-lab.c -o /tmp/string-functions-lab
/tmp/string-functions-lab
```

Two of these have a subtlety worth pausing on before you start.

`my_strchr(s, '\0')` must find the terminator, not return `NULL`. Write
the loop so that the comparison happens before the termination check, or
you will fail that test and learn something useful about loop structure.

`my_strlcpy` returns `strlen(src)`, not the number of bytes copied. Read
section 5 again if that seems backwards.

**One deliberate compiler warning.** This file is expected to produce
exactly one diagnostic:

```text
day-001-string-functions-lab.c:127:12: warning: function returns address
of local variable [-Wreturn-local-addr]
```

That is `bug3_dangling`, and it is there on purpose. Every other lab
this week compiles silently under `-Wall -Wextra`.

### Part two: making invisible bugs visible

The three `bug` modes are all wrong. Run each one in the plain build
first and record what happens, because "nothing happened" is a result:

```bash
/tmp/string-functions-lab bug1
/tmp/string-functions-lab bug2
/tmp/string-functions-lab bug3
```

Then build with AddressSanitizer. **On this machine, gcc's ASan runtime
package (`libasan`) is not installed**, so `gcc -fsanitize=address`
fails at link time with `cannot find /usr/lib64/libasan.so.8.0.0`. Clang
ships its own runtime and works out of the box:

```bash
clang -std=c17 -Wall -Wextra -O1 -g -fsanitize=address \
  day-001-string-functions-lab.c -o /tmp/string-functions-lab-asan

/tmp/string-functions-lab-asan bug1
```

```text
==901644==ERROR: AddressSanitizer: heap-buffer-overflow on address
0x7bcaef3e0015 at pc 0x00000041ae3f
READ of size 6 at 0x7bcaef3e0015 thread T0
    #0 in strlen
    #1 in bug1_read_past_end day-001-string-functions-lab.c:110:9
0x7bcaef3e0015 is located 0 bytes after 5-byte region
  [0x7bcaef3e0010,0x7bcaef3e0015)
```

Read that carefully. `malloc(5)` gave five bytes, `memcpy` filled all
five with characters, and `strlen` read a sixth looking for a
terminator. The report names the exact byte, the exact source line, and
the allocation it belongs to.

`bug2` reports a **stack-buffer-overflow** and even prints the frame
layout showing which variable was overrun. `bug3` reports a
**stack-use-after-scope** inside `printf`.

### How AddressSanitizer knows

It is worth understanding, because it explains both its power and its
limits. ASan does two things at compile time:

1. It replaces allocations with padded ones. Every heap block and every
   instrumented stack variable is surrounded by **redzones** — bytes
   marked poisoned.
2. It maintains **shadow memory**: one byte of metadata for every eight
   bytes of application memory, recording how many of those eight bytes
   are currently addressable.

Every load and store the compiler emits gets a shadow lookup inserted
before it. If the shadow says poisoned, the program stops and prints the
report. The cost is roughly 2× slower and 3× more memory, which is why
this is a test-build tool, not a production one.

The limits follow from the mechanism. ASan finds *spatial* errors
(reading outside an object) and *temporal* errors (reading an object
after its lifetime). It does not find uninitialised reads — that is
MemorySanitizer's job — and it does not find logic errors. Valgrind's
Memcheck catches an overlapping but different set, works on unmodified
binaries, and is far slower. You will use both this week.

---

## 7. The four ways a pointer goes wrong

Every pointer bug you will ever debug is one of these. Name them, so you
can recognise them under time pressure.

### Out of bounds

The pointer is valid, points at a real object, and you moved it too far.
`bug1` and `bug2` are this. The fix is always a bound the code can
actually check, which means the length has to be *present* — back to
section 3.

### Dangling

The pointer's bits are fine; the object they identify is gone. `bug3`
returns the address of a local whose frame is about to be reused. The
address is still a real, mapped address, which is why the program often
does not crash. Tomorrow's lesson and Day 3 both return to this.

### Uninitialised

```c
int *p;
*p = 5;    /* p holds whatever was in that stack slot */
```

At `-O0` this often reads leftover stack garbage and segfaults. At `-O2`
the compiler may reason that a correct program never does this and
optimise on that basis. Day 4 shows exactly that happening.

### Type-confused

The bits are a valid address, but you are reading the object through the
wrong type. Sometimes this is deliberate and legal (`unsigned char`
access is always allowed); sometimes it is a strict-aliasing violation
with real consequences. Day 2 covers the legal forms and Day 4 covers
what happens when you break the rule.

### Where Linux draws a hard line

The kernel faces a fifth problem you never see in userspace: a pointer
that a *user process* supplied. The kernel runs at a privilege level
where dereferencing an arbitrary address would be catastrophic, and the
address might be unmapped, might belong to a different process's page
tables by the time it is used, or might point into kernel memory as a
deliberate attack.

Kernel code therefore annotates such pointers:

```c
ssize_t my_read(struct file *f, char __user *buf, size_t len, loff_t *off);
```

`__user` expands to nothing in a normal build, but the `sparse` static
checker enforces that a `__user` pointer is never dereferenced directly.
The only legal way to touch it is through the accessor functions:

```c
if (copy_to_user(buf, kernel_data, len))
        return -EFAULT;
```

`copy_to_user` validates the range, handles a fault by returning the
number of bytes not copied, and on modern x86-64 toggles SMAP — a CPU
feature that traps kernel accesses to user pages unless explicitly
permitted. This is the privilege boundary from Week 1 Day 7, now visible
as a *pointer* discipline rather than as a mode bit.

Hold that thought. The same idea — "an address is only meaningful
together with the address space it belongs to" — is what makes the rest
of this course's virtual-memory and virtualization material work.

---

## 8. Predict, run, explain: a short integrated exercise

Do this on paper before touching a compiler.

```c
char  msg[16] = "hello";
char *p = msg;
char *q = msg + 5;

/* 1 */  sizeof msg
/* 2 */  sizeof p
/* 3 */  strlen(msg)
/* 4 */  q - p
/* 5 */  *q
/* 6 */  msg[7]
/* 7 */  p[strlen(msg)]
/* 8 */  &msg[16]
/* 9 */  &msg[17]
```

For each one, state the **value** and the **C rule that produced it**.
Items 6, 8, and 9 are the interesting ones:

- 6 is defined and predictable. `char msg[16] = "hello"` initialises all
  sixteen bytes; the initialiser supplies six and the rest are zeroed.
  That zero-fill rule applies to any array with an initialiser, and it is
  why `msg[7]` is 0 rather than garbage.
- 8 is the one-past-the-end address. Forming it is legal; dereferencing
  it is not.
- 9 is undefined behaviour at the moment the address is *formed*, before
  any dereference.

Then verify with a five-line program and explain any disagreement.

---

## 9. Mastery checkpoint

The tracker checkpoint for this section is:

> **Define an address precisely, explain pointer arithmetic, distinguish
> arrays from pointers, and handle strings safely.**

### Explain

Without notes, and out loud:

- what a pointer value is at the machine level, and what its *type*
  adds that its bits do not;
- why `p + 1` advances by `sizeof(*p)`, and where that multiplication
  ends up in the generated code;
- what array-to-pointer decay is, which three contexts are exempt, and
  what information is destroyed by it;
- why `sizeof arr` and `sizeof p` differ even when `arr` and `p` hold
  the same address;
- why a NUL-terminated string makes `strlen` O(n) and substrings free,
  and which of those two the 1970s designers were buying;
- the exact contract of `strncpy`, `snprintf`, and `strlcpy`, and why
  only two of the three are safe;
- why the kernel refuses to dereference a `__user` pointer directly.

### Draw

One diagram, on one page. Show a stack frame containing `int a[5]` and
`int *p = &a[2]`. Mark:

- the five element boundaries with their byte offsets;
- the eight bytes that `p` itself occupies, and the arrow from them;
- where `a + 5` points and why that address is legal to form;
- the `.rodata` region, a string literal in it, and a `const char *` in
  the frame pointing at it;
- the direction the stack grows, and where the next call's frame
  would go.

Beside it, draw the same array as a `struct int_span` and mark what
information the second version has that the first loses at a function
boundary.

### Observe

Run both labs and record:

1. Two runs of Lab A, plus one `setarch -R` run. Which addresses are
   stable and which are not, and what distinguishes the two groups?
2. `readelf -h` on your Lab A binary and on `/usr/bin/ls`. State the two
   `Type:` values and explain the difference in one sentence.
3. All three ASan reports from Lab B, with the specific byte offset each
   one names.
4. `objdump -d` your own `my_strlen` at `-O0` and at `-O2`. At `-O2`,
   gcc may vectorise it. Count the loads.

### Build

1. Complete all five TODO functions in Lab B until every test passes.
2. Then change `my_strlcpy` to return the number of bytes it copied
   instead of `strlen(src)`, and write down which test breaks and what a
   caller could no longer detect.
3. Write a `struct int_span` version of a `sum` function and compare its
   call site with the `(pointer, count)` version. Which one can be
   called incorrectly, and how?

### Why? notebook

1. Why does C pass arrays by reference-to-first-element rather than by
   value, and what would change if it did not?
2. Why is `&arr` a different type from `arr` when they print the same
   address?
3. Why is forming `a - 1` undefined even though the resulting address is
   usually mapped?
4. Why does `sizeof` not decay its array operand?
5. Why does a `char` array initialised from a shorter literal have
   defined values in its tail?
6. Why is `strncpy` not a safe `strcpy`, and what was it actually for?
7. Why can AddressSanitizer find a one-byte overflow that a normal run
   cannot?
8. Why does disabling ASLR make some lab addresses reproducible and
   leave others unchanged?
9. Why must the kernel treat a user-supplied pointer as data rather than
   as an address?

---

## Mental model at the end

```text
memory is a numbered array of bytes
        ↓
a load/store instruction takes one of those numbers
        ↓
C gives that number a TYPE, and calls it a pointer
        ↓ the type supplies the stride
pointer arithmetic moves in units of the pointed-to object
        ↓ objects laid end to end
an array is contiguous storage the compiler knows the size of
        ↓ decay, at almost every use
the callee receives only the first element's address
        ↓ the length must be carried some other way
counts, sentinels, or a pointer-plus-length structure
        ↓ the sentinel choice, for text
a string is "bytes until the first zero"
        ↓ therefore
every string operation is a loop bounded by data, not by the caller
```

The last arrow is the one worth keeping. C's string bugs are not
carelessness; they are the predictable output of a representation whose
end is discovered rather than declared. Once you see that, the correct
habit follows on its own: *always know the length, and always know where
it came from.*

---

## References used selectively

- Bryant and O'Hallaron, *Computer Systems: A Programmer's Perspective*,
  3rd ed., §3.8 "Array Allocation and Access" and §3.9.1 "Structures",
  for the mapping from array indexing to x86-64 addressing modes.
  Local PDF: `references/library/books/01-foundations-and-architecture/computer-systems-programmers-perspective-bryant-ohallaron-3e-global.pdf`
- Kerrisk, *The Linux Programming Interface*, Ch. 6 "Processes", for the
  layout of text, data, bss, heap, and stack in a Linux process image —
  the regions Lab A prints addresses from.
  Local PDF: `references/library/books/02-os-and-linux/linux-programming-interface-michael-kerrisk.pdf`
- ISO/IEC 9899:2018 (C17), §6.3.2.1 for array-to-pointer conversion,
  §6.5.6 for additive operators and the one-past-the-end rule, and
  §6.5.2.1 for the definition of subscripting.
- Google's AddressSanitizer algorithm description, for shadow memory and
  redzones: <https://github.com/google/sanitizers/wiki/AddressSanitizer>
- Linux kernel documentation, `Documentation/dev-tools/sparse.rst` and
  the `__user` annotation, plus `arch/x86/include/asm/uaccess.h` for
  `copy_to_user` and the SMAP interaction:
  <https://docs.kernel.org/dev-tools/sparse.html>
- glibc 2.38 release notes for the addition of `strlcpy`/`strlcat`:
  <https://sourceware.org/glibc/wiki/Release/2.38>

**After Day 1:** bring your two lab transcripts, the one-page diagram,
and your answers to the section 8 exercise. Tomorrow we group these
objects together — and immediately discover that the compiler inserts
bytes you did not ask for.
