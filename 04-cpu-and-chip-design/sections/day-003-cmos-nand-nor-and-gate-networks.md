# Day 3 — CMOS NAND, NOR, and Gate Networks

**Target time:** approximately 2–3 hours

- First-principles derivation and sketches: 75–90 minutes
- Small ngspice/Python lab: 35–45 minutes
- CPU bridge and mastery evidence: 30–40 minutes

> A truth table says what a gate must do. What arrangement of physical
> switches makes every row happen—and leaves a clean enough voltage for the
> next gate?

## Why this day exists

Earlier we treated NAND and NOR as Boolean boxes. That was the correct level
for combining gates, but it hid an important question:

> Where does the truth table exist in silicon?

It does not sit in a transistor as a tiny stored table. A MOS transistor reacts
locally to voltages. The Boolean behavior emerges from a **network** of
transistors that creates one conducting route for each required output state.

Today's path is:

```text
input voltage ranges
    → MOS-controlled conducting paths
    → complementary pull-down and pull-up networks
    → restored NAND/NOR outputs
    → standard-cell gates
    → CPU decode, control, arithmetic, and timing paths
```

We will go only as deep as a systems programmer needs. The objective is not to
design a fabrication process or size transistors. It is to understand why a
gate has a particular topology, why high fan-in can be slower, and why a CPU's
logical decisions ultimately involve charging and discharging real nodes.

---

## 1. The historical purpose: make switching logic composable

Relay networks made Boolean structure visible: series contacts express “all
must conduct,” while parallel contacts express “either may conduct.” Relays
were useful, but mechanical movement made them large, slow, noisy, and subject
to wear. Vacuum tubes removed moving contacts but consumed substantial space
and power.

The transistor supplied an electrically controlled solid-state device.
Integrated circuits then placed many devices and their connections on one
piece of material. The decisive gain was not merely replacing a relay with a
smaller switch. Engineers could build **restoring, composable logic**:

```text
one stage accepts imperfect but valid voltage ranges
    → computes through physical conduction
    → drives its output back toward a supply rail
    → the next stage receives another valid logic signal
```

This is why modern digital machines can contain enormous networks rather than
one fragile chain of analog approximations.

Historical attribution needs care. Boole supplied an algebra for two-valued
reasoning. Shannon showed how switching networks could be analyzed with that
algebra. Semiconductor researchers developed practical transistors. CMOS
logic and integrated-circuit manufacturing emerged through many later
contributions. No one step, person, or device was “the CPU.”

---

## 2. The minimum MOSFET model we need

A MOSFET has more terminals and richer analog behavior than a switch. For this
lesson, use a controlled-switch approximation:

### nMOS

```text
gate sufficiently HIGH relative to source → conducting path
gate LOW                              → path mostly off
```

An nMOS device is therefore natural in a path that should conduct when its
Boolean input is `1`.

### pMOS

```text
gate sufficiently LOW relative to source → conducting path
gate HIGH                             → path mostly off
```

A pMOS device is natural in a path that should conduct when its Boolean input
is `0`.

The words “sufficiently,” “mostly,” and “relative” matter. Real MOSFET current
depends continuously on terminal voltages. An off device leaks; an on device
has finite resistance; terminals have capacitance; and switching is not
instantaneous. Digital logic works by designing around those analog facts, not
by making them disappear.

### What not to say

Avoid:

> One transistor is one Boolean gate.

A transistor is a controlled device. A conventional static-CMOS inverter uses
two transistors. A two-input NAND uses four. More complex standard cells may
contain larger networks. The Boolean function belongs to the **topology and
electrical behavior of the whole circuit**, not to one isolated transistor.

---

## 3. Start with the CMOS inverter

A static-CMOS inverter has two complementary routes:

```text
                 VDD
                  │
             pMOS, gate=A
                  │
                  ├──── Y
                  │
             nMOS, gate=A
                  │
                 GND
```

Treat `VDD` as the HIGH rail and ground as the LOW rail.

### Case A = 0

- pMOS is on;
- nMOS is off;
- a route connects `Y` toward `VDD`;
- `Y` becomes HIGH.

### Case A = 1

- pMOS is off;
- nMOS is on;
- a route connects `Y` toward ground;
- `Y` becomes LOW.

So the physical behavior matches:

| A | Y |
|---:|---:|
| 0 | 1 |
| 1 | 0 |

In a stable ideal logic state, there is no intended direct conducting path
from `VDD` to ground. During a transition, both devices can briefly conduct;
we will return to that as short-circuit power on Day 4.

### Two networks, one output

Name the halves:

- **pull-down network (PDN):** nMOS devices that connect the output to ground
  exactly when the Boolean output must be `0`;
- **pull-up network (PUN):** pMOS devices that connect the output to `VDD`
  exactly when the Boolean output must be `1`.

For valid static inputs, complementary CMOS aims for exactly one of those
networks to conduct. If neither conducts, the output can float. If both conduct
strongly, the circuit has contention from supply to ground. Neither is the
ordinary steady state we want.

---

## 4. Series and parallel are physical AND and OR of conduction

The easiest way to derive CMOS gates is to reason first about **conduction**,
not the desired output expression.

Two nMOS devices in series:

```text
Y ──[ nMOS A ]──[ nMOS B ]── GND
```

The complete path exists only when both devices conduct:

```text
PDN conducts = A AND B
```

Two nMOS devices in parallel:

```text
       ┌──[ nMOS A ]──┐
Y ─────┤              ├── GND
       └──[ nMOS B ]──┘
```

Either branch is sufficient:

```text
PDN conducts = A OR B
```

For pMOS, LOW inputs turn devices on. The topology still obeys “series means
all conducting paths are required; parallel means either conducting branch is
enough,” but each pMOS's conduction condition is the inverted input.

This distinction prevents a common mistake. Series does not universally mean
the gate's output is AND. Series describes when a **conducting path** exists.
Whether that path pulls the output HIGH or LOW determines the final Boolean
function.

---

## 5. Derive the two-input NAND

The target is:

```text
Y = ¬(A ∧ B)
```

Do not begin by memorizing a picture. Ask when `Y` must be LOW.

```text
Y = 0 exactly when A = 1 and B = 1
```

The nMOS pull-down must therefore conduct only for `A AND B`. Series nMOS
devices provide that condition:

```text
Y ──[ nMOS A ]──[ nMOS B ]── GND
```

Now derive the complementary pull-up. NAND is HIGH whenever at least one input
is LOW. A pMOS controlled by A conducts when A is LOW, and a pMOS controlled by
B conducts when B is LOW. Put those alternatives in parallel:

```text
          ┌──[ pMOS A ]──┐
VDD ──────┤              ├── Y
          └──[ pMOS B ]──┘

Y ──[ nMOS A ]──[ nMOS B ]── GND
```

Trace all four stable cases:

| A | B | Pull-up path | Pull-down path | Y |
|---:|---:|:---|:---|---:|
| 0 | 0 | both pMOS branches conduct | series path broken | 1 |
| 0 | 1 | A pMOS branch conducts | series path broken | 1 |
| 1 | 0 | B pMOS branch conducts | series path broken | 1 |
| 1 | 1 | no pMOS branch conducts | complete series path | 0 |

The topology and truth table agree.

### Why NAND appears so often

NAND is functionally complete, but that is only part of the story. Its
pull-down uses nMOS devices, and its pull-up offers parallel alternatives.
Small-input NAND structures fit static CMOS naturally. Real cell libraries
therefore offer several NAND variants along with many other gates.

This does **not** mean a synthesized CPU is literally a sea of identical
two-input NAND symbols. Libraries include inverters, buffers, NAND/NOR,
AND/OR, XOR, multiplexers, compound gates, latches, and flip-flops, each in
several drive strengths. Synthesis and physical-design tools select cells to
meet function, timing, power, and layout constraints.

---

## 6. Derive the two-input NOR

The target is:

```text
Y = ¬(A ∨ B)
```

Again ask when the output must be LOW:

```text
Y = 0 when A = 1 OR B = 1
```

Either HIGH input must create a pull-down route, so the nMOS devices are in
parallel:

```text
       ┌──[ nMOS A ]──┐
Y ─────┤              ├── GND
       └──[ nMOS B ]──┘
```

The output is HIGH only when both inputs are LOW. Both pMOS devices must
conduct, so they are in series:

```text
VDD ──[ pMOS A ]──[ pMOS B ]── Y

       ┌──[ nMOS A ]──┐
Y ─────┤              ├── GND
       └──[ nMOS B ]──┘
```

Trace it:

| A | B | Pull-up path | Pull-down path | Y |
|---:|---:|:---|:---|---:|
| 0 | 0 | complete series path | neither branch conducts | 1 |
| 0 | 1 | series path broken | B branch conducts | 0 |
| 1 | 0 | series path broken | A branch conducts | 0 |
| 1 | 1 | series path broken | both branches conduct | 0 |

### NAND and NOR are duals

Compare:

```text
NAND: PDN series,   PUN parallel
NOR:  PDN parallel, PUN series
```

Replacing series with parallel while moving between pull-down and pull-up is
the physical face of De Morgan's laws:

```text
¬(A ∧ B) = ¬A ∨ ¬B
¬(A ∨ B) = ¬A ∧ ¬B
```

---

## 7. Why static CMOS directly gives inverted functions

The nMOS network describes the condition under which the output is pulled
LOW. If its conduction condition is `G`, then the gate output is:

```text
Y = ¬G
```

That is why one-stage complementary networks naturally produce NAND, NOR, and
inverting compound gates.

To obtain AND:

```text
A, B → NAND → inverter → A ∧ B
```

To obtain OR:

```text
A, B → NOR → inverter → A ∨ B
```

An AND symbol in a logic diagram may therefore map to more than one transistor
stage, or the surrounding logic may absorb inversions so no separate AND cell
is needed. Logical symbols, library cells, and transistor networks are related
views—not interchangeable counts.

---

## 8. Derive a small compound network

Suppose the required output is:

```text
Y = ¬(A ∧ (B ∨ C))
```

### Step 1: derive the LOW condition

The output is LOW when:

```text
A ∧ (B ∨ C)
```

### Step 2: build the nMOS pull-down

- `A AND something` means a series requirement;
- `B OR C` means parallel alternatives.

```text
Y ──[ nMOS A ]──┬──[ nMOS B ]── GND
                └──[ nMOS C ]── GND
```

### Step 3: build the dual pMOS pull-up

Exchange series and parallel:

```text
          ┌──[ pMOS A ]────────────────┐
VDD ──────┤                            ├── Y
          └──[ pMOS B ]──[ pMOS C ]────┘
```

The top branch conducts if A is LOW. The lower branch conducts if both B and C
are LOW. Therefore the output is HIGH for:

```text
¬A ∨ (¬B ∧ ¬C)
```

By De Morgan:

```text
¬(A ∧ (B ∨ C)) = ¬A ∨ (¬B ∧ ¬C)
```

The two derivations agree.

### A reusable derivation procedure

For an inverting static-CMOS function:

1. write the condition that should make the output `0`;
2. use nMOS series for AND and nMOS parallel for OR to build the PDN;
3. construct the PUN as the dual network—exchange series and parallel and use
   pMOS devices controlled by the same inputs;
4. enumerate every input row and check that exactly one network provides the
   intended rail path in the stable state.

This is enough depth to read a simple schematic and understand the physical
origin of a logic function.

---

## 9. Series resistance, parallel paths, and fan-in

An on transistor is not an ideal wire. It has finite effective resistance.
If two conducting devices lie in series, the current encounters both:

```text
rough intuition: Rpath ≈ R1 + R2
```

The approximation is deliberately simple—the actual device current is
nonlinear—but it gives the right systems-level consequence. A longer series
stack generally drives a capacitive output less strongly and takes longer to
move its voltage.

Parallel paths can provide alternative current routes. Yet the unused devices,
internal diffusion regions, wiring, and receiver gates all add capacitance.
More devices are not free.

**Fan-in** is the number of inputs accepted by a gate. Increasing fan-in tends
to bring:

- longer series stacks in one complementary network;
- more capacitance at the output and internal nodes;
- more capacitance presented to input drivers;
- slower worst-case transitions;
- a larger and harder-to-route cell.

An eight-input Boolean condition is therefore not automatically best
implemented as one enormous eight-input gate. A library and synthesis tool may
use a tree of smaller gates. The tree adds stages but avoids one weak,
high-capacitance structure. Timing is an electrical-path question, not simply
“fewest Boolean symbols wins.”

Do not turn this into transistor-sizing arithmetic here. The durable model is:

```text
more series resistance + more node capacitance → more transition time
```

Day 4 will make that relationship observable.

---

## 10. Restoration: why many stages can be connected

Suppose a HIGH input is not exactly at `VDD`, or noise shifts it slightly. If
every stage merely copied an analog fraction, errors would accumulate down a
long path.

A CMOS inverter has a steep transfer region:

```text
Vout
 VDD ────────┐
             │\
             │ \
             │  \
   0 ────────┴───┴──── Vin
             transition
```

Outside the transition region, the output is driven near a rail. Near the
switching point, a small input change produces a larger opposite output
change. This voltage gain is what makes the stage **restoring**.

Digital systems then define ranges:

```text
input LOW range     safely interpreted as 0
uncertain region    behavior not guaranteed as a logic value
input HIGH range    safely interpreted as 1
```

The separation between a guaranteed output level and the receiving input
threshold gives a **noise margin**. We do not need to calculate exact margins
today. We need the consequence: a valid but imperfect output should still be
interpreted correctly and restored by the next stage.

Restoration does not make circuits immune to everything. Excessive supply
noise, crosstalk, slow edges, process variation, radiation-induced charge, or
violated timing assumptions can still cause failure. Reliability comes from
quantified margins and verification, not from the word “digital.”

---

## 11. Where these networks appear in a CPU

Consider one decode condition:

```text
is_integer_add =
    opcode_matches
    AND privilege_allows
    AND operands_ready
    AND no_flush
```

At the ISA level, this is a condition about instruction meaning. At the RTL
level, it is a Boolean expression. After synthesis, it becomes library cells
and wires. Inside those cells are transistor networks. While the CPU runs,
capacitances on those nodes charge and discharge.

```text
instruction bits and machine state
    → decode/control Boolean network
    → voltage transitions through cells and wires
    → enable/select signals
    → register, ALU, or memory action
```

The transistors do not understand `ADD`, privilege, or readiness. Their local
electrical behavior happens to implement a network whose abstraction matches
those predicates.

The same chain appears in:

- ALU carry and selection logic;
- branch-condition evaluation;
- address-generation control;
- cache tag comparison and hit selection;
- pipeline valid, stall, flush, and forwarding logic;
- interrupt and exception prioritization.

Once topology introduces finite resistance and each node has capacitance,
these decisions take time. That is the bridge from “the decoder computes a
Boolean answer” to “the clock cannot be arbitrarily fast.”

---

## 12. Lab — observe topology and restoration

The lab contains:

- a two-input static-CMOS NAND;
- a two-input static-CMOS NOR;
- an inverter swept through all input voltages;
- documented, generic MOS level-1 models;
- a Python runner that checks truth rows and summarizes restoration.

Files:

- `04-cpu-and-chip-design/challenges/day-003-cmos-networks.cir`
- `04-cpu-and-chip-design/challenges/day-003-cmos-networks.py`

### Predict before running

Write down:

1. For `(A,B) = (0,0), (1,0), (0,1), (1,1)`, which NAND network conducts?
2. For the same rows, which NOR network conducts?
3. At `Vin = 0`, should the inverter output settle near `0`, halfway, or
   `VDD`?
4. Near the inverter switching boundary, should the transfer curve be flat or
   steep?

### Run

From the repository root:

```bash
python3 04-cpu-and-chip-design/challenges/day-003-cmos-networks.py
```

To retain the raw ngspice data and log:

```bash
python3 04-cpu-and-chip-design/challenges/day-003-cmos-networks.py \
  --keep /tmp/day-003-cmos-evidence
```

The `.cir` file is also directly batch-runnable:

```bash
repo=$(pwd)
tmpdir=$(mktemp -d)
cd "$tmpdir"
ngspice -b \
  "$repo/04-cpu-and-chip-design/challenges/day-003-cmos-networks.cir"
```

Use the Python command for the normal course workflow because it creates a
temporary directory, checks expected values, and leaves the repository clean.

### What the model does—and does not prove

The netlist uses the classical ngspice MOS level-1 square-law model with
explicit teaching parameters. It is intentionally:

- public and inspectable;
- small enough to connect topology to waveforms;
- adequate for qualitative NAND/NOR and restoration observations.

It is **not**:

- a model for a named fabrication process;
- accurate for a modern nanometre CPU transistor;
- a standard-cell characterization;
- evidence of real CPU voltage, delay, power, or noise margin;
- suitable for tape-out or reliability sign-off.

Modern process models include short-channel, leakage, variation,
interconnect, and many other effects absent here. The lab proves conceptual
relationships inside its stated model.

### Explain the result

After running, complete these sentences:

1. NAND is LOW only for `11` because ...
2. NOR is HIGH only for `00` because ...
3. The inverter restores logic levels because ...
4. A high-fan-in gate can slow a CPU path because ...

---

## 13. Evidence: Explain, Draw, Observe, Connect

### Explain

Without notes:

1. Why is “one transistor equals one gate” wrong?
2. What are the roles of the PUN and PDN?
3. Why does a NAND use series nMOS and parallel pMOS?
4. Why does a NOR use parallel nMOS and series pMOS?
5. Why do AND and OR often require another inversion in static CMOS?
6. Why can high fan-in make a gate slower?
7. What does restoration contribute that a passive switch network does not?

### Draw

Draw from memory:

1. CMOS inverter;
2. NAND2 PUN and PDN;
3. NOR2 PUN and PDN;
4. the compound gate `¬(A ∧ (B ∨ C))`.

For every drawing, annotate which input combinations make each network
conduct. A transistor sketch without the conduction explanation is incomplete.

### Observe

Keep:

- the four settled NAND/NOR rows;
- inverter output at both rails;
- the approximate switching region;
- one sentence explaining why a steep transfer matters.

### Connect

Choose one CPU condition—branch taken, cache hit, pipeline stall, or register
write enable—and trace:

```text
architectural meaning
    → Boolean predicate
    → gate network
    → pull-up/pull-down conduction
    → restored output voltage
    → next stage
```

Do not claim a particular commercial CPU uses your exact network. The point is
the abstraction bridge.

---

## 14. Mastery checkpoint

Close the lesson and answer:

> Starting from a Boolean function, how can a complementary CMOS network
> produce a restored output, and why does its topology matter to CPU timing?

A complete answer should include:

- nMOS and pMOS conduction conventions;
- PDN for the output-LOW condition;
- dual PUN for the output-HIGH condition;
- series as “all paths required” and parallel as “either path sufficient”;
- NAND and NOR derivations;
- finite series resistance and node capacitance;
- restoration toward supply rails;
- the connection from cell delay to a CPU logic path.

If the answer is only “transistors act like switches,” it is not complete.

---

## Why? notebook

1. Why derive the pull-down from the output-LOW condition?
2. Why must the pull-up be complementary rather than an unrelated network?
3. Why does series describe conduction AND but not automatically output AND?
4. Why do NAND and NOR emerge directly as inverting static-CMOS gates?
5. Why can fewer logic symbols still produce a slower physical circuit?
6. Why does a floating output violate the model expected by the next gate?
7. Why is a steep inverter transfer useful but not a guarantee against every
   disturbance?
8. Where does an instruction's meaning disappear as we descend toward
   transistors?

---

## Mental model at the end

```text
A transistor responds to terminal voltages.
A network creates or removes conducting paths.
The PDN realizes the condition for output LOW.
The dual PUN realizes the condition for output HIGH.
The output is actively restored toward a rail.
Series stacks add effective resistance; nodes add capacitance.
Gate networks implement CPU predicates without understanding them.
```

---

## References used selectively

- Claude E. Shannon, “A Symbolic Analysis of Relay and Switching Circuits,”
  1937 thesis, for the historical algebra-to-switching-network bridge:
  <https://dspace.mit.edu/handle/1721.1/11173>
- Computer History Museum, “The Silicon Engine,” for the historical transition
  from relay/vacuum-tube limitations to transistor and integrated-circuit
  technologies:
  <https://www.computerhistory.org/siliconengine/>
- Hans-Dieter Wacker, *Introduction to Digital Circuits*, “Basic Digital
  Circuits,” for complementary static-CMOS PUN/PDN derivations and fan-in
  consequences:
  <https://www.strumpen.net/dc/build/html/basiccircuits/basiccircuits.html>
- Holger Vogt et al., *ngspice User's Manual*, Version 47, 11 August 2026,
  MOSFET device and level-1 model sections:
  <https://ngspice.sourceforge.io/docs/ngspice-html-manual/manual.xhtml>
- ngspice manual, “Notes on Level 1–6 models,” for the documented limits and
  parameters of the classical models used by the lab:
  <https://nmg.gitlab.io/ngspice-manual/mosfets/mosfetmodels_nmos_pmos/notesonlevel1-6models.html>

**Next bridge:** a logically correct output still takes time to move. Day 4
connects capacitance and finite drive to propagation delay, fan-out, power,
PVT variation, and the CPU's critical path.
