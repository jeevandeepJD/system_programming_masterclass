# Week 4 — From Transistors to RTL and Synthesis

**Format:** seven daily sections collected into one weekly module

**Suggested workload:** approximately 21 focused hours, adjustable to mastery

> A register-transfer description is not a bag of transistors. It is a
> behavioral and structural specification that tools can simulate, elaborate,
> optimize, and map—provided its timing and hardware meaning are clear.

This hardware depth serves the end-to-end systems goal. It explains what
exists beneath ISA-visible registers, instructions, exceptions, and memory
accesses before the course returns to CPU microarchitecture, MMU/privilege,
and ToyOS. It is **necessary depth, not a divergent specialization** in
professional ASIC or physical design.

Week 1 introduced physical bits, feedback, registers, a minimal CPU,
architectural state, and addressed memory. Week 4 revisits only the mechanisms
needed to support those abstractions: transistor switching, restoring logic,
bit-cell storage, timing, RTL, and synthesis. It does not replace the Week 1
programmer-visible model with circuit details; it connects the layers.

“Week” names a mastery module, not a calendar deadline. Spend longer when a
waveform, timing diagram, or synthesized netlist contradicts a prediction.
Reading all seven sections is not completion: preserve evidence across
Explain, Draw, Observe, and Build.

## Why these seven days belong together

This module follows one causal chain without pretending every abstraction is
the same thing:

```text
MOSFET controlled conduction
  → complementary pull-up/pull-down networks
  → restoring logic gates
  → delay, capacitance, fan-out, energy, and noise constraints
  → clocked state with setup/hold and clock-to-Q timing
  → SRAM feedback and DRAM charge as array bit-cell mechanisms
  → register/cache/main-memory latency, density, and cost tradeoffs
  → SystemVerilog RTL and simulation
  → elaborated design and generic cells
  → optimized, technology-mapped netlist
  → CPU datapath/control beneath the ISA
  → MMU, privilege, exceptions, and operating systems
```

The transistor explanation tells us why voltage, charge, delay, and power
cannot be ignored. The gate abstraction makes larger reasoning possible.
Timed sequential elements create stable state boundaries. RTL describes data
movement and operations between those boundaries. Synthesis then chooses and
rewrites a netlist that implements that RTL under a target model and
constraints. Placement, clock-tree synthesis, routing, and signoff are later
steps; synthesis alone does not create physical geometry. We name that
boundary concisely so we can reason honestly about CPU timing, area, and
power—not to turn this module into an ASIC-design course.

## Daily roadmap and exact files

### Day 1 — The MOSFET as a Controlled Switch

Build a useful device-level model of NMOS and PMOS behavior: terminals,
gate-controlled channels, body-diode caveats, charge, and why a MOSFET is not
an ideal Boolean switch.

Lesson:

```text
04-cpu-and-chip-design/sections/day-001-mosfet-as-a-switch.md
```

**Evidence:** explain channel control without saying current enters the
insulated gate, identify pull-up and pull-down roles, and relate a changing
node voltage to charging or discharging capacitance.

### Day 2 — CMOS Inverter and Noise Margins

Derive the CMOS inverter from complementary pull-up and pull-down devices.
Explain static behavior, valid voltage ranges, restoration, transition
behavior, and noise margins before composing larger gate networks.

Lesson:

```text
04-cpu-and-chip-design/sections/day-002-cmos-inverter-and-noise-margins.md
```

**Evidence:** draw the complementary inverter, explain restoration and noise
margins, and distinguish low settled static current from switching current.

### Day 3 — CMOS NAND, NOR, and Gate Networks

Derive NAND and NOR from complementary series/parallel transistor networks,
then connect those restoring gates to larger Boolean functions.

Lesson:

```text
04-cpu-and-chip-design/sections/day-003-cmos-nand-nor-and-gate-networks.md
```

**Evidence:** draw and trace NAND/NOR pull-up and pull-down networks for every
input combination, then explain what their truth tables omit physically.

### Day 4 — Delay, Capacitance, Fan-Out, and Power

Connect load capacitance and finite drive to propagation delay; distinguish
rise/fall behavior; reason about fan-out, dynamic energy, leakage,
short-circuit current, and the voltage/frequency tradeoff.

Lesson:

```text
04-cpu-and-chip-design/sections/day-004-delay-capacitance-fanout-and-power.md
```

**Evidence:** annotate a loaded gate waveform, compare fan-out cases, and use
`P_dynamic ≈ α C V² f` without treating it as a complete chip power equation.

### Day 5 — Storage Cells, Sequential Timing, and Metastability

Move from feedback to 6T SRAM, DRAM charge storage, latches, and edge-triggered
registers. Distinguish a bit-cell circuit from architectural memory; connect
register/cache/DRAM density, cost, and latency to cache misses; then use setup,
hold, clock-to-Q, skew, jitter, metastability, and clock-domain crossing
discipline to explain when state transfer is reliable.

Lesson:

```text
04-cpu-and-chip-design/sections/day-005-sequential-timing-and-metastability.md
```

**Evidence:** explain SRAM read/retention and DRAM sensing/restoration/refresh,
draw a register-to-register path, check setup and hold relationships, and
explain why synchronizers reduce rather than eliminate metastability risk.

### Day 6 — SystemVerilog RTL and Waveforms

Describe a small ALU/register stage with `always_comb`, `always_ff`,
nonblocking assignments, complete selection, reset, and valid state. Use a
testbench and waveform to distinguish combinational result from captured
state.

Lesson:

```text
04-cpu-and-chip-design/sections/day-006-systemverilog-rtl-and-waveforms.md
```

**Evidence:** simulate reset and enable cases, annotate state changes by clock
edge, identify `always_ff` as a register abstraction rather than a bit-cell
schematic, and explain why synthesizable RTL is only a subset of everything a
SystemVerilog simulator can execute.

### Day 7 — Synthesis, Netlists, and Physical-Design Overview

Follow RTL through elaboration, generic inference, optimization, and
technology mapping. Use one small Yosys observation, then reconnect the
netlist to CPU timing/area/power and the software→ISA→datapath→gates path.

Lesson:

```text
04-cpu-and-chip-design/sections/day-007-synthesis-netlists-and-physical-design-overview.md
```

Challenges:

```text
04-cpu-and-chip-design/challenges/day-007-synthesis-demo.sv
04-cpu-and-chip-design/challenges/day-007-synthesis-testbench.sv
04-cpu-and-chip-design/challenges/day-007-yosys-synthesis-lab.sh
```

**Evidence:** preserve optional simulation output plus generic/mapped
statistics, netlists, and DOT graphs from the single lab; identify inferred
mux/adder/flop structures and state the evidence handoff explicitly:
ngspice supports analog-model behavior, HDL simulation supports exercised
behavior, and synthesis supports structure—not silicon timing or layout.

## Adjustable workload guide

The nominal budget is approximately 21 focused hours:

```text
Day 1   3 h   MOS device model and switching observations
Day 2   3 h   CMOS inverter, restoration, noise margins
Day 3   3 h   NAND/NOR and complementary gate networks
Day 4   3 h   delay, loading, fan-out, dynamic/static power
Day 5   3 h   SRAM/DRAM cells, registers, timing, metastability, CDC
Day 6   3 h   SystemVerilog RTL, ALU/register, waveforms
Day 7   3 h   one synthesis observation and systems bridge
```

This is a planning estimate, not a completion rule. Split labs across
sessions when needed, repeat a waveform or synthesis run when the output is
surprising, and shorten familiar exposition only when the evidence remains
complete.

## Integrated weekly evidence

### Explain

Without notes, tell one connected story:

1. a MOS gate voltage controls channel conduction and therefore how a node
   capacitance can charge or discharge;
2. complementary transistor networks produce restoring logic levels;
3. finite resistance, capacitance, activity, and leakage create delay and
   power costs;
4. 6T SRAM uses cross-coupled feedback while DRAM uses capacitor charge,
   destructive sensing/restoration, leakage management, and refresh;
5. a bit-cell circuit is not architectural memory, and density/cost/latency
   tradeoffs help explain registers, cache SRAM, main DRAM, and cache misses;
6. timed sequential elements divide combinational work into state
   transitions;
7. SystemVerilog expresses intended combinational and sequential behavior;
8. simulation executes a model over time, while synthesis constructs and
   transforms an implementation graph;
9. elaboration resolves parameters, hierarchy, widths, and generate choices;
10. generic synthesis infers operations and storage, optimization changes
   structure while preserving required behavior, and technology mapping
   selects target-supported cells/resources;
11. constraints define clocks, I/O assumptions, and design limits used to
   judge candidate implementations;
12. synthesis still has not placed or routed physical resources;
13. timing, area, and power pressure feeds back into CPU choices such as
    pipeline depth, datapath width, cache organization, and clock frequency;
14. the ISA hides those implementation details while exposing the state and
    events on which compilers, kernels, and ToyOS depend.

### Draw

Produce one diagram containing:

- an NMOS/PMOS CMOS inverter and one two-input CMOS gate;
- a loaded gate with a capacitance and delayed output;
- a 6T SRAM cell at qualitative block level and a 1T1C DRAM cell, clearly
  labeled “bit cells, not architectural memory”;
- launch register → combinational path → capture register, with setup and
  hold windows;
- synthesizable RTL for a selected add feeding a register;
- RTL → elaborated hierarchy → generic mux/adder/flop → optimized graph →
  mapped cells/resources;
- software operation → ISA instruction → control/datapath → gates, with a
  concise “physical implementation remains” boundary.

Label where voltage behavior, Boolean behavior, cycle behavior, timing
constraints, and physical geometry become relevant. Do not draw synthesis as
if it emits a finished chip.

### Observe

Keep a compact evidence bundle:

- one device/gate switching observation or carefully annotated calculation;
- gate waveforms showing finite transition delay;
- one setup/hold timing exercise and metastability explanation;
- combinational and sequential RTL simulation logs/waveforms;
- Yosys version and commands;
- generic and mapped `stat` output from the single Day 7 observation;
- its generated generic/mapped netlists and DOT graphs;
- a bounded handoff: ngspice demonstrates analog behavior of its model, HDL
  simulation demonstrates exercised model behavior, and synthesis exposes
  structure; none establishes final silicon timing or layout;
- a short note explaining why cell counts can change with optimization and
  target mapping.

### Build

Complete or extend the labs so one datapath:

1. selects one of two input operands;
2. adds a constant or second operand;
3. captures the result in a clocked register with reset and enable;
4. passes an RTL simulation test;
5. synthesizes to observable mux, arithmetic/logic, and flip-flop structure;
6. can be modified and re-synthesized once to compare structure without
   claiming cell counts are final silicon area or transistor count.

Retain the loop:

```text
predict → simulate → synthesize → inspect → explain → change → compare
```

## Integrated mastery checkpoint

Trace one software-visible value through the entire stack:

1. How does a C operation become one or more ISA instructions?
2. How do fetch/decode and control select register and ALU actions?
3. How does RTL describe that combinational work and captured state?
4. Which mux, adder, and flip-flop structures can synthesis infer?
5. How do restoring gates compose those structures?
6. How does transistor conduction move charge to change a represented bit?
7. How do SRAM feedback and DRAM capacitor charge retain array bits, and why
   is neither cell by itself architectural memory?
8. How can a cache miss turn an architectural load into a lower-level DRAM
   access and line fill?
9. Why do fan-out, setup/hold, and propagation delay constrain the clock?
10. Why do area and power constrain datapath, pipeline, and cache choices?
11. What does the ISA expose while hiding those implementation choices?
12. Why do MMU, privilege, exception, and interrupt mechanisms form the next
    bridge from CPU hardware back to kernels and ToyOS?

Mastery is demonstrated when every arrow names both the representation and
the responsible tool or physical mechanism:

```text
software → ISA → datapath/control → RTL → netlist
         → gates → transistor switching
         → timing/area/power constraints on the CPU design
         → MMU/privilege/exceptions → operating system
```

## End-of-module reflection

Record in your own words:

- strongest understanding;
- remaining confusion;
- one prediction contradicted by simulation or synthesis;
- one place where a behavioral model was mistaken for physical reality;
- connection to CPU, kernel, driver, FPGA, or virtualization work;
- next revision action;
- whether to advance toward CPU microarchitecture, MMU/privilege, and ToyOS
  or repeat one lab.

Do not auto-fill confidence, completion, hours, or mastery. Those remain the
learner's evidence-based assessment.
