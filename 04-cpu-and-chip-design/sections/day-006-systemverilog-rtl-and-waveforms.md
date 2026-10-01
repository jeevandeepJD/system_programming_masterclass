# Day 6 — SystemVerilog RTL and Waveforms

**Target time:** approximately 2–3 hours

- RTL as a hardware abstraction: 35–45 minutes
- One ALU/register stage: 45–55 minutes
- Testbench, event scheduling, and waveform reading: 45–55 minutes
- Run, explain, and reconnect to the CPU: 30–40 minutes

> If SystemVerilog looks like software, why does an `always_ff` block become
> state while an `always_comb` block becomes gates?

## Why this day exists

Drawing every gate becomes impractical long before a CPU becomes interesting.
Hardware description languages let engineers specify useful structure and
behavior at a higher level, then use different tools for different questions:

```text
RTL source
  ├─ simulator: what scheduled behavior does this model produce?
  ├─ linter: what suspicious or inconsistent description is present?
  └─ synthesis: what hardware graph can implement the synthesizable subset?
```

SystemVerilog is not “C for hardware.” Source order is not the hardware's
execution order, time can be part of simulation, and many constructs have no
synthesizable meaning. RTL—**register-transfer level**—focuses on values
transformed by combinational logic and transferred between state elements on
clock boundaries.

Today uses one example only: an eight-bit ALU feeding an output register. The
point is to reveal the boundaries among:

- combinational intent;
- clocked state;
- simulation scheduling;
- a testbench;
- a waveform;
- synthesizable hardware;
- the later CPU datapath.

This is not a broad SystemVerilog language tutorial and not an advanced
verification course.

By the end, you should be able to:

- read a small `module` and its typed ports;
- distinguish `always_comb` from `always_ff`;
- explain blocking (`=`) and nonblocking (`<=`) assignments in this design;
- explain why the testbench is executable simulation support, not chip logic;
- read the result before and after a clock edge in a VCD waveform;
- describe the relevant simulator event ordering without memorizing the full
  language scheduler;
- identify which parts are intended for synthesis;
- map the example back to one stage of a CPU datapath.

---

## 1. RTL describes relationships in hardware

In software, one thread normally executes statements in sequence. In
hardware, an adder, an AND network, and a comparator can all exist and react
at once. RTL source is a notation for such concurrent structure and for
clocked updates.

Our design boundary is:

```text
a_in ─┐
      ├──▶ selected ALU operation ──▶ result register ──▶ result_out
b_in ─┘              ▲                       ▲
                   op_in                    clock

valid_in ─────────────────────────▶ valid register ─────▶ valid_out
```

The combinational ALU continuously derives a candidate value from `a_in`,
`b_in`, and `op_in`. The register captures that candidate at a rising edge.

This resembles a tiny CPU execution stage:

```text
operand registers / bypass muxes
    → ALU and selection logic
    → pipeline register
    → next stage
```

The example omits register files, hazards, forwarding, flags, exceptions, and
control. Those return in the CPU lessons. For now, one boundary is enough.

---

## 2. Read the module interface as wires crossing a boundary

The lab module begins conceptually as:

```systemverilog
module alu_register_stage (
    input  logic       clk,
    input  logic       rst,
    input  logic       valid_in,
    input  logic [7:0] a_in,
    input  logic [7:0] b_in,
    input  logic [1:0] op_in,
    output logic       valid_out,
    output logic [7:0] result_out
);
```

A module is a design boundary. Ports describe signals entering and leaving
that boundary.

`logic [7:0]` is an eight-bit packed vector with indices 7 down to 0.
SystemVerilog's four-state simulation domain includes `0`, `1`, `X`
(unknown), and `Z` (high impedance). That does not mean a physical wire has a
stable voltage named `X`. Four-state values help a simulator expose missing
initialization, conflicting drivers, or intentionally unknown behavior.

Signal width is part of the design. The eight-bit addition naturally keeps
the low eight result bits here:

```text
0xff + 0x01 → 0x00
```

The carry is discarded because this interface has no carry output. A real ISA
must define whether it ignores carry, records flags, traps, or uses a wider
result. RTL implements a chosen contract; it does not invent one.

---

## 3. `always_comb`: describe the candidate value

The lab contains:

```systemverilog
always_comb begin
    case (op_in)
        2'b00:   alu_result = a_in + b_in;
        2'b01:   alu_result = a_in & b_in;
        2'b10:   alu_result = a_in ^ b_in;
        default: alu_result = (a_in < b_in) ? 8'd1 : 8'd0;
    endcase
end
```

`always_comb` states combinational intent. The simulator reevaluates the
process when a read input changes, and tools check stronger rules than they
would for a generic procedural block.

Every possible `op_in` value assigns `alu_result`. That matters. If one path
left it unassigned, preserving the previous value would require storage—an
accidental latch in what was meant to be combinational logic.

### Blocking assignment here

The block uses blocking assignment:

```systemverilog
alu_result = expression;
```

Within a procedural activation, the left side updates immediately before the
next statement in that process. For straightforward combinational logic,
blocking assignment makes intermediate calculations follow ordinary
data-dependency order.

Do not turn “blocking for combinational” into a magical syntax rule. The
important question is whether the whole process describes a complete
combinational function with no remembered value and no conflicting drivers.
The convention supports that intent and avoids common simulation races.

### What hardware might result?

The source names operations, not final gates. Synthesis may infer:

- an eight-bit adder;
- bitwise AND and XOR logic;
- an unsigned comparator;
- selection logic controlled by `op_in`.

Optimization can restructure or share logic. The source does not specify
physical placement, exact cell count, route delay, or maximum frequency.

---

## 4. `always_ff`: describe the state boundary

The clocked part is:

```systemverilog
always_ff @(posedge clk) begin
    if (rst) begin
        valid_out  <= 1'b0;
        result_out <= 8'd0;
    end else begin
        valid_out  <= valid_in;
        result_out <= alu_result;
    end
end
```

`always_ff` states that these variables represent flip-flop-like state
updated on the rising edge. Reset is synchronous in this example: `rst` is
observed at a rising edge. Merely changing `rst` between edges does not
immediately clear the outputs.

This is an RTL **register abstraction**: it specifies when state is captured
and how the next value is chosen. It does not specify a transistor-level
storage circuit. For this small stage, synthesis will normally infer a bank
of flip-flops; the source is not a drawing of a 6T SRAM cell, a DRAM cell, or
an architectural register file. Day 5's bit cells answer “how can one array
retain charge or feedback state?” while `always_ff` answers “what clocked
state behavior does this RTL require?”

That is a design choice, not the universal meaning of reset. CPUs and SoCs
use synchronous resets, asynchronous resets, reset synchronizers, retention,
and power-domain sequencing according to implementation needs.

### Nonblocking assignment and simultaneous-looking capture

The clocked block uses nonblocking assignment:

```systemverilog
result_out <= alu_result;
```

At a rising edge, right-hand sides are evaluated using the current values,
then left-hand updates are scheduled for the nonblocking-assignment update
region. This models multiple registers sampling old-state-derived values at
the same edge.

For example:

```systemverilog
q1 <= d;
q2 <= q1;
```

At one edge, `q1` receives the old `d`, while `q2` receives the old `q1`.
The source order does not turn that into two cycles of data movement within
one physical edge.

This is why nonblocking assignment is the standard choice for clocked state.
Using blocking assignments carelessly in interacting clocked processes can
make simulation depend on process execution order, creating behavior that
does not represent the intended parallel capture.

---

## 5. Current value, candidate value, and captured value

Suppose before a rising edge:

```text
a_in       = 3
b_in       = 5
op_in      = ADD
alu_result = 8       combinational candidate
result_out = old     stored output from an earlier edge
```

At the rising edge, the `always_ff` process samples `alu_result`. After the
nonblocking update executes:

```text
result_out = 8
```

The distinction is the whole register-transfer model:

```text
between edges: candidate values settle
at edge:       selected candidates are sampled
after edge:    registered outputs update
```

This model assumes the Day 5 physical timing contract is satisfied. RTL
simulation does not automatically account for clock-to-Q, setup, hold,
routing, skew, or jitter. Those require timing constraints, implementation
information, static timing analysis, and sometimes timing-aware simulation.

---

## 6. The testbench is outside the synthesized design

The testbench instantiates the design under test:

```systemverilog
alu_register_stage dut (...);
```

It then provides an artificial environment:

- an `initial` process sets starting values;
- `forever #5` creates a 10 ns clock;
- a task drives operands and an operation;
- assertions compare observed and expected values;
- `$dumpfile` and `$dumpvars` record a VCD waveform;
- `$display`, `$fatal`, and `$finish` report and control simulation.

These features are valuable precisely because a simulator executes them.
They are not intended to become gates in this lab.

### A basic assertion

After a rising edge and a small observation delay, the testbench checks:

```systemverilog
assert (result_out === expected)
    else $fatal(1, "...");
```

This is an immediate assertion: evaluate the condition now and fail if it is
not true. The case-equality operator `===` treats `X` and `Z` explicitly, so
an unknown result cannot accidentally pass as an ordinary Boolean match.

This small check is enough for today's purpose. Industrial verification adds
constrained random stimulus, temporal assertions, coverage, formal methods,
reference models, and much more. None is needed to understand this boundary.

### Why drive at the falling edge?

The testbench changes inputs at a falling edge, halfway before the rising
capture edge:

```text
falling edge: testbench drives inputs
    ↓ combinational process reevaluates
rising edge: register captures result
    ↓ nonblocking updates occur
testbench observes after #1
```

This avoids a testbench/design race at the same rising edge and makes the
waveform easy to read. It is not a physical setup-time proof; `#1` and `#5`
are simulation delays in the testbench.

---

## 7. The event model: why “at the same time” needs ordering

A simulator cannot literally run every process at once. It uses an event
queue. Several events can occur at the same simulation timestamp in ordered
regions and repeated zero-time iterations called delta cycles.

For this lab, retain this reduced model:

1. A testbench input change schedules affected combinational logic.
2. `always_comb` computes `alu_result` without advancing simulation time.
3. A rising `clk` schedules the `always_ff` process.
4. The clocked process evaluates right-hand sides.
5. Nonblocking assignments update registered outputs later in the same
   timestamp.
6. The testbench's `#1` check observes after those updates.

The full IEEE scheduler contains more regions for assertions, program blocks,
and other semantics. The reduced model is sufficient to explain this
waveform. Learn more scheduler detail when a real race or verification
construct requires it.

### Delta cycles are not CPU cycles

A delta cycle is zero simulation time used to settle event dependencies. It
is not:

- a clock period;
- a gate delay;
- an instruction cycle;
- evidence that silicon performs statements sequentially.

If `alu_result` changes at the same timestamp as an input, that means the RTL
model reevaluated in zero modeled physical time. The gate implementation will
have propagation delay.

---

## 8. Run the one lab

Files:

```text
04-cpu-and-chip-design/challenges/day-006-alu-register-stage.sv
04-cpu-and-chip-design/challenges/day-006-alu-register-stage-tb.sv
04-cpu-and-chip-design/challenges/day-006-run-alu-register-stage.sh
```

The script:

1. runs Verilator lint over the design and testbench;
2. compiles the same sources with Icarus Verilog;
3. runs the simulation with `vvp`;
4. leaves a VCD file beside the lab;
5. prints an optional GTKWave command but never requires a GUI.

From the repository root:

```bash
bash 04-cpu-and-chip-design/challenges/day-006-run-alu-register-stage.sh
```

Expected final line from the testbench:

```text
PASS: 6 checks; VCD written
```

### Predict before running

Write down:

1. When inputs become `3`, `5`, and ADD, when may `alu_result` change?
2. When may `result_out` change?
3. What eight-bit result follows `0xff + 1`?
4. When `valid_in=0`, does this design hold the old result or merely mark the
   newly captured result invalid?
5. Does a passing simulation establish setup/hold timing?

The fourth question exposes a real interface decision. This stage captures
`alu_result` every non-reset edge; `valid_out` says whether consumers may
interpret it. Invalid payload bits are don't-care at the protocol level even
though the waveform still shows a value.

---

## 9. Read the VCD without requiring a GUI

The Value Change Dump file is:

```text
04-cpu-and-chip-design/challenges/day-006-alu-register-stage.vcd
```

If GTKWave is installed and a graphical session is available:

```bash
gtkwave 04-cpu-and-chip-design/challenges/day-006-alu-register-stage.vcd
```

Add at least:

```text
clk
rst
valid_in
a_in
b_in
op_in
alu_result
valid_out
result_out
```

For one ADD transaction, annotate:

1. the falling edge where the testbench drives inputs;
2. the delta-cycle change of combinational `alu_result`;
3. the next rising edge;
4. the registered-output update after that edge;
5. the testbench check one nanosecond later.

GTKWave is only a viewer. Simulation already completed and assertions already
checked behavior. The GUI does not make the evidence more correct.

If no GUI is available, keep the simulation log and inspect the VCD as text:

```bash
less 04-cpu-and-chip-design/challenges/day-006-alu-register-stage.vcd
```

VCD uses compact identifier codes, so text inspection is less convenient,
but it confirms that the artifact is portable trace data rather than an image.

---

## 10. Simulation is not synthesis

The design module is written in a conventional synthesizable subset:

- fixed-width ports and internal signals;
- combinational arithmetic, comparison, bitwise operations, and `case`;
- one edge-triggered process;
- synchronous reset;
- no testbench delays or file-driven behavior.

The testbench deliberately uses nonsynthesizable simulation features:

- `#` delays;
- an endless clock generator;
- tasks used as stimulus;
- `$dumpfile`, `$dumpvars`, `$display`, `$fatal`, and `$finish`.

Some constructs can be supported differently across synthesis tools, targets,
and flows. “SystemVerilog supports it” does not imply “this synthesizer maps
it as intended.” The target and tool documentation remain part of the
contract.

### What each result proves

A passing simulation proves:

> For the tested stimulus, this simulator's execution of this RTL model
> satisfied these assertions.

Warning-clean lint proves:

> These lint rules found no reported issue in this source under these options.

Neither proves:

- exhaustive functional correctness;
- setup or hold closure;
- metastability safety;
- a particular gate netlist;
- final area, frequency, or power;
- correct CPU behavior around this stage.

Synthesis will be the next representation boundary. Physical implementation
comes later still.

---

## 11. Return to the CPU

Replace the lab names with CPU names:

```text
a_in, b_in       selected source operands
op_in            decoded ALU control
alu_result       execute-stage candidate
result_out       pipeline-register value
valid_out        whether the stage carries a real instruction/result
```

Now the missing machinery becomes visible:

- Where do source operands come from?
- How does instruction decode choose `op_in`?
- Which destination register number travels with the result?
- How are overflow, comparison, and exception metadata carried?
- What happens when an older instruction stalls?
- How is a branch redirect handled?
- How does forwarding select a value not yet written back?

Those are CPU architecture questions, not reasons to turn this lesson into a
larger HDL tutorial. SystemVerilog is the notation we will use to make the
answers executable and inspectable.

---

## 12. Mastery evidence

### Explain

Without notes:

- explain why RTL is not sequential software;
- identify the combinational and stateful parts of the lab;
- explain why every `case` path assigns `alu_result`;
- explain blocking assignment in the combinational process;
- explain nonblocking assignment in the clocked process;
- explain why the testbench waits until after the edge to check;
- distinguish a delta cycle from a clock cycle;
- separate simulation evidence from synthesis and timing evidence.

### Draw

Draw one diagram:

```text
inputs → four ALU candidates / selection → D input of result register
                                            ↑ rising-edge clock
                                            ↓
                                      registered result
```

Beside it, draw a waveform for one ADD transaction. Mark:

- testbench drive;
- combinational candidate change;
- capture edge;
- nonblocking registered update;
- assertion check.

### Observe

Preserve:

- Verilator's warning-clean lint result;
- Icarus's warning-clean compile result;
- the `PASS: 6 checks` message;
- the generated VCD;
- one annotated transaction;
- one sentence saying what the waveform does not prove.

### Build—one small change only

Add an enable behavior to a copy of the lab:

```text
valid_in=1   capture ALU result
valid_in=0   preserve result_out and clear valid_out
```

Before editing, predict which waveform changes during the invalid
transaction. Keep the assignment style and rerun the same lint/simulation
loop. This single extension is enough; the next lesson returns to synthesis
and CPU structure.

### Mastery checkpoint

Answer:

> Starting from stable ALU inputs before a rising edge, explain what the
> combinational RTL, simulator scheduler, nonblocking assignment, waveform,
> and intended register hardware each contribute to the observed result.

Do not say that the simulator “executes the ALU and then executes the
register” as though those were software statements. The hardware blocks
coexist; the simulator schedules a model of their behavior.

---

## Why? notebook

1. Why does incomplete combinational assignment imply remembered state?
2. Why can all ALU operations exist in hardware even though `case` selects one
   result?
3. Why do clocked assignments observe old values at one edge?
4. Why can a waveform show zero-time combinational changes when gates have
   delay?
5. Why is a testbench clock generator not synthesized into the design?
6. Why can invalid payload bits still change?
7. Why does passing RTL simulation not prove setup/hold timing?
8. Why can synthesis legally produce a structure unlike the source layout?
9. Why is `X` useful in simulation but not a stable hardware logic level?
10. Where would destination register identity and exception state travel in a
    real CPU pipeline?

---

## References used selectively

- IEEE Std 1800-2023, *SystemVerilog—Unified Hardware Design,
  Specification, and Verification Language*, for `always_comb`, `always_ff`,
  assignment, assertion, and scheduling semantics:
  <https://standards.ieee.org/ieee/1800/7743/>
- Icarus Verilog documentation, for SystemVerilog compilation and `vvp`
  simulation:
  <https://steveicarus.github.io/iverilog/>
- Verilator guide, especially lint and timing options:
  <https://verilator.org/guide/latest/>
- GTKWave documentation and source, for VCD waveform inspection:
  <https://gtkwave.github.io/gtkwave/>
- David Money Harris and Sarah L. Harris, *Digital Design and Computer
  Architecture*, HDL and sequential-logic chapters, for the connection from
  RTL to datapaths and state.

**Next bridge:** synthesis will elaborate this module, infer arithmetic,
selection, and flip-flop structures, then optimize a netlist. After that
brief tool boundary, the course returns to the CPU's register file, decoder,
program counter, control, and instruction execution.
