# Day 6 — Hazards, Forwarding, Stalls, and Branches

**Target time:** approximately 2–3 hours

- Classify structural, data, and control hazards: 40–50 minutes
- Derive forwarding, stalls, and branch recovery: 50–60 minutes
- Run and explain the visual simulator: 40–50 minutes
- CPI, precise state, and systems evidence: 30–40 minutes

> Overlap improves throughput only if the machine prevents one instruction's
> timing from changing another instruction's architectural meaning.

## Why this day exists

Day 5 allowed independent instructions and unlimited access to every needed
resource. Real programs immediately violate those assumptions:

```text
one instruction produces a value another needs
two stages request one resource
a branch changes which instruction should have been fetched
an exception says younger work must never become visible
```

These are not rare edge cases. Dependencies carry program meaning. Loads,
branches, cache behavior, and resource conflicts shape delivered performance.

Today keeps the in-order five-stage model and adds only enough control to make
overlap correct:

- forwarding when a result exists but has not reached the register file;
- a one-cycle stall for the classic load-use case;
- flushing after a wrong branch prediction;
- valid/kill state so wrong-path instructions cannot commit;
- in-order retirement reasoning for exceptions and precise state.

There is no HDL today. The executable evidence is a dependency-aware Python
pipeline simulator.

---

## 1. A hazard is a timing threat, not necessarily a bug

A **hazard** is a condition that can prevent the next overlapped instruction
from proceeding in the intended cycle while preserving correct behavior.

Three broad classes are useful:

```text
structural hazard   required hardware resource is unavailable
data hazard         required value is unavailable at the needed time
control hazard      correct next instruction address is not yet known
```

The presence of a dependence does not prove the pipeline must stall. Hardware
may provide enough ports, forward a value, predict a branch, or otherwise
satisfy the need. A hazard exists when the chosen microarchitecture cannot
honor the desired schedule directly.

---

## 2. Structural hazards come from finite resources

Suppose IF and MEM share one single-ported memory:

```text
IF:   read next instruction
MEM:  read load data
```

If a load reaches MEM while another instruction needs IF, both request the
same port in the same cycle. One must wait.

Possible designs include:

- separate instruction and data memories or L1 caches;
- a multi-ported memory;
- arbitration plus an IF stall;
- buffering that decouples fetch from later consumption.

Every response has costs in area, power, timing, or complexity. Separate L1
instruction and data caches remove this particular port conflict, but misses
can still contend deeper in a unified cache/memory hierarchy.

Other structural limits include:

- one integer divider serving multiple operations;
- too few register-file ports;
- one load/store pipeline with several memory operations ready;
- limited issue, decode, or retirement width;
- finite queue, TLB, miss-status, or store-buffer entries.

“The processor has an ALU” is not enough information. Throughput depends on
how many relevant operations each resource can accept and complete.

---

## 3. RAW carries true data flow

Consider:

```asm
add x5, x1, x2
sub x6, x5, x3
```

The `sub` must read the value produced by the `add`. This is **read after
write (RAW)**, also called a true or flow dependence.

Without special handling, `sub` reads `x5` in ID before `add` writes it in WB:

```text
cycle      1    2    3    4    5    6
add        IF   ID   EX   MEM  WB
sub             IF   ID   EX   MEM  WB
                         needs x5
```

Reading the stale register-file value would violate the program.

### Forwarding uses the value when it exists

The `add` result exists at the end of its EX work. In the next cycle it is
held in EX/MEM while `sub` enters EX. A bypass network can select that newer
value instead of the stale ID/EX operand:

```text
EX/MEM ALU result ─┐
MEM/WB result ─────┼→ forwarding mux → EX operand input
ID/EX operand ─────┘
```

Control compares source and destination identities:

```text
if older instruction will write rd
and rd != x0
and younger EX source register == rd
then select the newest matching forwarded value
```

The newest match must win if several older instructions target the same
register.

Forwarding does not move a value backward in time. It works only after a
producer has generated the value and before a consumer needs it.

---

## 4. Why a load-use pair still stalls

Consider:

```asm
lw  x7, 0(x6)
add x8, x7, x5
```

The load computes its address in EX but receives data from memory near the end
of MEM. The following `add` needs `x7` at the beginning of its EX work in that
same cycle:

```text
cycle      1    2    3    4    5    6    7
lw         IF   ID   EX   MEM  WB
add             IF   ID   --   EX   MEM  WB
                         stall
```

The value arrives too late for ordinary EX-to-EX forwarding. The hazard unit:

1. holds PC/fetch state;
2. holds IF/ID, keeping the consumer in ID;
3. inserts a bubble into ID/EX;
4. lets the load advance;
5. forwards the load result when the consumer reaches EX one cycle later.

In logic-like notation:

```text
stall if ID/EX is a load
      and ID/EX.rd != x0
      and IF/ID actually reads ID/EX.rd
```

“Actually reads” matters. An instruction field occupying the same bit
position as `rs2` is not necessarily a source register for every opcode.
Hazard detection follows decoded semantics, not raw bit coincidence.

Some pipelines expose different load latencies or schedule assumptions. One
stall is the result for this teaching model, not a universal law for every
core.

---

## 5. WAR and WAW: real names, absent timing hazards here

Three dependence names are often memorized too broadly.

### RAW — read after write

```asm
add x5, x1, x2     # writes x5
sub x6, x5, x3     # reads x5
```

The second instruction needs a value from the first. Register renaming cannot
remove this true dependence.

### WAR — write after read

```asm
sub x6, x5, x3     # reads x5
add x5, x1, x2     # later writes x5
```

WAR is an anti-dependence: the younger write must not occur before the older
read.

### WAW — write after write

```asm
add x5, x1, x2     # older write x5
sub x5, x3, x4     # younger write x5
```

WAW is an output dependence: the final visible value must come from the
younger instruction.

### The important nuance

In this single-issue, in-order five-stage pipeline:

- instructions enter stages in order;
- register reads occur in ID in order;
- register writes occur in WB in order.

Therefore WAR and WAW names exist in the instruction stream, but their timing
cannot invert in this pipeline. They require no special stalls. RAW can still
need forwarding or a stall because a younger read naturally occurs before an
older WB.

Out-of-order execution changes the timing. Register renaming maps architectural
names to different physical destinations, removing false WAR and WAW
dependencies while preserving RAW flow.

Do not say “RISC-V has no WAR hazards.” The ISA does not choose an execution
schedule. Say: “this in-order pipeline cannot violate WAR or WAW ordering.”

---

## 6. Stalling must freeze the right state

A stall is not “stop the whole CPU.” For the load-use case:

```text
older load:       continues EX → MEM → WB
dependent in ID:  remains in ID
instruction in IF: remains in IF
EX input:         receives a bubble
```

If PC advanced while IF/ID remained held, an instruction could be skipped. If
the ID instruction advanced while its operand was stale, it could compute the
wrong result. If the load were also frozen unnecessarily, the needed value
would never approach availability.

The valid bit makes the bubble explicit:

```text
valid=1  stage contains a real instruction
valid=0  stage contains no instruction; control effects are disabled
```

Data bits in an invalid stage may contain old electrical values. Consumers
must honor validity rather than interpreting those bits.

---

## 7. Branches create control hazards

For:

```asm
beq x5, x0, target
```

the fetch unit needs a next PC before this teaching pipeline knows the branch
outcome in EX.

Possible policies include:

```text
stall until resolved      correct but wastes fetch opportunities
predict not taken         fetch sequential PC
predict taken             fetch target, if target is available
dynamic prediction       use recorded branch behavior and richer context
```

Prediction changes which work begins, not the ISA result. The machine must
detect a wrong prediction and recover.

### Predict not taken, actually taken

If the branch resolves in EX:

```text
cycle      1    2    3    4
branch     IF   ID   EX
wrong A         IF   ID    ← flush
wrong B              IF    ← flush
```

Recovery:

1. redirect fetch to the actual target;
2. invalidate younger wrong-path instructions in IF and ID;
3. retain the branch and every older instruction;
4. prevent all killed register, memory, CSR, and exception effects.

The two visible younger instructions produce a two-slot recovery cost in this
model. Deeper front ends and later resolution usually increase the amount of
work at risk.

### Prediction modes in the lab

The simulator supports:

- `not-taken` — always choose sequential fetch;
- `taken` — always choose the branch target;
- `one-bit` — remember the last resolved outcome for that branch PC,
  initialized not taken;
- `perfect` — an oracle baseline that knows the supplied outcome.

Perfect prediction is not implementable; it establishes the best-case trace.
The included branch executes once, so one-bit begins like not-taken. A loop
would reveal learning and its own first/last-iteration failures.

Current processors use substantially richer predictors. Their structures are
microarchitectural and vendor-specific. The architectural requirement remains:
wrong-path work must not alter architectural state.

### Architecture comparison — branch dependencies and memory order

The branch's architectural inputs differ. RV32I branches compare two
registers directly. AArch64 supports flag-consuming conditional branches as
well as direct register zero/bit tests; x86-64 conditional branches commonly
consume `RFLAGS` set by an earlier instruction. Hazard logic must therefore
track the dependencies defined by its **ISA**, but predictor design,
resolution stage, speculation depth, and recovery machinery remain
**microarchitectural** choices on all three.

Do not extend this lesson's in-order stage schedule into a claim about memory
ordering. x86-64 provides a relatively strong TSO-style architectural model
but still permits effects such as store buffering. AArch64 is more weakly
ordered and supplies acquire/release operations and barriers. RISC-V base
systems use RVWMO, with fences and extension-dependent atomic mechanisms such
as acquire/release bits, AMOs, and LR/SC. These are **architectural**
constraints on observable memory behavior, not statements that a pipeline
literally performs every access in source order.

Language atomics express a language-level contract; compilers select the
required ISA operations, and kernels use architecture-specific barrier APIs
and mapping rules as **software policy**. `volatile` alone does not establish
inter-core ordering. See
[Architecture Comparison for Systems Programmers](../../references/ARCHITECTURE_COMPARISON.md).

---

## 8. Exceptions connect flushing to precise state

A branch flush says:

```text
these younger instructions were fetched from the wrong path
therefore none may commit
```

An exception needs a closely related rule:

```text
older instructions may complete
the faulting instruction reports the trap
younger instructions must not commit
```

Pipeline registers therefore carry exception identity and metadata alongside
results. A fault detected early may travel until the machine can establish an
in-order architectural boundary.

Examples include:

- instruction-access fault in IF;
- illegal instruction in ID;
- arithmetic or privilege-related exception in EX;
- load/store page or access fault in MEM.

### Stores make precision especially important

A wrong-path register result can be blocked at WB. A store that updates memory
too early is harder to undo. Implementations delay or buffer stores until
their architectural legitimacy is established. Exact mechanisms vary.

### Interrupts

An interrupt is asynchronous to the instruction stream, but the architecture
still defines where it is taken and what restart state software observes.
The kernel depends on that contract to save a task, run a handler, and resume.

### Virtualization

A guest page fault, privileged operation, or external event can cause a trap
or VM exit. Hardware presents defined architectural state to the hypervisor.
Wrong-path pipeline work is not guest-visible state. Precise boundaries make
instruction emulation and reliable guest resumption possible.

---

## 9. CPI reasoning: account for empty retirement slots

For a long single-issue run:

```text
CPIideal ≈ 1
```

A first accounting model is:

```text
CPI ≈ 1
    + data-stall cycles / retired instructions
    + structural-stall cycles / retired instructions
    + control-recovery cycles / retired instructions
    + other lost cycles / retired instructions
```

This is bookkeeping, not a claim that all penalties are statistically
independent.

### Load-use contribution

If 20% of instructions are loads, 25% of those have an immediate dependent
consumer, and each such pair costs one cycle:

```text
load-use penalty/instruction = 0.20 × 0.25 × 1 = 0.05
CPI contribution             = 0.05
```

### Branch contribution

If 15% of instructions are branches, prediction accuracy is 92%, and each
misprediction costs two cycles in this pipeline:

```text
branch penalty/instruction = 0.15 × (1 - 0.92) × 2
                           = 0.024
```

With no other losses:

```text
CPI ≈ 1 + 0.05 + 0.024 = 1.074
```

For `N` retired instructions:

```text
execution time ≈ N × CPI × Tclock
```

A design change can lower CPI but lengthen the clock period. For example,
moving branch comparison earlier may reduce flush depth while adding decode
logic to a critical stage. Compare time, not CPI alone.

### Why measured CPI can be much larger

The teaching terms omit:

- instruction- and data-cache misses;
- TLB misses and page walks;
- variable-latency arithmetic;
- memory ordering and store-buffer pressure;
- front-end bandwidth and decode limits;
- interrupts, context switches, and frequency changes;
- speculation and width limits in superscalar cores.

Use the simple equation to ask where cycles went, then use architecture-
specific events and documentation to refine the answer.

---

## 10. Kernel and performance relevance

### `perf stat`

On a supported Linux system:

```bash
perf stat -e cycles,instructions,branches,branch-misses -- command
```

From the reported events, one may calculate:

```text
CPI                   = cycles / instructions
branch miss fraction  = branch-misses / branches
```

But do not multiply branch misses by this lesson's two-cycle penalty and call
it a current CPU measurement. A modern core's penalty varies with where the
branch is detected, front-end state, instruction availability, overlap, and
microarchitecture.

Also verify:

- what each event counts on this PMU;
- whether `instructions` means architecturally retired instructions;
- whether events were multiplexed;
- user/kernel/guest/host filters;
- migration and measurement noise.

### Compiler and kernel code shape

Compilers schedule independent instructions and may use conditional operations,
layout, inlining, or unrolling to alter dependencies and branch behavior.
Kernel hot paths care about unpredictable branches, dependent pointer chains,
cache misses, and serialization, but source appearance alone does not prove a
specific hardware event.

Security mitigations can intentionally constrain speculation or indirect
branch prediction. Their cost depends on CPU generation, firmware, kernel
configuration, workload, and mitigation mode; use current kernel and vendor
documentation.

---

## 11. Lab — visualize hazards and test predictions

File:

```text
05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py
```

The standard-library-only simulator tracks instruction identities through IF,
ID, EX, MEM, and WB. Rows show stage occupancy at the **start** of each cycle.
It models:

- full ALU-result forwarding;
- the classic one-cycle load-use stall;
- optional no-forwarding RAW stalls;
- optional unified-memory IF/MEM structural conflicts;
- branch resolution in EX;
- not-taken, taken, one-bit, and perfect prediction;
- wrong-path flushing and in-order retirement counts.

It does not compute register values, model caches, or represent a specific
commercial processor.

### Part A — data hazards

Before running, predict the stalls in:

```asm
add x5, x1, x2
sub x6, x5, x3
lw  x7, 0(x6)
add x8, x7, x5
sw  x8, 8(x1)
```

Run:

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py trace data
python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py trace data --no-forwarding
```

Explain:

1. why `add → sub` forwards without a stall;
2. why `lw → add` stalls once even with forwarding;
3. why no-forwarding mode waits longer;
4. why the store's data source is still a dependence.

### Part B — branches and prediction

Run all four baselines:

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py trace branch --prediction not-taken
python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py trace branch --prediction taken
python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py trace branch --prediction one-bit
python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py trace branch --prediction perfect
```

Record:

- which PCs are flushed;
- which instructions retire;
- cycles and CPI;
- why one-bit and not-taken match for a first encounter;
- why perfect prediction is evidence about an upper bound, not a design.

### Part C — structural conflict

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py trace structural
python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py trace structural --unified-memory
```

Locate every cycle where a MEM-stage load/store blocks IF.

### Part D — prediction exercises and validation

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py exercise
python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py exercise --answers
python3 05-riscv-cpu-microarchitecture/challenges/day-006-hazards-forwarding-stalls-and-branches.py selftest
```

Write the RAW/WAR/WAW classifications before revealing answers.

---

## 12. Read the simulator critically

The trace begins with an empty cycle, so do not force its total into a formula
using a different cycle-boundary convention. Compare traces generated by the
same model and explain each visible bubble.

The simulator's retirement count excludes flushed instructions. That makes a
critical point visible:

```text
fetched work can consume cycles
without becoming retired architectural work
```

Its one-bit predictor has persistent state during one simulation, but the
included branch is encountered only once. Change of predictor state matters
only if a branch PC is fetched again.

The model also assumes:

- one instruction fetched and retired at most per cycle;
- fixed instruction latency by class;
- branch target available when predicted taken;
- register-file timing compatible with its no-forwarding stall rule;
- no cache misses or exceptions in the executable scenarios.

State these assumptions when using its output as evidence.

---

## 13. Evidence: Explain, Draw, Observe, Connect

### Explain

Without notes:

1. distinguish structural, data, and control hazards;
2. classify RAW, WAR, and WAW;
3. explain why only RAW creates a timing hazard in this pipeline;
4. derive one ALU forwarding decision;
5. explain the load-use stall cycle;
6. explain prediction, detection, redirect, and flush;
7. connect valid/kill state to precise exceptions;
8. derive one CPI contribution from frequencies and penalties.

### Draw

Draw:

```text
ID/EX operand ───────┐
EX/MEM result ───────┼→ forwarding mux → ALU input
MEM/WB result ───────┘
```

Then add:

- destination/source comparisons;
- newest-match priority;
- load-use detection;
- PC and IF/ID enables;
- ID/EX bubble insertion;
- branch redirect and IF/ID kill.

### Observe

Preserve one trace for each:

- forwarded ALU dependency;
- load-use stall;
- no-forwarding RAW wait;
- unified-memory structural stall;
- branch misprediction and flush;
- correct prediction.

Annotate the exact instruction identities. “There is a bubble” is not enough;
say why it exists and which architectural error it prevents.

### Connect

Choose one systems case:

- a page fault and restart;
- a VM exit and guest resume;
- a `perf stat` branch-miss observation;
- a kernel pointer-chasing path;
- a speculation mitigation.

Identify the architectural contract, the relevant microarchitectural
mechanism, and one fact requiring current platform documentation.

---

## 14. Mastery checkpoint

Answer:

> How does an in-order five-stage processor preserve program meaning while
> instructions overlap, and where do lost cycles come from?

A complete answer includes:

- finite resources and structural arbitration;
- RAW detection and forwarding;
- one-cycle load-use interlock;
- why WAR/WAW do not reorder in this pipeline;
- prediction as speculative next-PC selection;
- redirect and younger-instruction flush;
- valid/kill control preventing wrong effects;
- in-order retirement and precise-state reasoning;
- CPI as retired work plus lost-cycle accounting.

---

## Why? notebook

1. Why is a dependence not automatically a stall?
2. Why can forwarding solve `add → sub` but not eliminate time itself?
3. Why does a load's address being ready not mean its loaded data is ready?
4. Why must hazard logic know whether an encoded field is really read?
5. Why are WAR and WAW names still useful when this pipeline cannot violate
   them?
6. Why must a load-use stall freeze IF/ID but let the load advance?
7. Why does prediction improve throughput without changing ISA semantics?
8. Why must a wrong-path store be prevented from reaching memory?
9. Why is branch accuracy alone insufficient to calculate execution time?
10. Why can `cycles / instructions` be informative but not identify the cause
    of a slowdown?
11. Why does precise state matter to page faults and VM exits?
12. Why should a current CPU's penalties come from measurement and vendor
    documentation rather than the five-stage diagram?

---

## References used selectively

- RISC-V International, *The RISC-V Instruction Set Manual, Volume I:
  Unprivileged Architecture*, current control-flow, load/store, and register
  semantics: <https://docs.riscv.org/>
- RISC-V International, *The RISC-V Instruction Set Manual, Volume II:
  Privileged Architecture*, current trap, interrupt, and virtualization
  contracts: <https://docs.riscv.org/>
- David A. Patterson and John L. Hennessy, *Computer Organization and Design:
  RISC-V Edition*, processor chapter, for the pedagogical five-stage hazard,
  forwarding, stall, and branch model.
- Linux `perf-stat(1)` manual, current event selection, filtering,
  multiplexing, and metric behavior:
  <https://man7.org/linux/man-pages/man1/perf-stat.1.html>
- Linux kernel documentation, “Hardware vulnerability and mitigation
  information,” for current kernel-visible speculation mitigations:
  <https://docs.kernel.org/admin-guide/hw-vuln/>
- Intel, *64 and IA-32 Architectures Optimization Reference Manual*, current
  dependency, branch, memory, retirement, and PMU guidance:
  <https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html>
- AMD, *Processor Programming Reference and Software Optimization Guides*,
  current family-specific pipeline and PMU guidance:
  <https://www.amd.com/en/search/documentation/hub.html>
- Arm, *Performance Monitoring Unit architecture* and Statistical Profiling
  Extension documentation, for current event and attribution contracts:
  <https://developer.arm.com/Architectures>
- Linux KVM documentation, for current guest/host architectural-state and
  virtualization interfaces: <https://docs.kernel.org/virt/kvm/>

**Next bridge:** caches and the memory hierarchy turn the fixed MEM stage into
a variable-latency system. Miss handling, TLB translation, ordering, and
memory-level parallelism extend the same resource, dependency, and retirement
questions.
