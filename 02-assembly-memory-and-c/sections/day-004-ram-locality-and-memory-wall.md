# Day 4 — RAM, Locality, and the Memory Wall

**Target time:** approximately 2–3 hours

- First-principles lesson and predictions: 70–85 minutes
- Fedora benchmark and plot: 45–60 minutes
- Evidence, explanation, and review: 30–40 minutes

> A CPU can finish an arithmetic operation in a handful of cycles. What happens
> when the operand has not arrived yet?

That question created much of the modern memory system. Today's path is:

```text
one ideal memory
    → incompatible speed, capacity, cost, and persistence goals
    → registers + caches + DRAM + storage
    → locality makes the hierarchy useful
    → experiments reveal where the illusion becomes expensive
```

This lesson connects the CPU datapath from earlier lessons to Linux virtual
memory and, eventually, NUMA. It deliberately stops short of cache indexing,
replacement, and coherence; those belong to the next lesson.

---

## 1. Why not build one enormous, instant memory?

A useful computer wants memory to be all of these:

- as fast as the CPU;
- large enough for every active program and file;
- cheap enough to manufacture;
- low-power;
- persistent after power loss.

No current technology gives all of those properties at once. Fast storage
usually spends more area and energy per bit. Dense storage usually takes
longer to access. Persistent storage has different physical constraints again.
Distance also matters: a signal must travel through wires, controllers, and
devices; large structures take time to select and drive.

The resulting hierarchy is not arbitrary decoration:

```text
                  smaller, faster, costlier per byte
CPU registers                 tens to hundreds of bytes per core
L1 / L2 / L3 caches           KiB to many MiB
DRAM                           GiB
SSD / other storage           hundreds of GiB to TiB
                  larger, slower, cheaper per byte
```

The exact sizes and timings are machine-specific. Even the diagram is an
abstraction: some caches are private, some shared; storage is reached through
I/O rather than ordinary CPU loads; and virtual memory may make storage back
some pages. The durable idea is that each level tries to make the next, slower
level less visible.

### The historical problem path

Early machines repeatedly changed storage technology because each design hit a
different limit. Registers and flip-flop-like storage were fast but expensive.
Delay lines, magnetic drums, and magnetic core offered useful capacity with
different access constraints. Semiconductor memory made random-access main
memory practical at scale. As processors became faster through integration,
pipelining, and parallel execution, DRAM latency improved much more slowly in
processor-cycle terms.

Caches were an engineering response: keep recently or nearby used data in a
small fast structure. Multiple cache levels appeared because one cache could
not simultaneously be tiny enough for the fastest access and large enough to
catch broad working sets. This is the historical core of the **memory wall**:
increasing CPU execution capability does not ensure proportional application
speedup when data delivery is the limiting path.

---

## 2. Where a loaded byte actually travels

For a normal userspace load whose address is already translated:

```text
instruction
  → CPU load machinery
  → search cache hierarchy
  → on a last-level miss, memory controller
  → DRAM channel/rank/bank/row
  → cache-line-sized transfer back toward the core
  → requested bytes delivered to a register
```

That is a conceptual path, not a universal timing diagram. Translation may
need the TLB or a page walk. Hardware prefetchers may request lines before the
load. Several misses may overlap. Dirty evictions can create more traffic.

Linux adds another layer of meaning. `malloc` normally reserves virtual
address space; physical pages are commonly supplied on first touch. The kernel
chooses physical pages, handles faults, may reclaim or migrate pages, and on a
NUMA host applies placement policy. Once mappings exist, ordinary cacheable
loads do not trap into the kernel. Hardware translates and accesses them.

On a NUMA machine, the memory controller nearest one CPU package may reach
some DRAM with lower cost than remote DRAM attached to another package.
Linux exposes topology and placement controls (`lscpu`, `numactl --hardware`,
`numactl` policies), but NUMA measurement is a later focused lab. For today,
remember that “DRAM latency” is not one context-free number.

---

## 3. SRAM and DRAM at the right depth

### SRAM: retain state while powered

A typical SRAM bit cell uses a small bistable transistor network plus access
transistors. Its state can be read without periodically refreshing every bit.
It is fast, but the cell and supporting circuitry consume substantial silicon
area per bit. SRAM is therefore a natural fit for caches and similar small,
fast on-chip arrays.

“Static” does **not** mean persistent: remove power and the state is lost. It
also does not mean no control circuitry or no energy use.

### DRAM: store charge densely

A conceptual DRAM cell stores a bit as charge in a tiny capacitor selected by
a transistor. Charge leaks, so rows must be refreshed. Reading involves sense
amplifiers and is effectively destructive at the cell level, requiring the
value to be restored. DRAM accesses involve opening a row, operating on
columns, and sometimes closing/precharging before another row.

The cell is dense, making large main memories economical, but access has more
latency and management overhead than SRAM. Real DRAM organizations include
channels, ranks, banks, rows, bursts, queues, and scheduling. We do not need
their electrical timings today to understand why access order can matter.

| Question | SRAM | DRAM |
|---|---|---|
| Typical role | CPU caches | Main memory |
| Conceptual storage | bistable circuit state | charge sensed and restored |
| Refresh | no periodic per-bit refresh | required |
| Density/cost per bit | lower density, higher cost | higher density, lower cost |
| Relative access | generally faster | generally slower |
| Persistent without power | no | no |

These are architectural tendencies, not promises that every product called
SRAM or DRAM has one fixed latency.

---

## 4. Latency and bandwidth are different questions

**Latency** asks how long one operation takes from request to useful result.

**Bandwidth** asks how much data can be transferred per unit time once work is
flowing.

A delivery truck has high latency for one package but can have high aggregate
bandwidth when full. Memory behaves similarly: a dependent pointer chase must
wait for each address before issuing the next useful load, exposing latency.
A sequential scan presents predictable, independent work; prefetching, wide
transfers, cache lines, multiple outstanding misses, and vectorization can
raise throughput.

Do not convert one into the other carelessly. `ns/access` from a dependency
chain is a latency-oriented observation. `GB/s` from a large sequential scan
is throughput-oriented. A cache line fetch also transfers neighboring bytes,
so “one C load” is not necessarily “one DRAM transaction.”

---

## 5. Locality is the reason caching usually works

Programs do not normally choose each next byte uniformly from all available
memory.

**Temporal locality** means recently used data is likely to be used again:

```c
for (...) {
    total += frequently_used_counter;
}
```

Keeping the relevant cache line nearby helps repeated access.

**Spatial locality** means an address near a recently used address is likely
to be used soon:

```c
for (size_t i = 0; i < n; ++i) {
    total += array[i];
}
```

Memory moves data in blocks—typically cache lines between DRAM and caches—so
one miss can bring neighboring elements needed by later iterations.

A row-major C matrix makes the consequence concrete. For:

```c
int a[rows][cols];
```

`a[r][0]`, `a[r][1]`, and `a[r][2]` are adjacent. Iterating columns in the
inner loop follows layout; iterating rows in the inner loop jumps by an entire
row. Both compute the same mathematical set of elements, but they present very
different address streams.

### Sequential versus random is not magic vocabulary

Sequential access tends to:

- use most bytes fetched in each cache line;
- be predictable to hardware prefetchers;
- permit several loads to overlap;
- make vectorization easier.

Random access tends to waste neighboring fetched bytes and defeat simple
prefetching. A **dependent** random chain additionally prevents the next
address from being known until the previous load completes.

But random does not automatically mean slow. A small random working set can
remain in cache. Conversely, a sequential stream larger than cache still
consumes memory bandwidth. Size, reuse, dependency, layout, and concurrency
all matter.

---

## 6. Working sets and visible boundaries

A program's **working set** is the data it actively needs during an interval.
As that set grows, it may cease to fit in a nearby cache level. More accesses
then reach farther levels.

One might expect a perfect staircase at L1, L2, L3, and DRAM capacities.
Measurements are rarely that clean:

- caches are shared with code, page tables, and other data;
- set mapping and associativity affect usable capacity;
- hardware prefetching changes sequential behavior;
- replacement is dynamic;
- the OS and hypervisor interrupt or deschedule the process;
- frequency, power, and thermal control change CPU timing;
- first touch causes page faults;
- transparent huge pages and page placement may differ;
- a virtual machine adds host scheduling and a virtualized topology.

Look for ranges, slopes, and repeatable transitions—not an invented claim that
one point is “the exact L2 latency.”

---

## 7. Lab: observe the wall without fooling yourself

The lab has two kernels:

1. **Sequential scan** sums every `uint64_t` in order. It reports elapsed
   nanoseconds per C-level access and is throughput-friendly.
2. **Random pointer chase** follows a shuffled cycle. Each next index depends
   on the preceding load, making it latency-sensitive.

They answer related but different questions. Their ratio is **not** a pure
measurement of “random versus sequential DRAM.” The instruction streams,
dependency structure, available memory-level parallelism, and extra index
array differ.

### Compile warning-clean on Fedora

From the repository root:

```bash
gcc -std=c17 -O2 -Wall -Wextra -Wpedantic -Wconversion \
  02-assembly-memory-and-c/challenges/day-004-memory-wall-lab.c \
  -o /tmp/day-004-memory-wall-lab
```

`-O2` is necessary for a representative optimized loop. The program consumes
results through a volatile sink and output, so the compiler cannot simply
delete the measured work. This does not freeze every optimization; inspect
assembly if you need to characterize exactly what GCC generated.

Run with a 64 MiB maximum working set (or choose another safe limit):

```bash
/tmp/day-004-memory-wall-lab 64 | tee /tmp/day-004-memory-wall.csv
```

The program allocates roughly three times the requested maximum because it
keeps data, permutation, and link arrays. Do not request a size that pressures
the host into swapping. Close heavy applications if appropriate.

Plot:

```bash
python3 02-assembly-memory-and-c/challenges/day-004-plot.py \
  /tmp/day-004-memory-wall.csv
```

If Matplotlib is absent, the script prints the Fedora package to install.
Installing it is optional; the CSV itself is valid evidence, and a spreadsheet
can plot `bytes` against `ns_per_access`.

### Predict before running

Write down:

1. At what working-set ranges do you expect changes?
2. Which curve should rise more visibly as the set exceeds caches?
3. Why might the sequential curve remain comparatively flat?
4. What result would make you rerun rather than tell a dramatic story?

### Run protocol

1. Record `lscpu` and whether this is bare metal or a VM.
2. Run the complete benchmark at least three times.
3. Do not average first. Plot or compare the runs and identify variability.
4. Repeat once while the system is busy, if safe, to see environmental noise.
5. Record compiler version and command.
6. Optionally inspect generated code with:

   ```bash
   objdump -d -Mintel /tmp/day-004-memory-wall-lab | less
   ```

The code performs a warm-up, but warm-up does not make a benchmark pure.
Initial page faults are reduced because initialization touches memory; cache
state still evolves, and larger sets evict earlier data. `CLOCK_MONOTONIC`
measures wall time, including descheduling. That is honest for a simple lab,
provided we report the noise.

### Interpret carefully

For each run, ask:

- Are transitions in similar size ranges across repeats?
- Does random-chase cost increase as the working set grows?
- Does sequential behavior look more throughput-efficient?
- Are there isolated spikes likely caused by scheduling?
- Is the largest set actually below, near, or above the host's last-level
  cache?
- Could a VM be hiding or sharing the physical cache topology?

You may say, “The repeatable increase between these ranges is consistent with
more accesses being served by a farther hierarchy level.” Do not say, “This
point proves my L3 is exactly X MiB and DRAM is exactly Y ns” from this lab
alone.

For deeper work, CPU performance counters can add evidence, but event names,
availability, privilege policy, multiplexing, and interpretation are
processor-specific. `perf stat` is not a universal truth machine.

---

## 8. Optimization lessons that survive the benchmark

Useful optimization is about reducing expensive data movement or hiding it,
not merely changing syntax.

1. **Lay out data for the traversal.** Contiguous arrays often beat scattered
   objects when the operation naturally visits everything.
2. **Reuse while data is nearby.** Loop tiling/blocking reorganizes work so a
   cache-sized tile receives several operations before eviction.
3. **Avoid needless footprint.** Smaller representations can fit more useful
   objects in each cache and cache line.
4. **Separate hot and cold fields.** Frequently used data need not drag rare
   metadata through caches.
5. **Allow parallel requests where semantics permit.** Independent loads can
   overlap; dependent pointer chains cannot.
6. **Measure the real workload.** A synthetic benchmark diagnoses mechanisms,
   not end-to-end application value.

Optimization can shift cost elsewhere. Compression reduces bytes but spends
compute. Copying into a contiguous buffer helps repeated traversal but costs a
copy. Prefetching too early evicts useful data; too late does not hide latency.
NUMA placement that helps one thread can hurt another.

---

## 9. CPU, controller, Linux, and NUMA bridge

An application says `sum += a[i]`. Several layers cooperate:

```text
C object and loop
  → optimized machine loads
  → virtual address translation
  → cache lookup and hardware prefetch policy
  → memory-controller queues and DRAM commands on misses
  → physical page chosen under Linux policy
  → possibly local or remote NUMA memory
```

This bridge prevents two common errors:

- “Linux handles every memory access.” It establishes and manages mappings;
  hardware normally services mapped loads without entering the kernel.
- “The CPU directly asks a DRAM chip for one variable.” Caches, line transfers,
  translation, controllers, queues, and DRAM organization intervene.

Later modules will sharpen cache geometry, page allocation, virtual memory,
and NUMA policy. Today's model is enough to explain why a faster execution
engine can spend cycles waiting for its data path.

---

## 10. Evidence: Explain, Draw, Observe, Build

### Explain

Without notes:

1. Why can no single practical memory level satisfy speed, capacity, cost, and
   persistence simultaneously?
2. Why is SRAM used for caches and DRAM for main memory?
3. Distinguish latency from bandwidth with one program example.
4. Define temporal and spatial locality without using the word “cache.”
5. Why can a sequential scan outperform a dependent random traversal?
6. Why can a faster CPU still wait on memory?

### Draw

Draw one complete load path:

```text
register ← caches ← memory controller ← DRAM
                   ↑
virtual address → translation → physical page / NUMA node
```

Add storage below DRAM and explain why reaching it is not an ordinary cache
miss. Mark where Linux policy acts and where hardware normally acts.

### Observe

Submit:

- three raw CSV runs;
- one plot or a hand-drawn graph from the CSV;
- host/VM, compiler, command, and maximum size;
- two repeatable observations;
- one noisy or ambiguous observation;
- one claim you deliberately refused to make.

### Build

The C benchmark and plot are the build evidence. Then change one variable:
maximum size, system load, compiler optimization level, or CPU affinity.
Predict first, rerun, and explain why the result changed—or why it did not.

---

## 11. Mastery checkpoint

Close the lesson and answer:

> Why may a CPU with a faster arithmetic pipeline fail to make a
> memory-intensive program proportionally faster?

A complete answer includes:

- the mismatch among register/cache, DRAM, and storage properties;
- latency versus bandwidth;
- cache lines and locality;
- working-set size;
- dependent versus independent accesses;
- the controller/DRAM path on cache misses;
- Linux page placement and the possibility of NUMA distance;
- why one noisy microbenchmark cannot establish universal hardware constants.

Then make two predictions:

1. a small randomly traversed array versus a huge one;
2. a huge sequential array versus a huge dependent random chain.

If the explanation is only “RAM is slow,” it is not yet complete.

---

## Why? notebook

1. Why does greater DRAM density not automatically make it cache-speed?
2. Why does fetching a cache line reward spatial locality?
3. Why does reuse reward temporal locality?
4. Why can bandwidth be high while dependent-load latency remains painful?
5. Why might a cache-capacity boundary appear as a broad slope?
6. Why does first touch matter on Linux?
7. Why can VM measurements differ between runs?
8. Why is pointer chasing useful for exposing latency but unlike many real
   application loops?
9. Why might reducing a structure's size improve performance without reducing
   the number of source-level accesses?
10. Where can NUMA enter the path, even though the C pointer looks unchanged?

---

## Mental model at the end

```text
The CPU requests an address, not “a variable from RAM.”
Nearby hierarchy levels are tried first.
Locality makes small fast levels effective.
Misses travel through controllers to denser, slower memory.
Sequential independent work can use bandwidth and overlap.
Dependent random work exposes more latency.
Linux controls mappings and placement; hardware handles ordinary mapped loads.
Measurements are evidence conditioned on code, machine, OS, and environment.
```

---

## References used selectively

- John L. Hennessy and David A. Patterson, *Computer Architecture: A
  Quantitative Approach*, memory-hierarchy chapters, for locality, latency,
  bandwidth, and the quantitative hierarchy model.
- Ulrich Drepper, “What Every Programmer Should Know About Memory,” 2007,
  Sections 2–3. Useful for foundational SRAM/DRAM and cache concepts; specific
  hardware examples are historical:
  <https://people.freebsd.org/~lstewart/articles/cpumemory.pdf>
- Wulf and McKee, “Hitting the Memory Wall: Implications of the Obvious,”
  *ACM SIGARCH Computer Architecture News* 23(1), 1995:
  <https://doi.org/10.1145/216585.216588>
- Linux kernel documentation, “Memory Management”:
  <https://docs.kernel.org/mm/index.html>
- Linux kernel documentation, “NUMA Memory Policy”:
  <https://docs.kernel.org/admin-guide/mm/numa_memory_policy.html>
- `clock_gettime(3)` and `perf-stat(1)` Fedora/Linux manual pages for the
  measurement interfaces and their limits.

The benchmark is a teaching instrument synthesized for this course. It is not
a hardware certification tool, and its observations must be reported with the
compiler, host, topology, and run conditions.
