# Day 5 — Cache Organization

**Curriculum alignment:** Stage 4 — Memory Hierarchy and Performance · Cache
Organization

**Target time:** approximately 2 hours 45 minutes

- Build the address-to-cache model: 55 minutes
- Misses, writes, replacement, and coherence: 45 minutes
- Mapping, locality, false-sharing, and `perf` labs: 50 minutes
- Explain · Draw · Observe · Build · Why: 15 minutes

> Yesterday established the memory wall: nearby computation can finish while
> DRAM is still finding and returning data. A cache narrows that gap—but how
> does a small structure decide whether it already holds the byte at address
> `0x12345678`?

The answer was shaped by an engineering conflict. A fully flexible lookup
would compare an address against every cached block: fast to describe,
expensive to wire, power-hungry, and difficult to scale. A single fixed home
for each block is cheap, but unrelated addresses can evict one another.
Modern caches compromise by dividing storage into **sets** and giving each set
several candidate **ways**.

Today we make that geometry mechanical enough to calculate by hand, then
connect it to Linux performance counters and a two-thread false-sharing lab.

## Tracker objectives

By the end, you should be able to:

- **Explain cache lines, sets, and ways.**
- **Identify compulsory, capacity, and conflict misses.**
- **Understand write policies.**
- **Reason about false sharing.**

The tracker labs are all present: address-to-cache mapping, a threaded false
sharing demonstration, and `perf stat` cache-event observation. The mastery
checkpoint is exact:

> **Given a memory address and cache geometry, identify its tag, set, and
> offset.**

---

## 1. A cache transfers lines, not isolated C objects

Programs name bytes. Main memory and caches move data in blocks. In cache
terminology that block is a **cache line**. A common line size on current
x86-64 systems is 64 bytes, but this is an observed property, not a C or Linux
guarantee.

If a load needs address `A`, a 64-byte line begins at:

```text
line_base = floor(A / 64) × 64
```

and contains addresses `line_base` through `line_base + 63`.

Fetching the neighbors is a wager on spatial locality. If code reads the next
array element, those bytes may already be present. If code reads one byte from
each 4 KiB page, most bytes fetched in each line may go unused.

Inspect the host's reported geometry:

```bash
lscpu --caches
getconf LEVEL1_DCACHE_LINESIZE 2>/dev/null || true
for f in /sys/devices/system/cpu/cpu0/cache/index*/{level,type,size,coherency_line_size,ways_of_associativity,number_of_sets}; do
    printf '%s: ' "$f"; cat "$f" 2>/dev/null
done
```

Containers and virtual machines may expose incomplete, synthetic, or host-
filtered topology. Record what the interface reports; do not promote it to a
universal hardware fact.

---

## 2. Sets and ways: restricting the search

A cache is organized as:

```text
capacity = line_size × number_of_sets × ways
```

Each memory line maps to exactly one set. Within that set, it may occupy one of
the available ways.

```text
set 0: [way 0] [way 1] [way 2] [way 3]
set 1: [way 0] [way 1] [way 2] [way 3]
...
```

- **Direct-mapped:** one way per set. One possible home.
- **N-way set associative:** N candidate slots in the selected set.
- **Fully associative:** one set containing every line. Any slot is possible;
  useful as a limiting model, uncommon for large ordinary data caches.

Associativity buys placement flexibility but costs tag comparisons,
multiplexing, power, and metadata. It reduces a class of conflicts; it cannot
make a finite cache infinite.

### Tag, index, offset

For power-of-two geometry:

```text
address = [             tag | set index | line offset ]
```

- **offset** chooses a byte within a line;
- **index** chooses one set;
- **tag** distinguishes all memory lines that map to that set.

The cache stores each resident line's data plus its tag and state bits. A
lookup selects a set with the index, compares the requested tag against every
valid way in that set, and uses the matching way's bytes at the requested
offset.

The arithmetic form works even before drawing bit fields:

```text
offset      = address mod line_size
line_number = floor(address / line_size)
set         = line_number mod number_of_sets
tag         = floor(line_number / number_of_sets)
```

Ways do not change these three values. Ways say how many different tags can
reside in the selected set simultaneously.

### Worked example

Geometry:

```text
address width: 32 bits
line size:     64 B = 2^6
sets:          256  = 2^8
ways:          4
```

Therefore:

```text
offset bits = 6
index bits  = 8
tag bits    = 32 - 6 - 8 = 18
```

For `0x12345678`:

```text
offset = 0x12345678 mod 64             = 56
line   = floor(0x12345678 / 64)        = 0x0048d159
set    = 0x0048d159 mod 256            = 0x59 = 89
tag    = floor(0x0048d159 / 256)       = 0x48d1
```

Check it:

```bash
gcc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -pthread \
  02-assembly-memory-and-c/challenges/day-005-cache-lab.c -o /tmp/day5-cache
/tmp/day5-cache map 0x12345678 64 256 4
```

Now solve before running:

1. `0x0000abcd`, line 64, 64 sets, 1 way
2. `0xdeadbeef`, line 64, 512 sets, 8 ways
3. two addresses separated by `sets × line_size`: what fields match?

That third spacing is a **cache-sized stride per way**: the offset and index
repeat while the tag changes. Enough such addresses can contend for one set.

---

## 3. Hit, fill, eviction

For a read:

```text
extract index → read selected set's tags
              → tag matches a valid way?
                  yes: hit; select bytes by offset
                  no:  miss; fetch line from lower level
                       choose/fill a way
                       update tag and state
                       retry or satisfy waiting load
```

Real cores overlap, pipeline, speculate, prefetch, and support multiple
outstanding misses. This diagram is an architectural teaching sequence, not a
cycle-accurate description.

The lower level may be L2, a shared last-level cache, another core's cache
through coherence, or DRAM. “Cache miss” does not mean “go directly to RAM.”

### Three useful miss categories

The classic 3C model classifies misses relative to a cache model:

1. **Compulsory (cold):** this line has not yet been brought into the cache.
2. **Capacity:** the active collection of lines exceeds cache capacity; even a
   fully associative cache of that size would evict something needed later.
3. **Conflict:** the cache has enough total room, but restricted placement
   forces lines mapping to the same set to evict each other.

Examples:

- First pass over a new array: mostly compulsory demand misses, though
  hardware prefetch may obscure them in counters.
- Repeated pass over data far larger than the cache: capacity pressure.
- Alternating among five same-set lines in a four-way cache while the rest is
  empty: conflict pressure.

These categories explain a model; production measurements rarely label each
event cleanly. Prefetching, inclusive/exclusive policies, victim structures,
TLB misses, coherence, and undocumented replacement details blur the simple
story.

---

## 4. Replacement: which resident line loses?

On a miss to a full set, one way must be replaced.

- **LRU** evicts the least recently used line. Exact LRU metadata becomes
  expensive as associativity grows.
- **Pseudo-LRU** approximates recency with less state.
- **Random** is simple and avoids some adversarial patterns.
- Practical policies may consider insertion position, reuse prediction,
  prefetch origin, or implementation-specific heuristics.

Do not assume the CPU uses exact LRU because a textbook exercise says “evict
the oldest.” The exercise needs a stated policy; the actual processor needs
its vendor documentation or measurement, and even then details may be
undocumented.

Kernel page replacement is a different layer. The kernel decides which
physical pages remain resident; CPU cache replacement decides which cache
lines remain in a hardware cache. Similar vocabulary does not make them the
same mechanism.

---

## 5. Writes create two separate policy questions

### Hit policy

- **Write-through:** update the cache and propagate the write toward the next
  level immediately, usually buffered.
- **Write-back:** update the cached line and mark it dirty; write it to the
  next level when the line is evicted or otherwise cleaned.

Write-back reduces lower-level traffic when the same line is updated
repeatedly, but requires dirty-state tracking and correct coherence.

### Miss policy

- **Write-allocate:** fetch the line into the cache, then update it.
- **No-write-allocate (write-around):** send the write downward without
  filling this cache.

Common write-back caches use write-allocate, while write-through designs may
pair with no-write-allocate. These are combinations, not definitions. Also
distinguish ordinary cached memory from uncached/device mappings, where the
architecture and kernel choose different memory types and ordering rules.

---

## 6. Coherence and false sharing

Multiple cores may hold copies of one physical line. A coherence protocol
maintains a permitted view of those copies. To write, a core generally needs
exclusive ownership; other cached copies must be invalidated or downgraded.

Now place two independent counters in one line:

```text
64-byte cache line
┌───────────────────────┬───────────────────────┬─────────────┐
│ counter A (thread 0)  │ counter B (thread 1)  │ unused      │
└───────────────────────┴───────────────────────┴─────────────┘
```

The threads do not share a C object, but coherence works at line granularity.
Each write can transfer ownership of the whole line. That is **false
sharing**: logically independent writable data shares a coherence unit.

It is “false” only at the source-level sharing model; the hardware traffic is
real. Padding or separating hot fields may help, but consumes memory and can
harm locality elsewhere. Measure the actual workload.

Run the safe C11-atomic experiment:

```bash
/tmp/day5-cache false-sharing 20000000
taskset -c 0,1 /tmp/day5-cache false-sharing 20000000  # if two CPUs allowed
```

The atomics prevent a C data race; each thread owns a different atomic
object. The compact and padded layouts change line placement, not language
correctness.

On a one-vCPU VM, a container with restricted affinity, or sibling SMT
threads with unusual topology, the difference may be small or noisy. Repeat,
record CPU placement, and do not “fix” results to match the lesson.

Linux itself fights false sharing in hot shared structures with field
reorganization, read-mostly grouping, per-CPU data, and batched updates.
Those techniques trade coherence traffic against memory, complexity, and
eventual aggregation cost.

---

## 7. Observe locality and counters

First run a software timing experiment:

```bash
/tmp/day5-cache locality 64 8
```

Predict before running:

- Why can a 1-byte stride have a different cost per *visited element* from a
  64-byte stride?
- Why does a 4096-byte stride add TLB/page effects to the cache story?
- Why is elapsed time alone unable to identify one particular miss type?

Then ask `perf` which generic events it can expose:

```bash
perf list cache
perf stat -r 5 -e cycles,instructions,cache-references,cache-misses \
  /tmp/day5-cache locality 64 8
perf stat -r 5 -e cycles,instructions,cache-references,cache-misses \
  /tmp/day5-cache false-sharing 20000000
```

### `perf` caveats are part of the lab

Generic `cache-misses` is not a portable synonym for “all L1 data misses.”
The kernel maps generic events to whatever the PMU supports. Events may be
unsupported, multiplexed, counted speculatively, affected by skid, or denied
by `perf_event_paranoid`. A hypervisor may hide the PMU entirely.

If `perf` reports `<not supported>` or permission denial:

1. keep the failure text as evidence;
2. run the timing and mapping modes;
3. inspect `perf list` and `/proc/sys/kernel/perf_event_paranoid`;
4. do not use `sudo` merely to force the course lab to work;
5. state that the environment cannot supply this evidence.

For serious false-sharing diagnosis on supported hardware, Linux provides
`perf c2c`, which can attribute cache-to-cache activity and contended line
offsets. It is more informative than comparing two wall-clock numbers, but it
also depends on suitable PMU support and permissions.

---

## 8. CPU, Linux, kernel, and VM connections

```text
C field/array layout
        ↓ determines addresses touched
virtual addresses
        ↓ translated by page tables and TLB
physical addresses
        ↓ select cache set/tag (exact stage is implementation-specific)
coherence domain
        ↓ coordinates line ownership among CPUs
memory controller/channel
        ↓ serves lower-level misses
Linux scheduler
        ↓ chooses where threads run
hypervisor
        ↓ may virtualize topology, time, and PMU access
```

Do not collapse cache indexing and virtual-memory translation into one
universal formula. L1 caches are often virtually indexed and physically
tagged; larger caches commonly use physical addressing, but exact designs are
microarchitectural.

The scheduler can migrate a thread, changing which private caches are warm.
Page allocation changes physical placement. Kernel per-CPU data avoids
cross-core writes. A VM can make measured CPU time and reported topology less
direct than bare metal. Cache behavior lives at the meeting point of all
these layers.

---

## 9. Mastery evidence

### Explain

Without notes:

- explain line, set, way, tag, index, and offset;
- distinguish direct-mapped, set-associative, and fully associative designs;
- distinguish compulsory, capacity, and conflict misses;
- separate write-through/write-back from write-allocate/no-write-allocate;
- explain false sharing without saying the two threads modify the same object;
- explain why `perf stat` output is evidence with scope, not ground truth.

### Draw

Draw a four-set, two-way cache. Place memory lines 0, 4, and 8 into it. Label
the common set, different tags, valid/dirty bits, and the replacement choice.
Then draw two cores repeatedly acquiring one line that contains two counters.

### Observe

Record:

- host-reported line size, set count, and associativity where available;
- one hand mapping checked by the challenge;
- repeated compact versus padded timings;
- `perf stat` output **or** the exact reason counters were unavailable;
- whether you were on bare metal, container, or VM as far as you can establish.

### Build

1. Extend mapping mode to print binary fields for power-of-two geometry.
2. Generate five addresses with one offset/index and five tags.
3. Change padding from 64 to 128 bytes and predict whether correctness or only
   placement changes.
4. Add a single-thread mode to separate atomic instruction cost from
   inter-core coherence cost.

### Why? notebook

1. Why fetch a whole line when the instruction asks for four bytes?
2. Why does associativity reduce conflict misses but not capacity misses?
3. Why is a tag required after the set has already been selected?
4. Why can two different addresses have the same index and offset?
5. Why can exact LRU become expensive?
6. Why are write-back and write-allocate separate decisions?
7. Why is false sharing a coherence problem rather than a C aliasing problem?
8. Why might padding make one workload faster and another worse?
9. Why can `cache-misses` differ across machines for the same binary?
10. Why can a VM hide the effect the source code was designed to expose?

---

## Mental model at the end

```text
address
  ├─ low bits choose byte offset
  ├─ next bits choose one set
  └─ remaining tag is compared across that set's ways
         ├─ match → hit
         └─ no match → miss, fill, perhaps evict/write back

multiple cores writing different words in one line
  → repeated ownership transfer
  → false sharing
```

---

## References used selectively

Local:

- Bryant and O'Hallaron, *Computer Systems: A Programmer's Perspective*, 3rd
  ed., Chapter 6, especially §§6.3–6.6 on locality, cache memories, cache
  parameters, reads/writes, and performance. Local PDF:
  `references/library/books/01-foundations-and-architecture/computer-systems-programmers-perspective-bryant-ohallaron-3e-global.pdf`

Current official documentation consulted September 2026:

- Linux kernel, “False Sharing,” including `perf c2c`, `pahole`, mitigation
  tradeoffs, and kernel examples:
  <https://docs.kernel.org/kernel-hacking/false-sharing.html>
- `perf-stat(1)`, event selection, repetition, aggregation, and PMU caveats:
  <https://man7.org/linux/man-pages/man1/perf-stat.1.html>
- `lscpu(1)`, including cache and topology data sources:
  <https://man7.org/linux/man-pages/man1/lscpu.1.html>

**Next bridge:** cache lines already revealed that address and layout matter.
Tomorrow asks who decides where an object begins, how bytes are ordered,
which memory controller serves a physical page, and why “the RAM” may be
nearer to one CPU than another.
