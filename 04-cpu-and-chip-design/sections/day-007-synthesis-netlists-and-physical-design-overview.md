# Day 7 — Synthesis, Netlists, and Physical-Design Overview

**Target time:** approximately 2½–3 hours

- Simulation versus synthesis: 25 minutes
- RTL to generic and mapped netlists: 50 minutes
- Timing, area, power, and implementation boundaries: 35 minutes
- One Yosys observation and end-to-end trace: 60 minutes

> A simulator showed the register changing after a clock edge. What turns that
> behavior into hardware structure—and how does it reconnect to the
> instructions and software that motivated this module?

Synthesis is the bridge between RTL and an implementation netlist. It is not
the end of chip design, and this lesson is not a detour into becoming an ASIC
physical designer. We need just enough depth to understand what CPU RTL
means, why frequency and power constrain microarchitecture, and what lies
beneath the ISA boundary used by systems software.

```text
software operation
  → ISA instruction
  → datapath/control action
  → RTL state transition
  → inferred mux/adder/register
  → gates and physical switching
```

---

## 1. Simulation and synthesis are different experiments

A **simulator** executes a model over simulated time. It evaluates events,
four-state values (`0`, `1`, `X`, `Z`), clock edges, and testbench stimulus.
It asks:

> For these inputs, what values and events does this model produce?

A **synthesizer** interprets the synthesizable subset as a hardware
description, then builds and transforms a graph of logic and state. It asks:

> What supported hardware structure implements this behavior?

SystemVerilog can describe activities that are valuable in a testbench but
are not circuits:

```systemverilog
initial begin
    #10 request = 1'b1;
    $display("request changed");
end
```

The delay and print are simulator actions. In contrast, this edge-triggered
assignment expresses intended state:

```systemverilog
always_ff @(posedge clk)
    result <= next_result;
```

“Simulation passed” does not prove synthesizability, timing closure, or
correct physical behavior. “Synthesis succeeded” does not prove the design
meets its specification. They provide different evidence.

### Four-state values are a model

`X` can reveal uninitialized or conflicting state and `Z` can represent
high-impedance behavior in simulation. They are not extra stable Boolean
voltage rails inside an ordinary CMOS gate. A synthesis tool may use some
unknown-valued cases as don't-care freedom. Never read an RTL `X` as a
transistor-accurate analog result.

---

## 2. The RTL-to-netlist path

The useful conceptual pipeline is:

```text
RTL source
  → elaboration
  → generic inference
  → optimization
  → technology mapping
  → target netlist
```

### Elaboration makes the requested design concrete

Elaboration chooses the top module, resolves parameters and widths, expands
generate constructs, creates instances, connects ports, and turns procedural
blocks into an internal representation. A parameterized eight-bit adder and
its 32-bit instance are not the same hardware size.

Hierarchy helps humans organize a design, but later optimization can flatten
or rearrange it. Source module boundaries need not survive unchanged in the
netlist.

### Generic inference recognizes hardware intent

Consider:

```systemverilog
logic [7:0] selected;

assign selected = choose_b ? b : a;

always_ff @(posedge clk) begin
    if (reset)
        result <= '0;
    else if (enable)
        result <= selected + 8'd3;
end
```

A generic representation can contain:

- a mux for `choose_b ? b : a`;
- an eight-bit addition;
- selection for the enable behavior;
- reset behavior;
- eight edge-triggered storage bits.

These are inferred operations, not a promise of particular transistors.

Incomplete combinational assignment can accidentally request storage:

```systemverilog
always_comb begin
    if (load)
        out = new_value;
    // What drives out when load is zero?
end
```

If the required behavior is “retain the previous value,” a latch is state,
not combinational logic. Complete assignments make the intended mux inputs
explicit.

### Optimization may erase the source-shaped structure

Tools propagate constants, remove unused logic, simplify Boolean expressions,
reduce widths, combine muxes, and rewrite arithmetic. `x + 0` can lose its
adder. An unobservable register can disappear. Equivalent netlists can look
very different.

Therefore, “one `+` in the source means one final cell named ADD” is false.
The requirement is preserved behavior under the tool's assumptions, not
preserved spelling.

### Technology mapping needs a target

Generic mux, adder, and register operations must eventually map into
available resources:

- an ASIC library offers characterized standard cells with particular logic
  functions, drive strengths, timing, area, and power models;
- an FPGA offers fixed LUTs, flip-flops, carry chains, memories, DSP blocks,
  clocks, and programmable routing.

The lab maps into Yosys's generic/internal primitives. That is enough to
observe structural transformation. It is not a foundry-cell result, FPGA
bitstream, transistor layout, or final area/power/timing prediction.

---

## 3. Why CPU designers care about constraints

A CPU is not useful merely because its state transitions are logically
correct. Those transitions must happen within electrical and timing limits.

For a simplified register-to-register setup path:

```text
launch register
  -- clock-to-Q --> combinational datapath
  -- propagation --> capture register setup window
```

A rough condition is:

```text
clock period ≥ clock-to-Q(max) + logic delay(max) + setup + uncertainty
```

Hold analysis checks the earliest arriving change near the same capture edge.
Real static timing analysis accounts for characterized arcs, clocks,
constraints, modes, corners, and eventually interconnect parasitics.

Constraints describe questions such as:

- What is the clock period?
- When may external inputs arrive?
- When must outputs be valid?
- What load or drive assumptions apply?
- Are any paths intentionally multicycle or logically false?

A clock constraint does not build an oscillator or route a clock wire.
Missing or incorrect constraints can produce a clean report for the wrong
question.

### Area, power, and timing shape microarchitecture

The three goals compete:

- stronger or duplicated logic can improve timing but consume area and power;
- shared hardware can save area but add muxing and delay;
- another pipeline stage can shorten combinational paths and raise frequency,
  but adds registers, clock load, latency, and hazard/control complexity;
- wider issue, larger caches, more ports, and aggressive speculation can
  improve some workloads while increasing switching, wiring, and design cost.

This is why pipeline depth, ALU organization, register-file ports, bypass
networks, cache size, and clock frequency are not purely abstract choices.
The physical costs rise back into CPU architecture.

---

## 4. Synthesis does not create placement or routing

A **netlist** names cells/operations and their connections. It does not
normally say where each standard cell physically sits or exactly which metal
segments connect it.

After ASIC logic synthesis, a much larger flow conceptually includes:

```text
floorplan → place → clock-tree synthesis → route
          → extract parasitics → timing/power/physical signoff
```

- **Floorplanning** allocates major regions, macros, I/O, and power strategy.
- **Placement** chooses legal cell locations.
- **Clock-tree synthesis (CTS)** builds clock distribution.
- **Routing** assigns physical wires and vias.
- **Signoff** checks timing and physical/electrical requirements using much
  richer models.

This concise map is enough for our boundary: synthesis has not performed
these steps. FPGA implementation likewise still packs, places, and routes
the mapped resources before producing configuration data, but it targets a
prefabricated programmable fabric rather than custom standard-cell geometry.

---

## 5. One small Yosys observation

The lab intentionally asks one bounded question:

> Can we see a selected addition and a clocked result change from RTL
> constructs into generic then mapped structural representations?

From the repository root:

```bash
bash 04-cpu-and-chip-design/challenges/day-007-yosys-synthesis-lab.sh
```

The script requires Yosys and was validated with Yosys 0.67. If `iverilog`
and `vvp` are available, it first runs a small behavioral check. It writes
all generated logs, statistics, Verilog/JSON netlists, and DOT graphs under
`/tmp`, never into tracked source directories.

Before running, predict:

1. Which expression implies a mux?
2. Where should an addition operation appear?
3. How many bits of state does `result` require?
4. What could optimization remove or reshape?
5. Why might generic and mapped cell counts differ?

After running, inspect:

```bash
less /tmp/week4-day007-yosys/generic-stat.txt
less /tmp/week4-day007-yosys/mapped-stat.txt
less /tmp/week4-day007-yosys/generic-netlist.v
less /tmp/week4-day007-yosys/mapped-netlist.v
```

The script prints the exact artifact path and key cell names. DOT is a graph
description; when Graphviz is installed, the script also renders SVG.

### What the observation proves

It provides evidence that Yosys:

- parsed and elaborated this top module;
- recognized process/operation structure;
- emitted generic and mapped netlist representations;
- reported structural statistics.

Separately, when Icarus is available, the RTL testbench provides behavioral
evidence for its few exercised cases. This lab does not perform formal or
sequential equivalence checking between the RTL and generated netlists.

### What it does not prove

It does **not** establish:

- transistor-accurate behavior;
- mapping to a real ASIC library or a named FPGA;
- final silicon area, frequency, or power;
- correct placement, CTS, or routing;
- timing closure across operating corners;
- functional correctness beyond the tested cases;
- manufacturability or signoff.

That limitation is part of the lesson, not a weakness to hide.

---

## 6. Reconnect the netlist to software

Suppose C contains:

```c
acc = acc + input;
```

The end-to-end path is not “C becomes transistors” in one jump:

```text
C expression
  → compiler-selected ISA instructions
  → fetched instruction bits
  → decoder/control signals
  → register-file reads and operand selection
  → ALU addition
  → result captured in architectural state
  → later instructions observe that state
```

Inside the implementation, operand selection can involve muxes, addition can
involve carry logic or a target-specific arithmetic structure, and writeback
ends at clocked state. Each gate transition is ultimately charge moving
through transistor networks. RTL and synthesis let engineers manage that
enormous composition without pretending the lower physical layer vanished.

The ISA remains the contract seen by the compiler, operating system, and
application. Microarchitecture decides how a CPU realizes that contract:
single-cycle, multicycle, pipelined, cached, speculative, or otherwise.

### The bridge back to systems

The next questions return upward:

- How do pipelining, hazards, forwarding, and stalls preserve ISA behavior?
- How do caches and memory interfaces change latency without changing the
  architectural memory contract?
- How do the MMU, page tables, TLB, privilege levels, exceptions, and
  interrupts connect CPU mechanisms to an OS?
- How does a kernel establish execution state, handle traps, and isolate
  processes?

Those questions lead toward CPU microarchitecture and the guided ToyOS work.
The HDL/chip material supplies necessary causal depth; it is not a divergent
specialization into professional ASIC implementation.

---

## 7. Mastery: Explain · Draw · Observe · Build

### Explain

Without notes:

- distinguish simulation, elaboration, synthesis, optimization, and mapping;
- explain how mux, adder, and flip-flop behavior can be inferred;
- explain why a netlist is not placement or routing;
- connect timing/area/power pressure to one CPU design choice;
- trace one software addition through ISA, datapath, RTL, netlist, gates, and
  transistor switching.

### Draw

Draw one diagram:

```text
software → ISA → decoder/control → datapath/register
         → RTL → generic netlist → mapped target resources
         → physical implementation
```

Add a return arrow showing that physical delay, area, and power constrain
pipeline, cache, and datapath choices.

### Observe

Preserve:

- `yosys -V`;
- the optional Icarus simulation result;
- generic and mapped statistics;
- one generic cell and its mapped structural replacement;
- one sentence for each conclusion the artifacts cannot support.

### Build

Change the constant addend or mux selection in the lab RTL, predict the
structural effect, and rerun the same lab. Explain the difference without
using cell count as a claim about final transistor count or silicon area.

### Why? notebook

1. Why can simulation accept constructs that synthesis cannot build?
2. Why is elaboration necessary before optimization?
3. Why can an inferred adder disappear?
4. Why does technology mapping require a target?
5. Why can a netlist still fail timing after routing?
6. Why can pipelining help frequency but hurt latency, area, and control?
7. Which parts of `acc += input` belong to the ISA, microarchitecture, RTL,
   gate network, and physical device layers?

**Next bridge:** use this physically grounded model to study CPU
microarchitecture, memory translation and privilege, then cross the ISA and
exception boundaries while building ToyOS.
