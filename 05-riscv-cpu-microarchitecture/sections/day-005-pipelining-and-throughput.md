# Day 5 — Pipelining and Throughput

**Target time:** approximately 2–3 hours

- Derive a pipeline from a critical path: 45–55 minutes
- Trace the in-order five-stage model: 45–55 minutes
- Run the timing and visual-trace lab: 35–45 minutes
- Architectural-state, systems, and mastery work: 30–40 minutes

> If one instruction still needs fetch, decode, execute, memory, and
> write-back, how can a processor finish nearly one instruction every cycle?

## Why this day exists

A single-cycle processor places all work for one instruction between two
architectural state updates. Its clock must wait for the slowest instruction's
longest combinational path. Much of the datapath is idle during parts of that
long interval.

Pipelining changes the schedule, not the ISA:

```text
long register-to-register path
    → split the combinational work
    → insert clocked boundaries
    → overlap different instructions
    → improve potential throughput
```

It does not make one instruction's work disappear. It adds state, clock
overhead, startup time, and control obligations. Today derives those costs
before introducing hazards on Day 6.

By the end, you should be able to:

- derive a clock period from stage delays and register overhead;
- distinguish instruction latency from throughput;
- draw the IF/ID, ID/EX, EX/MEM, and MEM/WB boundaries;
- calculate pipeline fill and drain time;
- trace independent instructions through an in-order five-stage pipeline;
- explain why pipeline depth has diminishing returns;
- distinguish architectural state from speculative or intermediate state;
- connect retirement to precise state, performance counters, kernels, and
  virtualization.

---

## 1. Begin with the unsplit critical path

Consider a teaching RV64 processor that completes an instruction in one long
cycle:

```text
PC register
  → instruction memory
  → decode and register-file read
  → ALU / branch compare
  → data memory for a load
  → write-back selection
  → architectural register file
```

Not every instruction uses every block. An `add` does not need data memory,
and a store does not write an integer register. The shared clock nevertheless
must accommodate the longest valid path used by any supported operation.

Use this illustrative delay budget:

```text
instruction fetch                    180 ps
decode and register read             220 ps
execute / address / branch work      250 ps
data-memory access                   210 ps
write-back selection                 140 ps
total combinational work            1000 ps
launch/capture and clock overhead      60 ps
```

For the simplified single-cycle machine:

```text
Tsingle ≥ 1000 ps + 60 ps = 1060 ps
```

Its ideal instruction rate is one instruction per 1060 ps. The exact numbers
are a teaching model, not measurements of a current core.

### Why not simply shorten the clock?

The capture edge would arrive before the worst required result became stable.
That is a timing failure, not a harmless request for more performance.
Software cannot repair a late internal signal.

---

## 2. Split the path and pay for every boundary

Insert registers around five groups of work:

```text
        IF          ID          EX          MEM         WB
PC → fetch → |R| → decode → |R| → ALU → |R| → memory → |R| → select/write
              IF/ID       ID/EX       EX/MEM       MEM/WB
```

Each stage now has one cycle to perform its local combinational work. The
period is bounded by the slowest stage, not by the sum:

```text
Tpipeline ≥ max(tIF, tID, tEX, tMEM, tWB) + treg
```

For the numbers above:

```text
Tpipeline ≥ max(180, 220, 250, 210, 140) + 60
          = 310 ps
```

The register overhead includes a simplified allowance for clock-to-Q, setup,
and clock uncertainty. Real timing closure checks individual paths and clock
relationships rather than adding one universal constant.

### The split is never perfectly free or balanced

The ideal five-way split of 1000 ps would put 200 ps in every stage. Our EX
stage needs 250 ps, so 50 ps of capacity in another stage cannot automatically
help it. Logic dependencies, memory interfaces, wiring, fan-out, and physical
placement constrain where boundaries can go.

Every added stage also costs:

- storage bits and clock-tree load;
- clock-to-Q and setup time;
- control and valid/kill metadata;
- energy on every active cycle;
- additional instruction latency;
- more in-flight work to recover after a redirect or exception.

If useful logic per stage becomes comparable to register overhead, further
splitting offers little frequency gain.

---

## 3. Latency and throughput answer different questions

**Latency** asks how long one instruction takes from entry to completion.
**Throughput** asks how frequently completed instructions can emerge.

For an ideal five-stage pipeline with a 310 ps clock:

```text
latency in cycles = 5 cycles
latency in time   = 5 × 310 ps = 1550 ps

steady-state throughput = 1 instruction / 310 ps
```

The pipelined instruction has *higher* time latency than the 1060 ps
single-cycle instruction in this example. Throughput improves because several
instructions occupy different stages concurrently.

This is the same distinction seen in systems:

- a storage device may have high single-request latency but high queue
  throughput;
- a network path may have long round-trip time but sustain many bytes per
  second;
- a CPU may overlap many instructions while one dependent chain remains
  latency-bound.

### A dependency chain sees latency

If each operation needs the previous operation's result, overlap is limited by
when that result becomes available. Independent work can exploit throughput;
a serial recurrence cannot simply claim the advertised issue width or clock
rate.

---

## 4. Fill, steady state, and drain

Trace six independent instructions:

```text
        C1  C2  C3  C4  C5  C6  C7  C8  C9  C10
I1      IF  ID  EX  MEM WB
I2          IF  ID  EX  MEM WB
I3              IF  ID  EX  MEM WB
I4                  IF  ID  EX  MEM WB
I5                      IF  ID  EX  MEM WB
I6                          IF  ID  EX  MEM WB
```

The first result appears only after the pipeline fills. The last instruction
must pass through the remaining stages while the pipeline drains.

For `k` stages and `N ≥ 1` independent instructions:

```text
ideal cycles = k + N - 1
```

For six instructions and five stages:

```text
cycles = 5 + 6 - 1 = 10
```

Average CPI for this finite group is:

```text
CPI = cycles / instructions = 10 / 6 ≈ 1.67
```

As `N` becomes large, ideal CPI approaches 1. It does not equal 1 for a short
cold sequence, and Day 6 will add bubbles that keep real CPI above the ideal.

### Batch speedup

For 100 independent instructions:

```text
single-cycle time = 100 × 1060 ps = 106000 ps
pipelined time    = (5 + 100 - 1) × 310 ps
                  = 32240 ps
speedup           ≈ 3.29×
```

The speedup is not 5× because:

1. the stages are uneven;
2. register overhead is paid in every pipeline cycle;
3. the finite sequence fills and drains.

---

## 5. What each stage means

The names describe useful responsibilities, not a universal physical law.

### IF — instruction fetch

Conceptually:

```text
instruction bits ← memory/cache at PC
candidate next PC ← PC + instruction length
```

The teaching model assumes fixed-width instructions. RISC-V implementations
supporting compressed instructions must account for variable 16-bit and
32-bit instruction lengths. Current high-performance cores may fetch blocks,
predict multiple control transfers, and queue decoded operations. The
five-stage name remains a reasoning scaffold.

### ID — decode and register read

ID identifies operation fields, reads source registers, forms immediate
values, and carries control metadata forward:

```text
instruction bits
    → operation, rs1, rs2, rd, immediate, control
```

### EX — execute and address generation

EX performs integer ALU work, comparisons, effective-address generation, and,
in this model, branch resolution.

### MEM — data-memory access

Loads read and stores present an address/data operation. ALU instructions
still occupy the stage even if they merely carry a result through it.

### WB — write-back / teaching retirement point

An integer result may update `rd`. Stores and branches do not write an integer
register but still complete in program order. We use WB as a simple retirement
boundary so the architectural story remains explicit.

Real machines do not have to use these exact boundaries. Some combine stages,
split fetch or memory access, use multiple execution pipelines, or retire via
a reorder buffer. ISA conformance constrains visible behavior, not these five
labels.

### Architecture comparison — pipeline shape is implementation freedom

RISC-V, AArch64, and x86-64 define architectural instruction effects and
exception/ordering contracts; none defines an IF/ID/EX/MEM/WB pipeline. A
small implementation of any of them may be in-order, while a high-performance
implementation may be deep, wide, speculative, and out of order.

Their **ISA** front ends begin with different work: base RISC-V and A64 have
regular 32-bit encodings, optional RISC-V compressed instructions add
16-bit lengths, and x86-64 has variable-length instructions that are commonly
translated to internal micro-operations. After decode, all three permit
substantial **microarchitectural** freedom: internal operations, queues,
physical-register renaming, execution ports, and retirement structures are
not software-visible. The obligation is precise architectural behavior, not
similar pipeline diagrams. See
[Architecture Comparison for Systems Programmers](../../references/ARCHITECTURE_COMPARISON.md).

---

## 6. Pipeline registers carry identity as well as data

A common incomplete drawing puts only an ALU value in the stage registers.
The machine must also preserve enough metadata to know whose value it is.

```text
IF/ID:
  PC, instruction bits, valid/prediction metadata

ID/EX:
  operand values, immediate, rd, operation, memory/write controls, PC, valid

EX/MEM:
  ALU/address result, store data, rd, controls, exception metadata, valid

MEM/WB:
  load or ALU result, rd, write control, exception metadata, valid
```

A **valid bit** distinguishes a real instruction from a bubble. A **kill**
or flush action invalidates younger wrong-path work. Without identity and
validity, a number arriving at WB would not say whether it should update
`x9`, write memory, raise an exception, or be ignored.

The values in these registers are microarchitectural state. Software observes
their eventual architectural effects, not the registers themselves.

---

## 7. Architectural state and the first retirement model

Architectural state is the state promised by the ISA and privilege
architecture:

- integer and other architected registers;
- the architecturally defined PC/sequencing behavior;
- memory effects under the architecture's ordering rules;
- privilege, interrupt, exception, and control state where applicable.

Pipeline registers are implementation state. Several instructions may be in
flight, but software needs behavior equivalent to the specified program order
for this simple in-order machine.

We will use **retirement** to mean:

> the point at which an instruction's result is accepted as part of the
> non-speculative architectural history.

In the simplified pipeline, completion and retirement are nearly coincident
and occur in order near WB. In an out-of-order core, execution may finish in a
different order while retirement restores in-order architectural commitment.

### Why precise state matters

Suppose an instruction faults. A precise exception boundary presents state as
if:

```text
all older instructions completed
the faulting instruction did not complete
no younger instruction completed
```

That contract lets an OS identify the faulting instruction, deliver a signal,
page memory in, emulate an operation, or resume a guest. Day 6 connects
flushing and exception metadata to this boundary.

“WB writes a register” is therefore only the beginning. Stores, exceptions,
interrupts, CSRs, and speculative work require a disciplined commitment
policy.

---

## 8. Why this model still matters on current machines

Modern application processors are usually deeper, wider, speculative, and
out of order. They may translate ISA instructions into internal operations,
use separate pipelines by operation class, and retire several instructions
per cycle. The five-stage model remains useful because it isolates durable
questions:

- Where is a value produced?
- When may a dependent consumer use it?
- Which work is older or younger?
- Which state is speculative?
- At what boundary does an effect become architectural?
- Which resource limits throughput?

Those questions recur in Intel and AMD optimization manuals, Arm pipeline and
PMU documentation, and RISC-V implementations even though the stage names and
latencies differ.

Do not infer a current CPU's exact pipeline depth from this lesson. Vendor
optimization guides and measured performance-counter evidence are the
appropriate sources for a particular core.

---

## 9. Kernel, `perf`, and virtualization relevance

### Performance accounting

Linux `perf stat` can report cycles and instructions when the platform PMU
supports them:

```bash
perf stat -e cycles,instructions -- command
```

A commonly computed ratio is:

```text
observed CPI = counted cycles / counted retired instructions
observed IPC = counted retired instructions / counted cycles
```

Interpret those counts cautiously:

- event names and semantics are architecture- and PMU-specific;
- multiplexing can scale counts when events exceed available counters;
- frequency changes make “cycles” different from wall-clock time;
- kernel, hypervisor, guest, and user filtering changes what is counted;
- speculation may consume resources without increasing retired instructions;
- a process can migrate between cores unless constrained.

The pipeline equations explain the baseline. PMU documentation defines the
actual evidence.

### Precise events

Profilers want to attribute an event to the responsible instruction. A sampled
interrupt may arrive after the event because detection, buffering, and
interrupt delivery take time. Architecture features such as Intel PEBS or Arm
statistical profiling provide more precise mechanisms for selected events.
“Precise” is a documented property, not something inferred from one PC sample.

### Virtual CPUs

A hypervisor presents architectural CPU state to a guest while scheduling
virtual CPUs on physical cores. Pipeline state is generally not part of the
guest architectural interface. On a VM exit, hardware and the hypervisor
preserve the defined guest state, not every transient stage latch.

This separation is what makes different physical microarchitectures capable
of running the same guest ISA.

---

## 10. Lab — derive, trace, and challenge the ideal

File:

```text
05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py
```

It uses only Python's standard library.

### Predict before running

For stage delays `180, 220, 250, 210, 140 ps` and overhead `60 ps`, write:

1. the pipeline clock period;
2. latency in cycles and picoseconds;
3. cycles for 12 independent instructions;
4. whether one-instruction latency improves;
5. why batch speedup is less than five.

Then run:

```bash
python3 05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py predict
python3 05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py predict --answers
python3 05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py trace --instructions 8
python3 05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py compare
python3 05-riscv-cpu-microarchitecture/challenges/day-005-pipelining-and-throughput.py selftest
```

### Observe

Preserve:

- the first cycle in which `I1` reaches WB;
- the last cycle in which `I8` reaches WB;
- the calculated period and one-instruction latency;
- the 100-instruction batch speedup;
- one sentence explaining why the trace is idealized.

### Extend without changing the file

On paper, split EX from 250 ps into two 145 ps logic stages. Recalculate:

```text
new stage count
new clock period
new one-instruction latency
cycles and time for 1, 8, and 100 instructions
```

The result should make the trade explicit: a higher possible throughput can
coexist with greater latency and larger fill/drain cost.

---

## 11. Evidence: Explain, Draw, Observe, Connect

### Explain

Without notes:

1. derive the pipelined clock equation from register-to-register timing;
2. distinguish latency, throughput, CPI, and clock period;
3. explain why the slowest stage controls the clock;
4. derive `k + N - 1`;
5. explain why pipeline registers need metadata and valid bits;
6. distinguish execution completion from architectural retirement.

### Draw

Draw the five stages and all four interstage registers. Put one instruction in
each stage. Annotate:

- the oldest and youngest instruction;
- one architectural register write;
- one microarchitectural result;
- the retirement point;
- where a bubble would be represented.

### Observe

Run the lab and reconcile every number with an equation. A screenshot of the
trace without the derivation is not sufficient evidence.

### Connect

Choose one:

- a `perf stat` CPI observation;
- a page-fault restart;
- a VM exit preserving guest state;
- a latency-bound pointer chase;
- a throughput-oriented independent loop.

Explain which part of today's model helps and which part the five-stage model
omits.

---

## 12. Mastery checkpoint

Answer:

> Why can pipelining improve instruction throughput while making one
> instruction take longer, and what state must be carried to preserve the ISA?

A complete answer includes:

- critical-path splitting;
- the maximum stage delay plus boundary overhead;
- overlap among different instructions;
- fill and drain;
- latency versus throughput;
- stage imbalance and register cost;
- interstage data, identity, control, and validity;
- in-order retirement and precise architectural state.

---

## Why? notebook

1. Why does unused time in one stage not automatically repair another stage's
   critical path?
2. Why does adding a register both help timing and add delay?
3. Why can the first result arrive later while the hundredth arrives sooner?
4. Why is ideal CPI 1 an asymptote for a finite pipeline run?
5. Why must `rd` travel with an ALU value?
6. Why can a bubble occupy a cycle without representing an instruction?
7. Why is a pipeline register not normally architectural state?
8. Why does an OS need a precise exception boundary?
9. Why can retired-instruction counts differ from fetched instruction counts?
10. Why is the five-stage model useful but not a literal diagram of a current
    high-performance CPU?

---

## References used selectively

- RISC-V International, *The RISC-V Instruction Set Manual, Volume I:
  Unprivileged Architecture*, current ratified specifications and instruction
  semantics: <https://docs.riscv.org/>
- RISC-V International, *The RISC-V Instruction Set Manual, Volume II:
  Privileged Architecture*, trap and architectural-state contracts:
  <https://docs.riscv.org/>
- David A. Patterson and John L. Hennessy, *Computer Organization and Design:
  RISC-V Edition*, processor chapter, for the pedagogical five-stage datapath,
  pipeline timing, and hazards.
- Linux kernel documentation, “Perf events and tool security,” for PMU access
  and event-accounting context:
  <https://docs.kernel.org/admin-guide/perf-security.html>
- Linux `perf-stat(1)` manual, for current command options, event grouping,
  filtering, and metric reporting:
  <https://man7.org/linux/man-pages/man1/perf-stat.1.html>
- Intel, *64 and IA-32 Architectures Optimization Reference Manual*, current
  core-specific latency, throughput, front-end, retirement, and PMU guidance:
  <https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html>
- AMD, *Software Optimization Guides*, current processor-family optimization
  and performance-monitoring material:
  <https://www.amd.com/en/search/documentation/hub.html>
- Arm, *Learn the architecture: Understanding the Armv8.x and Armv9.x
  extensions*, and PMU documentation, for current architectural and profiling
  context: <https://developer.arm.com/Architectures>

**Next bridge:** overlap creates conflicts. Day 6 classifies structural, data,
and control hazards, then uses forwarding, stalls, prediction, and flushing
to preserve the in-order architectural result.
