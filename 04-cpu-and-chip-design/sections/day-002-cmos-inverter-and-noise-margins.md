# Day 2 — CMOS Inverter and Noise Margins

**Target time:** approximately 2–3 hours

- Reliable-gate model and CPU bridge: 70–80 minutes
- Prediction and drawing exercises: 25–35 minutes
- Optional ngspice observation: 45–60 minutes
- Explain/Draw/Observe/Build checkpoint: 20–30 minutes

> Software eventually changes voltages. Why do those imperfect analog
> voltages keep behaving like clean bits after passing through millions of
> gates?

## Why this day exists

Day 1 replaced the ideal switch with a useful systems-level MOSFET model:
gate voltage changes channel conductivity; current moves charge; charge
changes node voltage. One nMOS plus a resistor could invert a signal, but it
had an asymmetric pull-up and wasted steady power in one state.

The CMOS inverter solves the next problem by pairing complementary devices:

```text
input LOW  → pMOS pulls output toward VDD
input HIGH → nMOS pulls output toward GND
```

That sentence is only the settled-state story. A reliable gate also needs:

- a transition region between LOW and HIGH;
- enough gain to restore degraded inputs;
- voltage ranges that tolerate noise;
- enough current to charge and discharge its load on time;
- acceptable static and switching energy.

These are not side concerns for analog specialists. They are why a CPU can
treat physical voltages as bits, why clock frequency and fan-out are limited,
why power rises with activity, and why lower supply voltages make reliable
design harder.

The path back to software is direct:

```text
instruction changes control/data state
    → selected gate inputs change
    → MOS networks source or sink current
    → internal capacitances gain or lose charge
    → voltages cross receiving-gate limits
    → later pipeline/register state changes
```

We need enough circuit behavior to make that path real. We do not need to
design a fabrication process or a production standard-cell library.

---

## 1. Build the complementary inverter

Connect a pMOS between the supply and output, and an nMOS between output and
ground. Tie both gates to the input:

```text
                 VDD
                  │
                pMOS
VIN ──────────────┤
                  ├──── VOUT ─── load capacitance
VIN ──────────────┤
                nMOS
                  │
                 GND
```

The shared output has two controlled paths:

- the **pull-up network** can add charge and drive `VOUT` toward `VDD`;
- the **pull-down network** can remove charge and drive `VOUT` toward ground.

### Input LOW

With `VIN` near ground:

- pMOS is on because its gate is low relative to its source at `VDD`;
- nMOS is off or weak;
- the output charges toward `VDD`;
- the next gate receives a strong HIGH.

### Input HIGH

With `VIN` near `VDD`:

- pMOS is off or weak;
- nMOS is on;
- the output discharges toward ground;
- the next gate receives a strong LOW.

In either stable state, the ideal model has no conducting path all the way
from `VDD` to ground. This is the central static-power advantage of CMOS.
Real devices leak, so “no path” means no intended strong DC path.

### Why this is inversion

The transistor network does not know logical NOT. It responds locally to
voltages. We call the behavior inversion because the settled output ranges
map to the opposite logical values:

| Input range | Dominant path | Output range | Logical relation |
|---|---|---|---|
| near GND | pMOS pull-up | near VDD | LOW becomes HIGH |
| near VDD | nMOS pull-down | near GND | HIGH becomes LOW |

Logic is the abstraction over repeatable electrical behavior.

---

## 2. The middle is a real operating region

Suppose `VIN` rises slowly from 0 V to `VDD`.

1. Initially, pMOS is strong and nMOS is weak; output is HIGH.
2. As input enters the middle, nMOS strengthens while pMOS has not fully
   turned off.
3. Both devices conduct for part of the transition.
4. The output falls rapidly through its middle range.
5. Finally, pMOS becomes weak and nMOS holds the output LOW.

There is no instant at which a physical “0” token becomes a “1” token. Input
and output are continuous voltages. The steep middle of the inverter's
response is what permits a digital interpretation.

### Threshold caveat, repeated because it matters

Do not identify the inverter's switching region with one transistor's
threshold voltage. The circuit response depends on:

- nMOS and pMOS strengths;
- both device thresholds;
- supply voltage;
- sizing and load;
- process, voltage, and temperature variation.

Production timing and noise guarantees come from characterized libraries
across operating corners. Our generic ngspice model illustrates mechanisms;
it is not a CPU process model.

---

## 3. The voltage transfer characteristic

The **voltage transfer characteristic**, or VTC, is the settled output
voltage for each slowly varied input voltage:

```text
VOUT
 VDD ──────────┐
               │
               └──── steep transition
                    └──────── 0
     0                        VDD   VIN
```

The inverter is most useful when:

- low inputs produce outputs close to `VDD`;
- high inputs produce outputs close to ground;
- the transition is steep;
- the next gate can classify the output safely.

The VTC is a DC view. It does not show propagation time, glitches, or the
energy required to move charge. Those need transient reasoning.

### Predict the sweep

Before the optional lab, sketch `VOUT` against `VIN` and mark:

- `VOH`: guaranteed or characterized output HIGH level;
- `VOL`: guaranteed or characterized output LOW level;
- `VIL`: largest input guaranteed to be accepted as LOW;
- `VIH`: smallest input guaranteed to be accepted as HIGH;
- the uncertain input interval between `VIL` and `VIH`.

In textbook analysis, `VIL` and `VIH` are often associated with points where
the VTC slope is `-1`. That is a useful way to extract margins from a smooth
curve. Datasheet or standard-cell limits, however, are specifications backed
by characterization; do not substitute a plot from a generic model for a
manufacturer guarantee.

---

## 4. Noise margins: room for imperfection

A sending gate promises output levels. A receiving gate promises input
interpretation limits. The gaps are the **noise margins**:

```text
NML = VIL - VOL
NMH = VOH - VIH
```

Example—not a universal CMOS specification:

```text
VDD = 3.3 V
VOL = 0.1 V
VIL = 1.0 V       → NML = 0.9 V
VIH = 2.2 V
VOH = 3.2 V       → NMH = 1.0 V
```

If a LOW output of at most 0.1 V picks up 0.4 V of disturbance, the receiving
input is still only 0.5 V, safely below `VIL`. The margin is not permission to
ignore signal integrity; it is a compatibility budget between guaranteed
ranges.

### What “noise” can mean in a system

Noise is not only random fuzz from the environment. A digital path can be
disturbed by:

- voltage drop in power and ground distribution;
- coupling from nearby switching wires;
- simultaneous switching current;
- package and board effects;
- slow edges that spend too long in the uncertain region;
- process, voltage, and temperature variation;
- a driver overloaded by excessive fan-out or wire capacitance.

At the software level these effects are usually hidden by hardware margins
and timing constraints. When they are not hidden, symptoms can look like
rare bit errors, timing failures, machine-check events, or total instability.
Software observes the consequence, not a friendly message saying “noise
margin exceeded.”

---

## 5. Restoration: how analog voltages become reusable logic

Suppose an upstream gate's HIGH arrives at 2.9 V instead of an ideal 3.3 V.
If 2.9 V is safely above the receiving inverter's `VIH`, the inverter turns
its nMOS on strongly and drives its output close to ground. A following
inverter can then drive a fresh output close to `VDD`.

```text
degraded but valid HIGH
        ↓ receiving inverter
strong LOW near ground
        ↓ another inverter
strong HIGH near supply
```

This is **logic-level restoration**. The gate does not amplify the symbolic
meaning of a bit. Its transistor network uses energy from the power supply to
produce a new output near a rail.

Three conditions are hidden inside “valid input”:

1. **Voltage:** the input lies inside a guaranteed range.
2. **Time:** it becomes valid early enough and remains valid long enough for
   the receiving sequential element.
3. **Drive/load:** the source can charge or discharge the path's capacitance
   quickly enough.

Noise margins address the first. Static timing analysis addresses much of the
second and third. Later lessons will connect delay to setup/hold constraints
and clocked state.

### Restoration is not error correction

An inverter can restore an electrically degraded but correctly classified
level. If a disturbance pushes an input across the wrong guaranteed region
for long enough, the gate may faithfully produce the wrong logic value.
Restoration does not recover lost intent.

---

## 6. Static power, dynamic power, and the transition interval

### Ideal stable-state picture

For a settled LOW or HIGH input, one transistor is off. The simple CMOS model
therefore predicts almost no supply current except leakage.

### Dynamic charging energy

An output and its connected wires/gates behave partly like capacitance.
Charging an effective capacitance `C` from 0 to `VDD` draws energy from the
supply. A commonly used switching-power estimate is:

```text
Pdynamic ≈ α C VDD² f
```

where:

- `α` is the activity factor—how often the node makes an energy-consuming
  transition;
- `C` is effective switched capacitance;
- `VDD` is supply voltage;
- `f` is an associated switching/clock rate.

Conventions can move a factor between `α` and the energy-per-transition
definition. The durable result is the quadratic dependence on supply voltage
and linear dependence on switched capacitance and activity.

The energy stored on the charged node is:

```text
Ecapacitor = ½ C VDD²
```

When the nMOS later discharges the node, that stored energy is mostly
dissipated as heat in the pull-down path. The power supply also lost energy
in the pull-up during charging.

### Short-circuit current

During a finite input transition, nMOS and pMOS may both conduct. For a short
interval there is a direct path:

```text
VDD → pMOS → nMOS → GND
```

This **short-circuit current** adds energy beyond charging the load. It grows
with conditions such as slow input edges and device sizing. It is not the
same as leakage, and it does not imply a defective literal short circuit.

### Leakage

When the gate is logically idle, real transistors still leak. Relevant
mechanisms vary by process and operating point; a systems programmer mainly
needs the consequence:

```text
many transistors × small current each = meaningful idle power
```

Temperature and voltage affect leakage. Power-management features therefore
include clock gating, power gating, voltage/frequency scaling, and low-power
states. Clock gating reduces unnecessary switching; it does not remove all
leakage. Power gating can reduce leakage in a domain but adds wake-up latency
and state-retention concerns.

---

## 7. Delay: charge cannot move instantaneously

An inverter must charge or discharge its effective load:

```text
load = transistor capacitances + wire capacitance + receiving gates
```

A useful first-order idea is:

```text
delay grows when load capacitance grows
delay shrinks when available drive current grows
```

The HIGH-to-LOW and LOW-to-HIGH delays can differ because nMOS and pMOS drive
strengths differ. A larger transistor can provide more current but also adds
input and diffusion capacitance. Making every gate huge merely moves the
problem upstream and consumes area and energy.

**Fan-out** is the load presented by downstream inputs. Gate inputs draw
little steady DC current, but every input adds capacitance that must be
charged and discharged. Thus “MOS inputs draw no current” cannot justify
unlimited fan-out at speed.

This is enough device knowledge for the systems bridge:

```text
longer wire / more gate inputs
    → more capacitance
    → more charge per transition
    → more delay and dynamic energy
    → lower feasible clock or extra pipeline stages/buffers
```

---

## 8. Optional lab — VTC, margins, current, and delay

The small lab is:

`04-cpu-and-chip-design/challenges/day-002-cmos-inverter-lab.cir`

It uses generic educational MOS models and performs:

1. a DC input sweep producing the inverter VTC, slope magnitude, and supply
   current;
2. a transient run producing input/output voltages and supply current;
3. propagation-delay and average-current measurements in the text log.

No GUI or external model file is required.

### Predict

Record answers before running:

1. At `VIN = 0`, which device is on and where should `VOUT` settle?
2. At `VIN = VDD`, which device is on and where should `VOUT` settle?
3. Where should supply current be largest during the DC sweep?
4. During a rising input edge, does output rise or fall?
5. If output capacitance doubles, what happens to propagation delay and
   energy per full output transition?
6. Is static supply current in this model exactly representative of hardware?

### Simulate

From the repository root:

```bash
mkdir -p /tmp/cmos-inverter-lab
ngspice -b \
  -o /tmp/cmos-inverter-lab/ngspice.log \
  04-cpu-and-chip-design/challenges/day-002-cmos-inverter-lab.cir
```

Outputs:

```text
inverter-vtc.dat         VIN, VOUT, gain magnitude, supply current
inverter-transient.dat   time, VIN, VOUT, supply current
ngspice.log              measured levels, delay, and average current
```

These are whitespace-separated numeric text files. With `wr_singlescale`,
ngspice writes one analysis scale followed by the requested vectors; the
header names each column. They can be inspected without graphical tools:

```bash
less /tmp/cmos-inverter-lab/inverter-vtc.dat
less /tmp/cmos-inverter-lab/inverter-transient.dat
cat /tmp/cmos-inverter-lab/ngspice.log
```

Optional VTC plot:

```bash
gnuplot -persist <<'PLOT'
set xlabel "VIN (V)"
set ylabel "VOUT (V)"
set grid
plot "/tmp/cmos-inverter-lab/inverter-vtc.dat" \
  using 2:3 with lines title "CMOS inverter", \
  x with lines title "VOUT=VIN"
PLOT
```

### Observe

Use the DC data to identify:

- output HIGH and LOW plateaus;
- the steep transition region;
- approximate points where gain magnitude crosses 1;
- the region of elevated supply current.

For this educational curve, estimate:

```text
VIL = lower-VIN crossing where |dVOUT/dVIN| ≈ 1
VIH = upper-VIN crossing where |dVOUT/dVIN| ≈ 1
NML ≈ VIL - VOL
NMH ≈ VOH - VIH
```

State that these are estimates from this model, not guaranteed logic-family
specifications.

Use the transient data and log to identify:

- finite input and output slopes;
- inversion;
- output delay after an input crossing;
- a current pulse during switching;
- asymmetry, if any, between rising and falling output delay.

### Explain

Answer:

1. Why does the VTC have two plateaus and a steep middle?
2. Why can a degraded valid input produce a restored output near a rail?
3. Why is supply current larger in the transition region?
4. Why does capacitance affect delay although the next gate draws almost no
   steady input current?
5. Why can this lab teach mechanisms but not certify a CPU voltage limit?

---

## 9. From one inverter to CPU behavior

An inverter is not interesting because software frequently asks for logical
NOT in isolation. It is interesting because complementary pull-up/pull-down
networks generalize:

- NAND and NOR gates arrange series and parallel paths;
- larger gates implement terms used in decoders and multiplexers;
- gates compose into adders, comparators, register controls, and clock logic;
- clocked circuits capture restored levels as architectural and
  microarchitectural state.

Consider a machine instruction:

```text
add x5, x6, x7
```

At the ISA level it means addition of register values. Below that abstraction:

```text
instruction bits
  → decoder control voltages
  → mux paths select operands and ALU operation
  → carry and sum gate networks switch
  → many internal capacitances charge/discharge
  → result voltages settle
  → destination register captures the result on a clock edge
```

The software meaning “add” exists at the architectural layer. Individual
transistors respond only to local voltages. Reliable restoration and timing
allow those layers to coexist.

### Why activity affects CPU power

A tight workload that repeatedly changes wide datapaths, caches, and
execution units causes more nodes to switch than an idle core. More switching
raises dynamic power. The relationship is not simply “one instruction costs
one fixed energy”:

- instructions activate different units;
- operand values alter switching activity;
- cache misses engage different structures;
- out-of-order execution overlaps work;
- frequency and voltage may change;
- clock and power gating alter active domains.

Performance counters and model-specific energy interfaces can expose parts of
this behavior later, but the physical starting point is still `C V²` energy
associated with moving charge.

### Why the CPU uses abstraction

Systems software cannot reason transistor-by-transistor. Hardware designers
characterize gates and paths, impose timing constraints, and verify that
valid outputs reach receiving registers under supported conditions. The ISA
then promises architectural behavior to software.

That contract has boundaries. Overclocking, undervolting, overheating, or
electrical faults can violate the assumptions beneath the ISA and produce
incorrect execution. The abstraction is engineered, not magical.

---

## 10. Explain, Draw, Observe, Build

### Explain

Without notes:

1. Explain both stable states of a CMOS inverter.
2. Explain why the transition region cannot be removed.
3. Distinguish transistor threshold, inverter transition, and logic input
   limits.
4. Derive `NML` and `NMH` from sending and receiving voltage guarantees.
5. Distinguish dynamic, short-circuit, and leakage power.
6. Explain restoration using charge, supply energy, and output rails.
7. Connect fan-out to capacitance, delay, and dynamic energy.

### Draw

Draw one page containing:

```text
CMOS inverter transistor network
        ↓
VTC with VOH, VOL, VIL, VIH
        ↓
noise margins
        ↓
two inverters showing restoration
        ↓
gate network → ALU path → destination register
```

Add arrows showing charge entering and leaving the output node.

### Observe

If you run the optional lab, submit:

- predictions made before simulation;
- VTC and transient text files plus log;
- estimated `VIL`, `VIH`, `NML`, and `NMH`;
- observed propagation delays;
- one current observation;
- two reasons the numbers are model-specific.

If ngspice is unavailable, draw the expected VTC and transient waveforms,
then annotate the same quantities. The conceptual checkpoint does not depend
on simulator installation.

### Build

Copy the lab into `/tmp`:

```bash
cp 04-cpu-and-chip-design/challenges/day-002-cmos-inverter-lab.cir \
  /tmp/cmos-inverter-lab/experiment.cir
```

Double `CLOAD`, predict both delay directions and switching current behavior,
then rerun the copy. Explain the result with:

```text
more capacitance → more charge required for the same voltage change
```

Do not tune transistor dimensions for an “optimal” circuit. The learning goal
is the system consequence of load, not professional cell design.

---

## 11. Mastery checkpoint

Close the lesson and answer:

> How does a CMOS gate turn continuous, imperfect voltages into reliable
> logic that can carry an instruction through a CPU?

A complete answer should include:

- complementary pull-up and pull-down paths;
- stable outputs driven close to supply rails;
- a steep but finite VTC transition;
- defined input and output ranges;
- noise margins;
- gain and restoration using power-supply energy;
- capacitance and finite propagation delay;
- dynamic, short-circuit, and leakage power;
- timing constraints before a register captures state;
- the ISA as a higher-level contract built on these physical guarantees.

If the answer says “the transistor converts voltage to 0 or 1,” refine it.
The circuit produces voltages; designers define and enforce the ranges that
software can safely treat as bits.

---

## Mental model at the end

```text
Software requests an architectural operation.
The implementation causes selected electrical nodes to switch.
CMOS networks move charge toward a supply rail or ground.
Steep gate response restores valid but imperfect inputs.
Noise margins separate guaranteed output and input ranges.
Capacitance and finite current create delay and energy cost.
Registers capture settled logic under clock timing constraints.
The ISA hides this machinery while its guarantees hold.
```

---

## Why? notebook

1. Why does complementary pull-up/pull-down operation reduce ideal static
   power?
2. Why are both devices partly on during many real transitions?
3. Why is a steep VTC useful for restoration?
4. Why are `VIL` and `VIH` different numbers?
5. Why is the interval between them not a third logic value?
6. Why does restoration need energy from the supply?
7. Why is restoration unable to recover a signal that crossed into the wrong
   valid range?
8. Why does doubling supply voltage increase dynamic energy by more than
   double?
9. Why can an “idle” chip still consume power?
10. Why does fan-out matter if MOS gates have negligible steady input current?
11. Why can lowering voltage save energy but reduce timing/noise headroom?
12. Where, along an instruction's path, do analog voltages stop mattering?
    Is there really one boundary?

---

## References used selectively

Systems-oriented foundations:

- David A. Patterson and John L. Hennessy, *Computer Organization and Design:
  The Hardware/Software Interface*, digital-logic and processor-datapath
  chapters. Used for the bridge from gates to datapath state and ISA-visible
  behavior.
- Neil H. E. Weste and David Harris, *CMOS VLSI Design: A Circuits and Systems
  Perspective*, 4th ed., chapters 2–4. Used selectively for inverter VTC,
  noise margins, capacitance, delay, and power—not as a fabrication or
  professional analog-design assignment.
- R. Jacob Baker, *CMOS: Circuit Design, Layout, and Simulation*, 4th ed.,
  inverter chapters. Used to cross-check the complementary switching and VTC
  explanation.

Primary/current tool references:

- ngspice documentation and current manual for batch mode, DC/transient
  analyses, measurements, and text output:
  <https://ngspice.sourceforge.io/docs.html>
- ngspice source and release information:
  <https://sourceforge.net/projects/ngspice/>

Further credible context:

- IEEE IRDS reports, including system integration and power/performance
  scaling context:
  <https://irds.ieee.org/editions>
- Intel, *Intel 64 and IA-32 Architectures Software Developer's Manual*,
  Volume 1, for the architectural execution contract visible to systems
  software:
  <https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html>
- RISC-V International, unprivileged ISA specification, for another clear
  architectural contract independent of circuit implementation:
  <https://docs.riscv.org/reference/isa/unpriv/unpriv-index.html>

The ngspice parameters are generic educational values, not a foundry model or
a characterization of any commercial processor.

**Next bridge:** compose restoring CMOS gates into combinational networks,
then study how capacitance, fan-out, and path depth become propagation delay
that a clocked CPU must respect.
