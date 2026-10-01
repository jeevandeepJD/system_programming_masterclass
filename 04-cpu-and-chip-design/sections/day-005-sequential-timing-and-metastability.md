# Day 5 — Sequential Timing and Metastability

**Target time:** approximately 2–3 hours

- Feedback, latches, flip-flops, and the clock: 40–50 minutes
- Setup, hold, clock-to-Q, skew, and jitter: 50–60 minutes
- Metastability, synchronizers, and CDC warnings: 40–50 minutes
- CPU timing exercises and mastery evidence: 25–35 minutes

> An ALU may produce the right result eventually. What makes it safe for a CPU
> register to call that result its new state at one particular clock edge?

## Why this day exists

Combinational logic has no intentional memory. Change its inputs and, after
finite propagation delay, its outputs change. A CPU cannot be built from that
alone. Its program counter must retain an address, its register file must
retain operands, and a pipeline stage must preserve one instruction's result
while the next instruction enters the logic behind it.

Feedback creates the possibility of state. Clocked storage makes state
changes orderly. Timing constraints describe when that order is reliable.

```text
old register state
    → finite clock-to-Q delay
    → combinational CPU work
    → setup at the next register
    → next edge captures new state
```

That arrow chain is the purpose of today's lesson. It is also where the neat
digital model touches analog reality: a storage element asked to decide at
the wrong time can become metastable.

By the end, you should be able to:

- explain how controlled feedback preserves state;
- distinguish an SRAM or DRAM bit cell from architectural memory;
- explain qualitatively why registers, caches, and main memory use different
  storage organizations;
- distinguish a level-sensitive latch from an edge-triggered flip-flop;
- explain what a clock coordinates and what it does not compute;
- reason about setup, hold, clock-to-Q, skew, and jitter;
- explain metastability as analog resolution, not a third logic value;
- state why synchronizers reduce failure probability rather than remove it;
- recognize common clock-domain-crossing mistakes;
- connect timing directly to CPU frequency, pipelining, reset, interrupts,
  device inputs, and multi-clock SoCs.

---

## 1. Feedback turns a present signal into remembered history

A combinational block is described by its current inputs:

```text
output = F(current inputs), after propagation delay
```

A sequential block also depends on state:

```text
next_state = F(current_state, current inputs)
```

The simplest route to state is feedback—returning an output to an input.
Cross-coupled restoring gates can reinforce either of two stable conditions:

```text
Q=0, Q_bar=1
Q=1, Q_bar=0
```

Each side helps hold the other side in its opposite state. This is physical
memory only while the circuit remains powered; ordinary CPU registers are
volatile.

Feedback is not automatically useful. An arbitrary combinational loop may
oscillate or settle unpredictably. Storage circuits arrange feedback and
control inputs so that stable states can be deliberately selected and held.

Historically, reliable state was one of computing's hardest resources.
Relays, vacuum-tube flip-flops, delay lines, electrostatic stores, magnetic
cores, and semiconductor memories offered different compromises in speed,
density, power, and reliability. The modern register is easy to draw as a
box precisely because generations of circuit engineering are hidden inside
that box.

### CPU path

State divides one continuous electrical machine into understandable steps:

```text
PC holds address N
    → instruction and next-PC logic settle
    → PC captures address N+1
    → the next interval begins from a durable value
```

Without storage boundaries, “instruction N” and “instruction N+1” would not
have clean architectural moments.

---

## 2. Bit cells are physical mechanisms, not architectural memory

Week 1 treated a register and memory as boxes that preserve bits so a minimal
CPU could fetch instructions and retain results. We can now unpack those
boxes one level without turning this into a memory-circuit design course.

A **bit cell** is a circuit that physically retains one bit under specified
electrical conditions. **Architectural memory** is the programmer-visible
model: addressed bytes or words, load/store behavior, ordering rules, and
faults. Between those layers are register files, caches, controllers, buses,
error correction, coherence, address translation, and packaging. An
architectural load does not directly select “a transistor”; it initiates a
memory-system operation that may encounter several structures.

### 6T SRAM: feedback stores the state

A common six-transistor SRAM cell contains:

- two cross-coupled CMOS inverters, four transistors total, whose outputs
  reinforce the stable states `Q=1, Q_bar=0` or `Q=0, Q_bar=1`;
- two access transistors controlled together by a **word line**;
- two complementary **bit lines**, conventionally `BL` and `BL_bar`, shared
  by many cells in a column.

```text
                 cross-coupled inverters
                    Q ◀──────▶ Q_bar
                    │             │
BL ── access ───────┘             └────── access ── BL_bar
          ▲                                  ▲
          └──────────── word line ───────────┘
```

With the word line inactive, the access transistors isolate the cell and the
cross-coupled inverters hold its state while powered. To write, peripheral
circuits drive `BL` and `BL_bar` to opposite levels strongly enough that
asserting the word line forces the cell into the requested stable state.

For a read, the two bit lines are typically precharged. The word line then
connects the selected cell. The side storing zero causes one bit line to
discharge slightly relative to the other; a sense amplifier detects that
small differential rather than waiting for the cell to drive a long,
capacitive column to a full logic level. The read must not overpower and flip
the cell. Its cross-coupled feedback retains and restores full internal logic
levels after the small read disturbance and after the word line closes.

This is SRAM—**static** RAM—because feedback retains the bit without periodic
refresh while power and operating conditions remain valid. “Static” does not
mean nonvolatile, zero-power, or zero-latency.

### DRAM: charge stores the state

A simplified DRAM cell uses one access transistor and one capacitor. The word
line controls the transistor; the bit line connects the capacitor to sensing
and write circuitry:

```text
bit line ── access transistor ── storage capacitor
                    ▲
                 word line
```

The stored charge is approximately:

```text
Q = C V
```

A charged versus less-charged capacitor represents the bit. Because the cell
capacitor is tiny, reading shares its charge with a much larger bit line and
creates only a small voltage change. A sense amplifier detects and amplifies
that change to a full logic level. This sensing disturbs the original charge,
so an ordinary DRAM read is internally **destructive** and the sensed value is
written back as part of activation/restoration.

Charge also leaks through real devices. An RC picture gives useful intuition:
a finite leakage path gradually changes capacitor voltage. DRAM controllers
therefore refresh rows before leakage makes their state ambiguous. Real DRAM
adds row buffers, banks, activation/precharge commands, timing rules, and
refresh scheduling; the one-transistor/one-capacitor cell is only its storage
core.

### Why the hierarchy uses different storage

No single implementation simultaneously gives minimum latency, many ports,
maximum density, and minimum cost per bit:

```text
CPU registers   few bits, very close and heavily ported; lowest access latency,
                but expensive in transistors, wiring, clock load, and area
cache SRAM      denser arrays with decoder/sense circuitry; fast enough to
                keep recently used blocks near the core, but costly per bit
main DRAM       much denser 1T1C cells and lower cost per bit; access requires
                array commands, sensing/restoration, and sometimes refresh
```

These are organization-level tendencies, not guarantees for every product.
Registers are often built from flip-flops or specialized register-file cells,
not ordinary 6T cache SRAM cells. Caches contain SRAM arrays plus tags,
comparators, replacement state, and control. Main memory contains far more
than isolated DRAM capacitors.

This distinction explains a cache miss physically without confusing layers.
The requested architectural address was not found in a nearby cache's SRAM
tags/data, so the cache controller obtains a whole block from a lower level,
potentially reaching DRAM with its longer command, sensing, and restoration
path. The CPU may stall or continue other work, then retry or complete the
load after the block is installed. The software-visible operation is still a
load; the miss is a microarchitectural event caused by where a copy was—or
was not—present.

---

## 3. A latch is open for a level; a flip-flop samples around an edge

### Level-sensitive latch

A D latch has a data input, an enable, and an output:

```text
enable active    Q follows D after propagation delay
enable inactive  Q holds the last accepted value
```

While enabled, the latch is **transparent**. Changes can pass through it.
When enable becomes inactive, feedback preserves the last value.

Latches are legitimate design elements. A disciplined latch-based pipeline
can borrow time between phases. The cost is more subtle reasoning about
transparency, overlapping phases, races, and time borrowing.

### Edge-triggered flip-flop

An edge-triggered D flip-flop accepts data only around an active clock edge:

```text
before edge   D must arrive early enough
at edge       the internal decision is made
after edge    Q changes after clock-to-Q delay
between edges later D changes do not immediately change Q
```

Its circuit is not an infinitely fast snapshot device. “At the edge” is a
digital abstraction over a small analog sampling aperture.

### Why CPU diagrams favor flip-flops

Edge-triggered registers produce a simple pipeline model:

```text
launch edge N
    → register output changes
    → combinational path settles
capture edge N+1
```

Real processors also contain SRAM arrays, level-sensitive structures, clock
gating, multiple clock domains, and specialized circuits. The edge model is
a foundation, not a claim that every state bit uses one identical cell.

---

## 4. What the clock does—and does not do

The clock provides coordinated sampling boundaries. It lets many state
elements distinguish old state from new state.

It does not:

- perform arithmetic;
- push bits through gates;
- guarantee that data arrived in time;
- make an asynchronous input synchronous;
- erase transistor delay or analog behavior.

Between edges, combinational circuitry responds continuously:

```text
register outputs
    → decoder / mux / ALU / compare / address logic
    → candidate register inputs
```

At an enabled edge, selected storage elements attempt to capture those
candidates. A synchronous design works because every timed path is checked
against the clock contract.

This approach became dominant because it converts a global continuous-time
problem into repeated local obligations. If all relevant paths meet their
constraints under the specified process, voltage, temperature, and clock
conditions, designers can reason cycle by cycle.

---

## 5. Setup timing: can data arrive before the next edge?

Consider two registers separated by combinational logic:

```text
launch register ──▶ logic and routing ──▶ capture register
       edge N                              edge N+1
```

Important quantities are:

- **clock-to-Q (`t_clk→Q`)** — delay from the source's active clock edge until
  its output is valid;
- **combinational delay (`t_comb`)** — propagation through logic and routing;
- **setup time (`t_setup`)** — how long the destination input must be stable
  before its active edge.

For an ideal common clock, a first setup equation is:

```text
T_clock ≥ t_clk→Q,max + t_comb,max + t_setup
```

Suppose:

```text
t_clk→Q,max = 80 ps
t_comb,max  = 720 ps
t_setup     = 100 ps
```

The period must be at least 900 ps before adding clock uncertainty and other
margins. The corresponding ideal upper frequency is about 1.11 GHz.

This explains a central CPU trade-off. If a register-file-to-ALU-to-writeback
path is too long, designers can:

- lower frequency;
- simplify or restructure logic;
- improve cells, placement, or routing;
- reduce fan-out;
- insert a pipeline register.

Pipelining can shorten the work per stage and raise clock frequency, but it
adds latency and creates hazard, forwarding, stall, and branch-recovery
problems. Timing pressure changes architecture.

---

## 6. Hold timing: can old data remain stable after this edge?

Setup asks about arrival before a future capture. Hold asks whether the old
value remains stable briefly after the current capture edge.

```text
                 active edge
                      │<-- t_hold -->│
data must remain stable through this interval
```

A simplified same-clock hold condition is:

```text
t_clk→Q,min + t_comb,min ≥ t_hold
```

The minimum delays matter because a hold failure is a race: newly launched
data reaches the destination too quickly and disturbs the decision associated
with the same nominal edge.

Slowing the clock normally does not repair this race. The next edge is not
the problem. Physical-design tools may add delay, change cells, or reroute a
short path.

### Setup and hold form a sampling aperture

```text
stable old value   setup window | edge | hold window   stable new value
─────────────────────────────────↑──────────────────────────────────────
```

The input should not change within that aperture. This is evidence that a
flip-flop does not decide at a dimensionless instant; internal nodes need
time to amplify a small voltage difference into a full logic level.

---

## 7. Clock skew and jitter spend margin

The clock is distributed through real buffers and wires. It does not arrive
at every register simultaneously.

**Clock skew** is the difference in arrival time between clock endpoints in a
cycle. If the capture clock arrives later than the launch clock, a setup path
may gain time while a hold path loses margin. The opposite arrival relation
can reverse those effects. Tool sign conventions vary, so reason from actual
arrival times.

**Clock jitter** is variation of an edge from its ideal time across cycles.
Oscillator and PLL noise, supply variation, and coupling all contribute.

```text
ideal:   |----------|----------|----------|
actual:  |---------|------------|---------|
```

Skew is primarily a spatial arrival difference. Jitter is temporal edge
variation. Static timing analysis incorporates clock uncertainty and checks
paths across operating corners. An untimed RTL waveform cannot establish
that silicon meets either setup or hold.

---

## 8. Metastability is unresolved analog competition

If data changes inside the sampling aperture, the storage element can enter
a delicately balanced internal condition. Its output may remain near a
switching threshold longer than normal before tiny device imbalance and
noise drive it toward a valid rail.

```text
voltage
 VDD |                    ______ valid HIGH
     |                 __/
 mid |───────────────•       delayed analog resolution
     |                \__
   0 |                   ______ valid LOW
     +------------------------------------ time
```

Metastability is:

- an analog condition in a bistable circuit;
- probabilistic in occurrence and resolution duration;
- possible whenever an input has no guaranteed timing relationship to the
  sampling clock;
- capable of violating timing at downstream logic.

It is not:

- a stable third Boolean value;
- the same thing as a SystemVerilog `X`;
- guaranteed to resolve to the old value or the new value;
- eliminated by simulation, a faster simulator, or a type declaration.

An RTL simulator models discrete values and scheduling rules. It may show
`X` when drivers conflict or a value is unknown, but it does not solve the
analog differential equations of a metastable storage node. Conversely, a
clean zero/one simulation does not prove metastability safety.

### Why ordinary synchronous paths avoid it

Static timing analysis tries to prove that register-to-register paths meet
setup and hold across specified corners. Asynchronous inputs cannot satisfy
that proof merely by declaration because their transitions are unrelated to
the destination clock.

Examples relevant to systems programmers include:

- an external interrupt pin entering a CPU clock domain;
- a device status signal crossing into an interconnect clock;
- reset deassertion;
- communication between CPU, memory-controller, PCIe, and peripheral clock
  domains;
- GPIO or board-level signals.

---

## 9. A synchronizer buys resolution time

For a slowly changing single-bit level, a common structure is two destination
clocked flip-flops in series:

```text
async input ──▶ FF1 ──▶ FF2 ──▶ destination logic
                  destination clock
```

FF1 can become metastable. FF2 samples one cycle later, giving FF1 most of a
clock period to resolve before its value is consumed. There is still a
nonzero probability that resolution takes too long. The synchronizer reduces
that probability to an engineered level.

Do not place combinational destination logic between FF1 and FF2. Keep the
stages physically suitable and tell CDC/timing tools that this is an
intentional synchronizer so the path receives appropriate treatment.

### MTBF intuition

A common qualitative model has the shape:

```text
MTBF grows approximately exponentially with available resolution time
MTBF falls as destination clock frequency rises
MTBF falls as asynchronous transition rate rises
```

Library characterization supplies device-dependent constants. One extra
cycle can improve MTBF enormously because resolution probability has an
exponential tail.

MTBF is a statistical population/time measure, not a schedule. It does not
say which event fails or promise that this boot is safe. System reliability
must account for synchronizer count, product population, operating lifetime,
voltage, temperature, and required failure rate.

### What two flip-flops do not solve

They do not automatically make these crossings correct:

- **narrow pulses** — the destination may never sample the asserted level;
- **multi-bit buses** — bits can settle on different cycles, producing a word
  that never existed at the source;
- **counters** — several binary bits may change together;
- **data plus valid without a protocol** — control and payload can lose
  alignment;
- **reset release** — asynchronous deassertion can violate recovery/removal
  requirements.

Typical solutions depend on the transfer:

```text
single persistent level     two-flop synchronizer
event/pulse                 toggle, pulse-stretch, or handshake
multi-bit payload           handshake with stable data
continuous stream           asynchronous FIFO
cross-domain counter        Gray-coded representation plus synchronization
reset                       asynchronous assert, synchronized deassert
```

These are patterns, not substitutions for CDC analysis. A protocol must state
when data becomes stable, when it may change again, and how acknowledgement
works.

---

## 10. Why a kernel or driver engineer should care

CDC is normally implemented below software, but its contract appears in
software-visible behavior:

- status bits may be delayed by synchronizer stages;
- MMIO register documentation may require polling or handshakes;
- reset and clock-enable sequences may require bounded waits;
- hardware event counters may use snapshot protocols;
- device FIFOs bridge independently clocked producer and consumer logic;
- an interrupt line is synchronized before internal handling;
- power-management transitions can stop or change clocks.

`volatile`, barriers, locks, and C atomics solve software/compiler/CPU
ordering problems under their own contracts. They do not repair a broken
hardware clock-domain crossing. Conversely, a correct synchronizer does not
replace the memory-ordering rules software needs.

This distinction matters when debugging “impossible” intermittent failures:

```text
software race?
memory-ordering bug?
device protocol violation?
reset/clock sequencing error?
hardware CDC defect?
```

The symptoms can look similar while the responsible layer differs.

---

## 11. Timing exercises

### Exercise A — setup budget

A CPU stage has:

```text
t_clk→Q,max       70 ps
decode + mux     260 ps
ALU              480 ps
routing          110 ps
t_setup           80 ps
clock uncertainty 60 ps
```

1. Calculate the minimum period under this simplified model.
2. Calculate the corresponding ideal frequency.
3. If a pipeline register splits the logic into 430 ps and 420 ps stages,
   what new non-logic overhead appears in both stages?
4. Why does doubling the number of stages not automatically double useful
   performance?

### Exercise B — hold race

Use:

```text
t_clk→Q,min = 35 ps
t_comb,min  = 10 ps
t_hold      = 55 ps
```

1. Is the simplified hold condition met?
2. Why is lowering frequency not the direct fix?
3. Name one physical change that could add minimum-path delay.

### Exercise C — crossing choice

Choose a mechanism and justify it:

1. a board button that remains pressed for milliseconds;
2. a one-source-cycle completion pulse entering a slower domain;
3. a 64-bit DMA descriptor plus a ready indication;
4. a continuous stream between unrelated clocks;
5. a binary counter observed from another domain.

Do not answer every case with “two flip-flops.”

---

## 12. Mastery evidence

### Explain

Without notes:

- explain how feedback permits remembered state;
- distinguish 6T SRAM feedback from DRAM capacitor charge and restoration;
- distinguish either bit cell from architectural memory;
- connect register/cache/DRAM tradeoffs to a cache miss;
- distinguish latch transparency from flip-flop edge sampling;
- explain why the clock coordinates capture but does not perform computation;
- derive the simple setup and hold inequalities;
- distinguish skew from jitter;
- explain metastability without calling it `X` or a third logic level;
- explain what synchronizer MTBF means and does not mean;
- explain why a multi-bit bus needs a protocol.

### Draw

Draw:

1. cross-coupled restoring gates with two stable states;
2. a transparent latch interval and an edge-triggered capture;
3. launch register → combinational CPU path → capture register;
4. setup and hold windows around one edge;
5. two-flop single-bit synchronizer;
6. asynchronous FIFO as two clock domains separated by storage and pointer
   transfer—block-level only.

### Observe

There is intentionally no HDL metastability lab today. Preserve:

- your setup and hold calculations;
- a timing diagram annotated with data arrival and clock arrival;
- one CDC classification from a real SoC/device block diagram if available;
- a statement of why a digital waveform cannot prove metastability MTBF.

This absence is part of the lesson: do not manufacture false evidence with an
RTL `X`.

### CPU checkpoint

Answer:

> How do setup, hold, clock-to-Q, clock distribution, and metastability limit
> the claim that “all CPU registers update at the clock edge”?

A strong answer separates the architectural abstraction from the electrical
contract that makes the abstraction reliable.

Also be able to state why that register abstraction does not imply that cache
SRAM, main-memory DRAM, and CPU registers use the same bit cell.

---

## Why? notebook

1. Why does arbitrary feedback oscillate while designed feedback can store?
2. Why can a latch pass more than one transition while transparent?
3. Why is a flip-flop's edge really a sampling aperture?
4. Why does setup use maximum delay while hold uses minimum delay?
5. Why can lowering frequency help setup but not an ordinary same-edge hold
   violation?
6. Why can useful clock skew help one check and hurt another?
7. Why is metastability inevitable at asynchronous boundaries?
8. Why does another synchronizer stage improve probability rather than
   guarantee correctness?
9. Why can independently synchronizing every bus bit create an incoherent
   word?
10. Why can a software barrier not fix a physical CDC error?

---

## References used selectively

- David Money Harris and Sarah L. Harris, *Digital Design and Computer
  Architecture*, chapters on sequential logic and synchronous timing, for
  latch/flip-flop behavior and register-to-register timing.
- Neil H. E. Weste and David Harris, *CMOS VLSI Design*, chapters on
  sequential circuits, clocking, and synchronization, for the physical timing
  and metastability model.
- Clifford E. Cummings, “Clock Domain Crossing (CDC) Design & Verification
  Techniques Using SystemVerilog,” SNUG Boston 2008, for CDC structures and
  failure modes:
  <http://www.sunburst-design.com/papers/CummingsSNUG2008Boston_CDC.pdf>
- Intel, *Metastability in FPGAs*, for MTBF intuition and synchronizer
  resolution time:
  <https://www.intel.com/content/www/us/en/docs/programmable/683082/current/metastability.html>
- AMD, *Vivado Design Suite User Guide: Design Analysis and Closure
  Techniques (UG906)*, sections on clock-domain crossing and timing analysis:
  <https://docs.amd.com/r/en-US/ug906-vivado-design-analysis>

**Next bridge:** now that a register-to-register transfer has a physical
timing contract, Day 6 uses one small RTL design to expose how engineers
describe that transfer as an `always_ff` register abstraction—not as a 6T
SRAM or DRAM cell—simulate its scheduled behavior, and inspect a waveform,
then returns immediately to the CPU datapath.
