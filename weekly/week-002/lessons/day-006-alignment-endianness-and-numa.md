# Day 6 — Alignment, Endianness, and NUMA

**Curriculum alignment:** Stage 4 — Memory Hierarchy and Performance ·
Alignment, Endianness, and NUMA

**Target time:** approximately 2 hours 45 minutes

- Alignment, padding, and ABI layout: 55 minutes
- Byte order and serialization: 40 minutes
- Controllers, channels, sockets, and NUMA: 45 minutes
- Layout and topology labs: 35 minutes
- Explain · Draw · Observe · Why: 10 minutes

> Yesterday, an address selected a cache line, set, and tag. Today we ask a
> prior question: who arranged the bytes at those addresses, and where in the
> machine will a cache miss eventually be served?

A C declaration looks logical: one character, one integer, one short, one
double. Hardware sees byte addresses, naturally aligned transfers, cache
lines, interconnects, memory controllers, and perhaps several unequal paths
to DRAM. A network peer sees a byte sequence and cannot infer the sender's C
ABI or native byte order.

The connecting idea is **layout**. Layout determines which addresses are
touched. Those addresses influence whether an access is legal or efficient,
whether two binaries agree, how bytes must be serialized, which cache lines
are fetched, and—on a NUMA host—which memory controller may satisfy a miss.

## Tracker objectives

By the end, you should be able to:

- **Explain alignment requirements** separately from alignment performance.
- **Recognize structure padding** and connect it to an ABI.
- **Understand byte order** and use an explicit wire format.
- **Describe NUMA implications**, including first touch and locality.

The tracker labs are present: inspect structure layouts, write and check
endian conversions, and inspect the host with `lscpu`, `numactl`, `hwloc`,
and safe fallbacks.

The mastery checkpoint is:

> **Explain how layout choices affect correctness, ABI compatibility, and
> performance.**

---

## 1. Alignment starts with addresses

An object is aligned to `A` bytes when its starting address is a multiple of
`A`:

```text
aligned(address, A)  ⇔  address mod A = 0
```

An eight-byte object at `0x1000` is eight-byte aligned. At `0x1004` it is
four-byte aligned but not eight-byte aligned.

Why would a machine care? A naturally aligned access is easier to route
through fixed-width hardware. A misaligned access may cross boundaries:

```text
8-byte transfer lanes:

0x1000  [ 0 1 2 3 4 5 6 7 ] [ 8 9 a b c d e f ]
aligned load of 8 bytes:       └───────────────┘

0x1004  [ 0 1 2 3 4 5 6 7 ] [ 8 9 a b c d e f ]
misaligned load of 8 bytes:            └───────────────┘
                                      crosses a boundary
```

The real penalty depends on the microarchitecture and on which boundary is
crossed. An access contained within one cache line may be cheap on a current
x86-64 core. Crossing a cache-line boundary can require two line accesses.
Crossing a page boundary may require two translations and can fault if the
second page is absent. Atomicity guarantees can also be narrower for
misaligned objects.

That gives us two different questions:

1. **Is the access valid under the language, ABI, and ISA?**
2. **If valid, how much does this machine charge for it?**

Never replace the first question with the second. “It worked quickly on my
x86 laptop” does not prove that a C expression is defined or portable.

### The C requirement

Every complete object type has an alignment requirement. C exposes it with
`_Alignof(type)` in C11 and later (also spelled `alignof(type)` through
`<stdalign.h>` in C11–C17). An object must be created at an address suitable
for its type.

```c
printf("%zu\n", _Alignof(uint32_t));
```

Allocators and declarations arrange this for ordinary objects. Trouble
appears when code invents a typed pointer into a byte buffer:

```c
unsigned char raw[8];
uint32_t value = *(uint32_t *)(raw + 1);  /* not a portable decoder */
```

The converted address may not satisfy `uint32_t` alignment. The expression
can also run into C object/effective-type and representation rules. Some
ISAs trap on some unaligned loads; others handle them in hardware; compilers
may assume that a typed pointer is correctly aligned and optimize on that
basis before the CPU ever executes an instruction.

For moving an object representation to or from bytes, `memcpy` is the
portable tool:

```c
uint32_t value;
memcpy(&value, raw + 1, sizeof value);
```

`memcpy` establishes the byte copy without dereferencing a misaligned
`uint32_t *`. Compilers recognize fixed-size copies and commonly lower them
to an efficient load when the target permits one.

### ISA behavior is architecture-dependent

- x86 generally supports unaligned ordinary integer loads and stores, though
  boundary crossings can cost more and special atomic/vector rules matter.
- AArch64 supports many ordinary unaligned accesses to normal memory, but not
  every instruction or memory type behaves identically.
- Other architectures or modes may trap and let the operating system emulate
  an access, or may reject it.

The safe conclusion is not “architecture X always allows unaligned access.”
It is: consult the language rule first, then the ISA instruction and memory
type, then measure the actual processor.

---

## 2. Why structs contain bytes you did not declare

Consider:

```c
struct mixed {
    char marker;
    uint32_t count;
    uint16_t code;
    double value;
};
```

Suppose this ABI gives `char`, `uint16_t`, `uint32_t`, and `double`
alignments of 1, 2, 4, and 8 bytes. One plausible layout is:

```text
offset   0       1 2 3       4 5 6 7       8 9      10 11 12 13 14 15
       ┌──────┬───────────┬─────────────┬─────────┬───────────────────┐
       │marker│  padding  │    count    │  code   │      padding      │
       └──────┴───────────┴─────────────┴─────────┴───────────────────┘

offset  16 17 18 19 20 21 22 23
       ┌────────────────────────┐
       │         value          │
       └────────────────────────┘
```

The compiler inserts **internal padding** before a member when the next
member needs a more aligned address. It may add **tail padding** after the
last member so every element in an array of the struct has suitable
alignment:

```text
base of element i+1 = base of element i + sizeof(struct mixed)
```

`sizeof` includes padding. It must: pointer arithmetic on a struct array uses
`sizeof` as the stride.

Reordering the fields from strictest to loosest alignment often reduces
padding:

```c
struct reordered {
    double value;
    uint32_t count;
    uint16_t code;
    char marker;
};
```

But “largest first” is a heuristic, not a language rule or always the best
design. Layout changes can:

- break a stable binary interface or on-disk assumption;
- put fields used together on different cache lines;
- increase false sharing between threads;
- improve one array scan by shrinking the stride;
- worsen another access pattern by separating hot fields.

Measure the workload and preserve interfaces deliberately.

### Layout belongs to an ABI, not just the source

C guarantees declaration order and forbids overlap between distinct
members, but implementation and ABI rules determine concrete offsets,
padding, alignment, and argument/return conventions. Separately compiled
code interoperates because it follows the same ABI.

Use the compiler's results instead of guessing:

```c
sizeof(struct mixed)
_Alignof(struct mixed)
offsetof(struct mixed, count)
```

Kernel and systems tooling can inspect compiled layouts too:

```bash
pahole /tmp/day6-layout 2>/dev/null || true
readelf --debug-dump=info /tmp/day6-layout 2>/dev/null | less
```

`pahole` needs debug information, so build with `-g`. Its absence is not a
lab failure.

### Packed is a protocol tool, not free compression

Compiler extensions such as `__attribute__((packed))` reduce or remove
padding. That may be necessary for an explicitly specified hardware or wire
layout, but a packed member can live at an address unsuitable for its type.
Taking its address and dereferencing it as an ordinary typed pointer can be
unsafe or slow.

Packing also does **not** define:

- integer byte order;
- integer widths unless fixed-width types are chosen;
- floating-point representation;
- bit-field allocation;
- protocol versioning;
- validation of untrusted lengths.

For a wire format, explicit encode/decode routines are clearer and more
portable than sending a packed struct.

### Padding bytes are real but not payload

Padding participates in `sizeof`, but its values need not represent any
field. Consequences:

- `memcmp(&a, &b, sizeof a)` is not a valid general test of struct value
  equality;
- hashing every byte may include irrelevant padding;
- writing a struct directly to disk or a socket leaks an ABI-specific format
  and can expose uninitialized padding;
- changing compiler, ABI, options, or architecture can change the format.

Compare fields. Serialize fields.

---

## 3. Endianness answers one narrow question

Memory is byte-addressed. A multi-byte integer must assign its significance
to increasing byte addresses.

For the value `0x12345678`:

```text
address             A      A+1    A+2    A+3

big-endian         0x12   0x34   0x56   0x78
little-endian      0x78   0x56   0x34   0x12
```

**Big-endian** stores the most significant byte at the lowest address.
**Little-endian** stores the least significant byte there.

Endianness does not mean that bits run backward inside each byte. It does not
by itself specify bit-field layout, instruction encoding, text encoding, or
the order in which a CPU performs operations.

Observe an object's representation legally through `unsigned char` bytes or
`memcpy`:

```c
uint32_t word = UINT32_C(0x01020304);
unsigned char bytes[sizeof word];
memcpy(bytes, &word, sizeof bytes);
```

If `bytes[0]` is `0x04`, this run observed little-endian order for that
object. If it is `0x01`, it observed big-endian order.

### Network byte order

Internet protocols define integers in **network byte order**, which is
big-endian. Socket interfaces provide conversions:

```c
uint16_t port_wire = htons(port_host);
uint32_t addr_wire = htonl(addr_host);

uint16_t port_host = ntohs(port_wire);
uint32_t addr_host = ntohl(addr_wire);
```

On a little-endian host these commonly compile to a byte swap. On a
big-endian host they may compile to no operation. Correct source uses the
conversion either way because the source states the boundary between host
representation and protocol representation.

The names handle 16- and 32-bit unsigned quantities. For 64-bit values or a
file format, use a specified library/API or encode bytes explicitly; do not
assume a nonstandard helper exists everywhere.

### Serialization is more than byte swapping

A robust format specifies:

```text
field order
field widths and signedness
byte order
length and bounds rules
optional/versioned fields
text encoding
floating-point representation, if any
error handling
```

For a four-byte big-endian field:

```c
uint32_t decode_be32(const unsigned char p[4])
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |
           (uint32_t)p[3];
}
```

This describes the format directly. It does not depend on host endianness,
struct padding, alignment, or a type-punned load.

Before decoding untrusted input, prove that all required bytes are present.
Byte-order correctness does not prevent an out-of-bounds read.

---

## 4. Run the layout and byte-order lab

The existing lab avoids undefined behavior:

```text
weekly/week-002/challenges/day-006-layout-endian-lab.c
```

Build and run:

```bash
gcc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g \
  weekly/week-002/challenges/day-006-layout-endian-lab.c \
  -o /tmp/day6-layout

/tmp/day6-layout
```

Predict first:

1. What are the offsets and total size of `struct mixed`?
2. Which padding disappears in `struct reordered`?
3. What four bytes represent `0x01020304` on this host?
4. Why does the `memcpy` from `unaligned + 1` remain safe when a
   `uint32_t *` dereference would not be portable?

Then preserve the output. Results from one build are evidence about this
compiler/target ABI, not a universal C layout.

### Inspect what the compiler emitted

```bash
objdump -drwC -Mintel /tmp/day6-layout | less
readelf -h /tmp/day6-layout | grep 'Data:'
file /tmp/day6-layout
```

Find the fixed-size `memcpy` operation in the disassembly. The compiler may
inline it. That is the payoff of expressing the operation legally: the
optimizer can still select an efficient target instruction.

### Extend safely

Add a function that encodes a `uint32_t` into four explicitly big-endian
bytes, then decode it with the existing expression. Check these values:

```text
0x00000000
0x00000001
0x01020304
0x80000000
0xffffffff
```

Do not reveal correctness by printing only. Return nonzero when any
round-trip fails, so the lab can be used in a script or test runner.

---

## 5. From a cache miss to a memory controller

“RAM” is not one uniform bucket. A simplified current server path is:

```text
load instruction
      ↓ virtual-to-physical translation
private cache(s)
      ↓ miss
shared cache / coherence fabric
      ↓ miss or remote request
socket/package interconnect
      ↓
integrated memory controller
      ↓ selects channel/rank/bank/row
DRAM module
```

Exact organization varies. A **memory controller** schedules requests and
speaks the DRAM protocol. A controller may expose multiple **channels** that
can transfer in parallel. DIMMs, ranks, banks, rows, and columns add more
levels.

More populated channels can increase available memory bandwidth, but only
when the platform's population rules and workload permit parallel use.
Channel count is not NUMA node count:

- channels are parallel paths below a controller;
- a NUMA node is an operating-system-visible locality domain containing CPUs
  and/or memory;
- one NUMA node can have several channels;
- firmware and platform design decide what Linux reports.

Commodity tools often report CPU and NUMA topology but not reliable channel
population. Use firmware/vendor tools or hardware documentation when channel
details matter; do not infer channels from `lscpu`'s NUMA output.

---

## 6. Packages, cores, threads, and nodes are different

Linux and hardware tools use several topology levels:

- **logical CPU / processing unit (PU):** one schedulable hardware thread;
- **core:** an execution core; it may expose one or more logical CPUs through
  simultaneous multithreading;
- **socket/package:** a physical processor package containing cores, caches,
  and often one or more memory controllers;
- **NUMA node:** CPUs and memory grouped by access locality.

These relationships are common, not universal:

```text
Machine
├── Package 0
│   ├── NUMA node 0: cores + local memory
│   └── NUMA node 1: cores + local memory
└── Package 1
    ├── NUMA node 2: cores + local memory
    └── NUMA node 3: cores + local memory
```

A package can contain multiple NUMA nodes. A virtual machine may expose one
virtual node over hardware with a different shape. A container normally sees
the host kernel's topology filtered by CPU and memory constraints.

In a **uniform** memory model, access cost is treated as approximately equal
for every CPU-memory pairing. In **non-uniform memory access (NUMA)**,
latency and sustainable bandwidth depend on where the CPU runs and where the
physical page resides:

```text
CPU on node 0 → memory on node 0     local path
CPU on node 0 → memory on node 1     interconnect + remote controller
```

Remote memory is still ordinary coherent memory. NUMA affects cost, not
whether a valid pointer works.

---

## 7. First touch: virtual reservation is not physical placement

An anonymous `mmap` or a large `malloc` can reserve virtual address space
without immediately assigning a private physical page to every virtual page.
Linux allocates pages on demand.

Under the default local policy, an anonymous page is generally allocated
near the CPU that triggers allocation. For writable anonymous memory, the
first write commonly causes that private allocation:

```text
thread running on node 0
        ↓ first write faults on virtual page
kernel allocates a physical page under current memory policy
        ↓ usually local to node 0 when available and allowed
later thread runs on node 1
        ↓ accesses the same page remotely
```

This is the practical **first-touch** rule. State it with its conditions:

- placement follows the policy active when a new page is allocated;
- a first read of untouched anonymous memory may use the kernel's shared
  zero page rather than allocate the final private page;
- cpusets, cgroups, affinity, memory pressure, explicit policies, huge pages,
  migration, and automatic NUMA balancing can change the result;
- fallback may allocate from another allowed node when local memory is
  unavailable.

Therefore initialization strategy matters. If one thread initializes an
entire large array on node 0 and workers later split across all nodes, many
workers may use remote memory. Parallel initialization by the same workers
that later process each region often establishes better locality.

First touch is not a source-level ownership rule, and it does not guarantee a
page stays put forever.

### Linux NUMA policy

Linux supports task and address-range policies including:

- **local/default:** prefer allocation local to the allocating CPU;
- **bind:** allocate from a specified set of nodes, subject to policy rules;
- **preferred:** try preferred node(s), with fallback;
- **interleave:** distribute allocations across nodes, useful for bandwidth
  or avoiding one-node concentration.

`numactl` exposes these policies for a launched process. Binding CPUs and
binding memory are separate actions:

```bash
numactl --cpunodebind=0 --membind=0 -- command
numactl --interleave=all -- command
```

Do not run these examples mechanically on node 0. First inspect which nodes
exist and are allowed. Strict memory binding can fail when a node lacks
available memory, and a container may prohibit it.

---

## 8. Inspect the host without assuming special tools

Run the read-only helper:

```bash
bash weekly/week-002/challenges/day-006-topology-probe.sh \
  | tee /tmp/day6-topology.txt
```

It tries richer tools when installed and falls back to `/proc` and `/sys`.
It changes no affinity or memory policy.

You can also run the core commands individually:

```bash
lscpu
lscpu -e=CPU,NODE,SOCKET,CORE,ONLINE
lscpu --caches

numactl --hardware
numactl --show

lstopo-no-graphics
# or:
lstopo
```

Interpret failures as evidence:

- **command not found:** the optional package is absent;
- **one NUMA node:** this host or VM exposes uniform topology to the guest;
- **empty node distance files:** firmware or virtualization may not expose
  the data;
- **restricted CPU list:** a container/cgroup or affinity mask limits what
  this process can use;
- **policy operation not permitted:** the environment does not authorize it.

`lscpu` gathers from sysfs, `/proc/cpuinfo`, and architecture-specific
libraries. `hwloc` builds a richer hierarchy and may reveal cache sharing and
I/O locality. Neither tool turns a virtualized topology into a bare-metal
fact.

### Read the sysfs representation

```bash
cat /sys/devices/system/cpu/online 2>/dev/null
cat /sys/devices/system/node/online 2>/dev/null

for node in /sys/devices/system/node/node[0-9]*; do
    [ -d "$node" ] || continue
    printf '%s cpulist=' "${node##*/}"
    cat "$node/cpulist" 2>/dev/null || printf 'unavailable\n'
done
```

Node-distance matrices, when exposed:

```bash
for f in /sys/devices/system/node/node[0-9]*/distance; do
    [ -r "$f" ] || continue
    printf '%s: ' "$f"
    cat "$f"
done
```

The numbers are firmware-provided relative costs, not nanoseconds. Lower
normally means nearer. Compare relationships; do not label a value “latency
in ns.”

### Optional NUMA behavior experiment

Only do this when at least two nodes are available, `numactl` is installed,
and the allowed CPU/memory masks include them.

Choose a memory-bandwidth benchmark already installed, or write one that
allocates a large buffer, writes every page, then repeatedly scans it.
Record:

1. which node the initializer ran on;
2. which node the scanning thread ran on;
3. local and remote elapsed times over several repetitions;
4. buffer size, page size, CPU list, and node-distance matrix.

Launch variants using nodes that actually exist:

```bash
numactl --cpunodebind=NODE_A --membind=NODE_A -- ./scan
numactl --cpunodebind=NODE_B --membind=NODE_A -- ./scan
numactl --cpunodebind=NODE_A --interleave=all -- ./scan
```

Do not expect a dramatic result. Cache residency, hardware prefetch, thread
migration, automatic NUMA balancing, VM topology, page migration, frequency
changes, and workload size can obscure the difference. A null result is not
permission to invent one.

---

## 9. One causal chain from declaration to DRAM

```text
C member order and ABI
        ↓ choose offsets, padding, alignment, total stride
compiler-generated loads/stores
        ↓ touch particular virtual byte addresses
MMU and page tables
        ↓ select physical pages
cache line geometry
        ↓ combines nearby fields; may split or falsely share them
NUMA placement policy + first allocation
        ↓ place a physical page in a locality domain
CPU placement
        ↓ determines local or remote path
memory controller and channels
        ↓ schedule DRAM transfers
```

This is why a “small” source decision can be visible many layers below:

- reordering a struct changes array stride;
- stride changes how many objects fit in a cache line;
- cache-line use changes bandwidth and coherence traffic;
- page initialization changes physical placement;
- scheduler placement changes the distance to those pages.

None of those effects makes one universal best layout. The correct design
starts with correctness and interface stability, then measures the relevant
access pattern on the target system.

---

## 10. Mastery evidence

### Explain

Without notes:

- separate C alignment validity from CPU unaligned-access performance;
- explain internal and tail padding, including why arrays need tail padding;
- explain why an ABI needs stable member offsets;
- explain why packed structs do not solve serialization;
- draw `0x12345678` in little- and big-endian memory;
- explain why network byte order conversions appear even on a big-endian
  host;
- distinguish logical CPU, core, package, NUMA node, controller, and channel;
- explain first touch without claiming physical memory is allocated at
  `malloc` time;
- explain why remote NUMA memory remains coherent but can cost more.

### Draw

Draw:

1. `struct mixed` byte by byte, with member offsets and padding;
2. the same fields reordered, with `sizeof` and alignment;
3. one four-byte integer in both byte orders;
4. two packages, four NUMA nodes, local memory controllers, and one remote
   access crossing the interconnect;
5. the declaration-to-DRAM causal chain from section 9.

### Observe

Keep:

- the C lab's `sizeof`, `_Alignof`, `offsetof`, and byte output;
- `file`/ELF byte-order evidence;
- `/tmp/day6-topology.txt`;
- online/allowed CPUs and nodes;
- a node-distance matrix or a statement that none was exposed;
- whether the environment appears to be bare metal, VM, or container;
- optional local/remote NUMA measurements, including null results.

### Build

1. Add checked big-endian encode/decode round trips to the C lab.
2. Add a third struct order and predict its layout before compilation.
3. Place two `struct mixed` objects in an array and verify that the second
   address differs by exactly `sizeof(struct mixed)`.
4. Write a scanner whose initialization and read phases can be independently
   CPU-bound; run it only under safe, available NUMA policies.

### Why? notebook

1. Why can a misaligned typed C access be wrong even when one CPU instruction
   would tolerate the address?
2. Why can crossing a page boundary be more serious than crossing an
   eight-byte boundary?
3. Why does a struct sometimes need padding after its final member?
4. Why can smaller structs improve bandwidth yet worsen false sharing?
5. Why is `memcmp` not general struct equality?
6. Why can a packed struct still fail as a network format?
7. Why is endianness about byte significance rather than bit reversal?
8. Why must serialization define lengths and versions as well as byte order?
9. Why are memory channels and NUMA nodes not interchangeable terms?
10. Why can one socket expose multiple NUMA nodes?
11. Why can single-threaded initialization create remote accesses later?
12. Why might the first read of anonymous memory not establish the final
    private-page placement?
13. Why can a VM report a topology that is internally consistent but not
    describe the physical host?
14. Why is a node-distance value not a latency in nanoseconds?

---

## Mental model at the end

```text
layout is a contract and a performance input

type alignment + ABI
    → member offsets + padding + array stride
    → addresses and cache-line placement

serialized format
    → explicit widths + field order + byte order + bounds
    → independent of native struct layout

page allocation policy + CPU at first allocation
    → physical NUMA placement
CPU running the access
    → local or remote controller path
```

The mastery connection is now complete: **data layout determines hardware
access**. It does so through several contracts—C, ABI, serialization format,
virtual memory policy, and hardware topology—not through one magical
property of the source declaration.

---

## References used selectively

Local:

- Bryant and O'Hallaron, *Computer Systems: A Programmer's Perspective*, 3rd
  ed., §3.9.3 on data alignment and Chapter 6 on the memory hierarchy. Local
  PDF:
  `source-materials/library/books/01-foundations-and-architecture/computer-systems-programmers-perspective-bryant-ohallaron-3e-global.pdf`

Current official documentation consulted September 2026:

- Linux kernel, “NUMA Memory Policy,” including policy scope, local
  allocation, range policies, cpuset interaction, and allocation-time
  behavior:
  <https://docs.kernel.org/admin-guide/mm/numa_memory_policy.html>
- `lscpu(1)`, including CPU, cache, socket, and NUMA topology data sources:
  <https://man7.org/linux/man-pages/man1/lscpu.1.html>
- `numactl(8)`, process CPU binding and NUMA memory-policy options:
  <https://man7.org/linux/man-pages/man8/numactl.8.html>
- hwloc current command-line tools, including `lstopo` and `hwloc-bind`:
  <https://hwloc.readthedocs.io/en/master/doxygen/html/tools.html>
- Linux kernel sysfs ABI, node topology and distance interfaces:
  <https://docs.kernel.org/admin-guide/abi-stable-files.html>
- POSIX byte-order conversion interfaces:
  <https://pubs.opengroup.org/onlinepubs/9799919799/functions/htonl.html>

**Next bridge:** layout showed that C source does not fully determine machine
behavior. Tomorrow turns that observation into the C execution model:
representations, integer conversions, promotions, scope, lifetime, and the
implementation-defined choices a systems programmer must identify rather
than guess.
