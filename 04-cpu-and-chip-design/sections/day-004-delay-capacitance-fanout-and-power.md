# Day 4 — Delay, Capacitance, Fan-out, and Power

**Target time:** approximately 2–3 hours

- First-principles timing and power model: 75–90 minutes
- ngspice/Python load sweep: 35–45 minutes
- CPU timing, reliability, and evidence: 35–45 minutes

> The Boolean answer may be correct. What determines whether it arrives before
> the next clock edge?

## Why this day exists

A truth table has no time axis. It says that an inverter maps `0` to `1`, but
not whether the output changes in one picosecond or one second.

A CPU does not receive unlimited time. A pipeline stage launches signals,
combinational logic responds, and a later register must capture the settled
result. If the slowest allowed path has not completed, the machine can store
the wrong bit even though every gate has the correct truth table.

Today's path is:

```text
every node stores charge
    + every driver has finite current/resistance
    → transitions take time
    → load and fan-out alter delay
    → switching consumes energy
    → process, voltage, and temperature alter both
    → the worst valid path constrains the clock
```

We will keep the hardware depth purposeful. You should finish able to reason
about CPU timing, performance, power, and reliability—not perform analog
transistor optimization or standard-cell characterization.

---

## 1. The historical problem: integration made time and power first-class

Early switching machines were visibly slow: relay contacts moved; vacuum
tubes and long wires had finite response. Transistors and integrated circuits
made switching dramatically faster and packed far more logic together, but
they did not make delay vanish.

Integration changed the scale of the engineering problem:

- each individual node might carry tiny capacitance;
- billions of devices and extensive wiring create enormous aggregate load;
- higher clock rates cause more charge movement per second;
- heat removal and supply delivery limit sustained operation;
- variation means the nominally “same” path is not identical on every chip or
  under every operating condition.

This is why modern processor design cannot be explained by Boolean algebra
alone. A design must be logically correct **and** close timing under allowed
conditions **and** stay within power, temperature, and reliability limits.

The historical “frequency scaling” story also needs precision. Higher clock
rates once delivered large performance gains, but power density, wire delay,
memory behavior, and diminishing voltage scaling made indefinite frequency
growth impractical. Modern CPUs use pipelining, parallel execution, caches,
specialized units, power management, and many cores because “just raise the
clock” is not a free strategy.

---

## 2. A logic node is a physical capacitor

Where does capacitance come from in a gate network?

- MOS gate terminals store charge;
- transistor junctions and internal nodes contribute capacitance;
- nearby and long wires contribute capacitance;
- every receiving gate attached to an output adds input capacitance;
- package and off-chip connections can add far larger loads.

We often combine those effects into an effective load:

```text
Cload = receiver input capacitance
      + wire capacitance
      + driver/output parasitics
      + other coupled contributions represented by the model
```

The precise decomposition belongs to circuit and physical design. For systems
reasoning, the important relationship is:

```text
Q = C × V
```

To move a capacitance `C` through a voltage change `V`, the driver must move
charge `Q`. More capacitance or a larger voltage swing requires more charge.

### The output does not teleport between rails

When a CMOS gate output rises, its pull-up network supplies current and charges
the node. When it falls, the pull-down network removes charge toward ground.

```text
rise: VDD → finite pull-up path → Cload
fall: Cload → finite pull-down path → GND
```

Because current is finite, both changes take time.

---

## 3. RC delay: a useful model, not a transistor law

Approximate the conducting network by an effective resistance `R` and the
output by a capacitance `C`:

```text
VDD ── R ── output
             │
             C
             │
            GND
```

For a simple charging RC network:

```text
Vout(t) = VDD × (1 - e^(-t/RC))
```

For discharge:

```text
Vout(t) = Vinitial × e^(-t/RC)
```

The product:

```text
τ = R × C
```

is the time constant. After one `τ`, a charging node has traversed about 63%
of the final change. The familiar factor `0.69RC` estimates the time to cross
50% in a first-order model.

Do not promote that approximation into a universal gate-delay formula. A real
MOSFET is nonlinear; the effective resistance changes during a transition;
internal nodes, input slew, coupling, and wires matter. Characterized cell
libraries and timing tools use richer data. `RC` remains valuable because it
explains the direction of effects:

```text
larger effective R → slower transition
larger effective C → slower transition
```

Day 3's series transistor stack raises effective path resistance. Today's
receiver and wire load raises capacitance. Together they explain why topology
and connectivity affect timing.

---

## 4. Propagation delay is measured between thresholds

An input edge itself takes time, and the output does not begin and finish at
one instant. We therefore define reproducible landmarks.

### High-to-low propagation delay

`tpHL` is commonly measured from the input crossing 50% of its swing to the
output crossing 50% while falling:

```text
input:   ___/‾‾‾‾‾
             │ 50%
output:  ‾‾‾‾\____
                │ 50%
          <----->
             tpHL
```

### Low-to-high propagation delay

`tpLH` is the corresponding input-to-output delay for a rising output.

One may summarize a gate with an average propagation delay:

```text
tpd ≈ (tpHL + tpLH) / 2
```

or use the worse direction when a bound is needed. Always check what a quoted
number means. “Gate delay” without measurement conditions, input slew, load,
supply, temperature, and process assumptions is incomplete.

### Rise and fall time

Rise time and fall time describe the output transition itself, often between
10% and 90%:

```text
trise: output crosses 10% → output crosses 90%
tfall: output crosses 90% → output crosses 10%
```

Propagation delay and transition time are related but not identical:

- propagation delay asks when the output response reaches a reference point;
- rise/fall time asks how long the output spends traversing most of its swing.

A slow edge can make the next stage slower and can leave its transistors
simultaneously conducting for longer. Timing analysis therefore tracks both
arrival time and slew.

### Why rise and fall differ

Pull-up and pull-down networks use different device types and topologies.
Their drive need not match exactly. NAND and NOR also have different
worst-case stacks in the rising and falling directions. Consequently:

```text
tpHL need not equal tpLH
trise need not equal tfall
```

The difference is physical, not a violation of the truth table.

---

## 5. Fan-out turns one decision into a load problem

**Fan-out** informally means how many receiving inputs an output drives:

```text
                 ┌→ gate input
driver output ───┼→ gate input
                 ├→ gate input
                 └→ gate input
```

Each receiver contributes gate capacitance. The connecting wire adds more.
So increasing fan-out generally increases `Cload`, which increases charge,
transition time, delay, and switching energy.

The integer receiver count is only a rough description. Four tiny nearby
inputs and four large distant inputs are not the same electrical load. Timing
tools work with capacitance, slew, wire estimates or extracted parasitics, and
characterized cell behavior—not receiver count alone.

### Why not attach everything to one signal?

A control signal such as clock enable, reset, or pipeline valid may need to
reach many destinations. One weak source driving the entire load would switch
slowly. Designers use buffering trees, replication, and structured
distribution:

```text
source → buffer ┬→ buffer → local receivers
                └→ buffer → local receivers
```

Extra stages sound slower, but each stage drives a manageable load. Under a
large total load, a staged path can arrive sooner than one small gate trying
to move all the charge.

### Logical effort—intuition only

Logical effort is a compact way to reason about two facts:

1. some gate topologies are intrinsically harder to drive than an inverter
   with comparable output capability;
2. delay also depends on the external capacitive load relative to the gate's
   own input capacitance.

It helps designers compare topology and staging without beginning every
question from a full transistor simulation. For this course, retain only:

```text
gate topology contributes intrinsic difficulty
load contributes electrical effort
too much load on one stage is slow
staging can distribute a large load
```

We will not calculate logical-effort values or perform transistor-sizing
exercises. CPU timing intuition—not analog design methodology—is the goal.

---

## 6. Dynamic energy: derive `C V²`

To charge a capacitor from `0` to `V`, the supply moves charge:

```text
Q = C V
```

The supply operates at voltage `V`, so the energy drawn from it during a
low-to-high charge is:

```text
Esupply = V × Q = C V²
```

An ideal capacitor stores:

```text
Ecap = ½ C V²
```

Where did the other half go? In the simple resistive charging model, it is
dissipated in the charging path. When the node later discharges to ground, the
stored half is also dissipated. Thus a complete `0 → 1 → 0` activity cycle
draws approximately:

```text
Ecycle = C V²
```

If this happens at an average switching rate `αf`:

```text
Pdynamic ≈ α C V² f
```

where:

- `C` is effective switched capacitance;
- `V` is supply voltage;
- `f` is a reference frequency, often clock frequency;
- `α` is an activity factor connecting that reference to actual switching.

Activity-factor conventions vary. State whether `α` counts low-to-high
charges, transitions, or cycles before comparing formulas or numbers.

### What the equation tells a systems programmer

- more active hardware raises power;
- larger loads and wires raise power;
- higher frequency raises switching power roughly linearly if activity is
  otherwise unchanged;
- voltage is especially expensive because of the square;
- reducing work, toggles, capacitance, or voltage can matter more than a
  source-level instruction count suggests.

Voltage reduction also slows transistors and reduces noise margin, so
dynamic-voltage/frequency scaling is a performance-reliability trade, not a
free energy switch.

---

## 7. Dynamic power is not all power

### Short-circuit power

During a finite input transition, the pMOS and nMOS networks can both conduct
briefly:

```text
VDD → partially conducting pull-up
    → output network
    → partially conducting pull-down → GND
```

That current does not exist in the ideal stable-state truth table. Slower input
edges can lengthen the overlap interval. The exact contribution depends on the
cell, load, slew, and supply.

### Leakage power

“Off” transistors are not perfect open switches. Leakage includes several
physical mechanisms, and their importance depends strongly on process and
operating conditions. A useful system-level model is:

```text
Pleak ≈ VDD × Ileak
```

Leakage is paid even when useful nodes are not switching. That matters for:

- large transistor counts;
- idle cores and caches;
- thermal management;
- battery life;
- the choice to clock-gate, power-gate, or place blocks in lower-power states.

Clock gating reduces unnecessary switching but does not remove all leakage.
Power gating can reduce leakage more aggressively but adds state-retention,
wake-up, supply-integrity, and latency concerns.

### Internal switching and glitches

A combinational network can briefly pass through intermediate states when
different paths arrive at different times. Those **glitches** may charge and
discharge internal nodes even if the final Boolean output does not change.
They consume dynamic energy and, if they escape an allowed timing window, may
affect behavior.

So `α C V² f` is a powerful first model, not a complete power report.

---

## 8. PVT: the same design does not have one delay

Timing depends on **process, voltage, and temperature (PVT)**.

### Process

Manufacturing variation changes device properties and interconnect. Some
instances or regions may switch faster or slower than nominal assumptions.
Design flows therefore analyze characterized corners and statistical or
on-chip variation models appropriate to the technology.

### Voltage

Lower supply voltage generally reduces transistor drive and increases delay.
Local voltage droop can make a path temporarily slower exactly when substantial
activity draws current. Higher voltage can improve speed within limits but
raises dynamic power and electrical stress.

### Temperature

Temperature changes mobility, threshold behavior, leakage, and wire
resistance. The direction and size of net delay change depend on technology,
voltage, and circuit. Do not memorize “hot is always slower” as a universal
device law, though slow/hot assumptions are common in simplified examples.

### Why corners are a reliability requirement

A CPU sold to operate across a specified range must work outside a room-
temperature typical simulation. Timing sign-off asks whether all constrained
paths meet requirements across defined operating scenarios, not merely whether
one nominal waveform looks clean.

Day 4's lab perturbs simple model parameters to expose sensitivity. Those
perturbations are **illustrative corners**, not a replacement for foundry
models or sign-off analysis.

---

## 9. From one gate delay to a CPU critical path

Between two state elements, a synchronous path is conceptually:

```text
launch register
    → clock-to-Q delay
    → combinational gates and wires
    → setup requirement at capture register
```

A simplified setup constraint is:

```text
Tclock ≥ tclock-to-Q + tcombinational,max + tsetup
         + clock/skew/jitter/variation margin
```

The **critical path** is the path with the least timing slack under the
relevant analysis—not necessarily the one with the most gate symbols.
Connectivity, cell choices, transition slew, wire length, fan-out, coupling,
variation, and clock relationships all matter.

### Concrete CPU candidates

Depending on microarchitecture, long paths may include:

- instruction decode and control selection;
- ALU carry or compare logic;
- bypass/forwarding selection;
- branch condition and next-PC selection;
- load address generation and cache tag/data selection;
- wide priority encoders or wake-up/select logic.

There is no universal “the CPU critical path.” It can change with design
revision, operating mode, PVT corner, placement, or optimization.

### What if the path is too slow?

Possible responses include:

- restructure Boolean logic;
- reduce fan-out or replicate a driver;
- insert buffers;
- place communicating cells closer;
- choose a stronger or faster characterized cell;
- reduce wire load;
- add a pipeline stage;
- lower the clock frequency;
- raise voltage within allowed limits;
- change the microarchitecture.

Every response has costs. A pipeline stage can shorten combinational work per
cycle and raise potential throughput, but adds registers, clock power,
latency, control complexity, and branch-recovery cost. Stronger cells can
reduce delay but increase input capacitance, area, and power.

### Frequency is not performance

The clock bound gives:

```text
fmax ≈ 1 / Tmin
```

but program performance also depends on:

- instructions or useful operations completed per cycle;
- stalls and dependencies;
- branch prediction;
- cache and memory behavior;
- core count and parallelism;
- thermal and power limits;
- operating frequency actually sustained.

A deeper pipeline with a higher advertised clock can lose work on
mispredictions or consume more power. Timing is one constraint in performance,
not a complete performance metric.

---

## 10. Timing failures become architectural failures

If a signal arrives after the capture window, a register may capture the old
value, the new value, or—in edge cases we will study later—enter metastable
behavior. Above that physical event, the symptom might look like:

- a wrong arithmetic result;
- a missed pipeline stall;
- an incorrect branch target;
- a corrupted address;
- an invalid cache-control action;
- an exception that appears unrelated to the underlying path.

Software expects the hardware contract to hold. It cannot retry every internal
gate transition. Hardware therefore uses design margins, static timing
analysis, dynamic simulation, physical verification, on-chip monitors,
clock/voltage control, testing, and error-detection mechanisms.

### A feedback loop worth remembering

```text
more switching
    → more power
    → higher temperature / more supply droop
    → changed delay and leakage
    → reduced timing margin or throttling
    → changed delivered performance
```

This is why `cpufreq`, thermal throttling, power limits, boost behavior, and
hardware error reports are connected to transistor-level constraints even
though Linux exposes them through high-level interfaces.

---

## 11. Lab — sweep load, observe delay and energy

The lab uses one CMOS inverter and varies an explicit output capacitor. For
discussion, `4 fF` is labeled as one notional receiver input:

```text
fan-out 1 → 4 fF
fan-out 2 → 8 fF
fan-out 4 → 16 fF
fan-out 8 → 32 fF
```

That mapping is a teaching convention, not a real cell-library specification.

Files:

- `04-cpu-and-chip-design/challenges/day-004-delay-power.cir`
- `04-cpu-and-chip-design/challenges/day-004-delay-power.py`

The script measures:

- `tpHL` and `tpLH` at 50% crossings;
- 10–90% rise and fall time;
- supply energy over one complete input period;
- three small PVT-sensitivity cases at one load.

### Predict before running

Write down:

1. As load doubles, what should happen to propagation delay?
2. What should happen to rise and fall time?
3. What should happen to supply energy per toggle?
4. Will rise and fall measurements be identical?
5. Which should be slower in the lab's illustrative set:
   `slow/low/hot` or `fast/high/cool`?
6. Which of these predictions are qualitative, and which claim a numerical
   relationship?

### Run

From the repository root:

```bash
python3 04-cpu-and-chip-design/challenges/day-004-delay-power.py
```

The script is batch-runnable, needs no plotting package, and writes:

```text
/tmp/day-004-delay-power.csv
```

Run one nominal circuit directly if you want the raw ngspice log and waveform:

```bash
repo=$(pwd)
tmpdir=$(mktemp -d)
cd "$tmpdir"
ngspice -b \
  "$repo/04-cpu-and-chip-design/challenges/day-004-delay-power.cir"
```

The direct run writes `day-004-waveform.dat` in the temporary directory.

### Read the result conservatively

Expected qualitative observations:

- added load increases propagation delay;
- added load lengthens the edge;
- supply energy rises with switched capacitance;
- pull-up and pull-down results differ somewhat;
- changed model strength, voltage, and temperature alter delay.

Do not claim:

- that the delay belongs to any commercial CPU;
- that one notional fan-out has a universal capacitance;
- that the sweep establishes a modern process corner;
- that the level-1 model predicts nanometre-device leakage or short-channel
  behavior;
- that a four-point curve proves exact linearity under all loads.

### Connect every row to a CPU

For each increasing-load row, say:

> If this node were on a pipeline's limiting path, the added delay would
> consume timing slack. The design might need less load, buffering,
> restructuring, another pipeline stage, or a longer clock period.

For the energy column, say:

> If many such nodes toggle repeatedly, their individual charging events
> accumulate into dynamic CPU power and heat.

For the PVT rows, say:

> A clock safe at nominal conditions is not sufficient evidence for the full
> operating range.

---

## 12. Why this model is intentionally limited

The `.cir` file uses documented ngspice level-1 MOS models with explicit
generic parameters. It includes a simple charge-storage model and explicit
load capacitance so delay and energy trends are visible.

It omits or greatly simplifies:

- modern short-channel transistor behavior;
- real process variation and correlated on-chip variation;
- accurate leakage mechanisms;
- extracted metal resistance, capacitance, and coupling;
- cell-internal characterization across input slew and output load;
- clock-tree and power-grid models;
- package effects and supply noise;
- aging, electromigration, and long-term reliability;
- statistical timing and sign-off constraints.

The numeric results belong only to this netlist and simulator configuration.
The conceptual result—finite drive charging capacitance causes delay and
energy—is the evidence we need.

ngspice 47 is the validation target for these files. The netlists use public
syntax and no proprietary model cards.

---

## 13. Evidence: Explain, Draw, Observe, Connect

### Explain

Without notes:

1. Where does capacitance exist in a logic path?
2. Why does `Q = CV` imply that a heavier load switches more slowly?
3. Distinguish propagation delay from rise/fall time.
4. Why is fan-out count only an approximation of load?
5. Derive the qualitative meaning of `P ≈ αCV²f`.
6. Distinguish capacitive switching, short-circuit, and leakage power.
7. Why must timing be checked across PVT conditions?
8. Why is the critical path not necessarily the path with the most gates?

### Draw

Draw:

```text
launch register
    → gate/wire RC path
    → high-fan-out node
    → capture register
```

Annotate:

- clock-to-Q;
- propagation delays;
- output load;
- setup time;
- clock period;
- one place where buffering could help;
- one place where extra buffering would add power.

### Observe

Keep the CSV and record:

- delay at fan-out 1 and fan-out 8;
- rise/fall time change over the sweep;
- energy change over the sweep;
- fastest and slowest illustrative corner;
- one asymmetry between rising and falling behavior.

### Connect

Choose one:

- ALU result to destination register;
- branch compare to next-PC register;
- cache tag compare to hit/miss decision;
- pipeline stall logic to stage-enable register.

Explain how extra load, lower voltage, or a slow corner could consume slack,
then name one hardware response and one trade-off it introduces.

---

## 14. Mastery checkpoint

Close the lesson and answer:

> Why can a logically correct CPU fail when clocked too fast, and how are
> capacitance, fan-out, power, PVT, and the critical path connected?

A complete answer includes:

- charge stored on gate and wire capacitances;
- finite pull-up/pull-down drive;
- propagation and transition time;
- fan-out increasing effective load;
- dynamic energy scaling approximately with `CV²`;
- short-circuit and leakage as additional power;
- PVT changing delay and power;
- the longest constrained register-to-register path bounding clock period;
- timing failure becoming incorrect captured state;
- performance depending on more than frequency alone.

---

## Why? notebook

1. Why does a correct truth table say nothing about maximum clock frequency?
2. Why is a wire part of the circuit rather than an abstract connection?
3. Why can adding buffers reduce delay while increasing power and area?
4. Why does lowering voltage save substantial dynamic power but threaten
   timing margin?
5. Why does an idle CPU still consume power?
6. Why can glitches consume energy even when the final output is unchanged?
7. Why is “typical at room temperature” insufficient for reliability?
8. Why might the critical path change after placement or at another PVT
   corner?
9. Why can a higher-frequency CPU deliver less useful performance?
10. How can transistor-level delay eventually surface as Linux thermal
    throttling or reduced boost residency?

---

## Mental model at the end

```text
Logic values are voltages on capacitive nodes.
Drivers move finite charge through finite conducting paths.
Load and fan-out change delay, edge rate, and energy.
Switching power is roughly αCV²f; overlap and leakage add more.
PVT means there is no single universal delay.
The slowest constrained path consumes the clock budget.
Timing and power limits shape CPU performance and reliability.
```

---

## References used selectively

- Holger Vogt et al., *ngspice User's Manual*, Version 47, 11 August 2026,
  transient analysis, measurement commands, MOSFET devices, and level-1 model:
  <https://ngspice.sourceforge.io/docs/ngspice-html-manual/manual.xhtml>
- ngspice release history, confirming release 47 and its 11 August 2026
  documentation:
  <https://ngspice.sourceforge.io/news.html>
- Ivan Sutherland, Bob Sproull, and David Harris, *Logical Effort: Designing
  Fast CMOS Circuits*, Morgan Kaufmann, 1999; companion material for the
  topology/load/staging intuition:
  <https://pages.hmc.edu/harris/research/letalk.pdf>
- David Money Harris, “CMOS VLSI Design — Power,” for activity, capacitive
  switching, short-circuit, and leakage categories:
  <https://pages.hmc.edu/harris/cmosvlsi/4e/lect/lect7.pdf>
- Mark Horowitz, “Computing's Energy Problem (and what we can do about it),”
  ISSCC 2014, for the modern system-level importance of data movement and
  energy limits:
  <https://doi.org/10.1109/ISSCC.2014.6757323>
- David A. Patterson and John L. Hennessy, *Computer Organization and Design*,
  processor-performance and pipelining chapters, for the connection between
  stage delay, clock period, CPI, and delivered performance.

**Next bridge:** timing between combinational stages leads directly to setup,
hold, clock skew, latches, flip-flops, and metastability—the rules that let a
CPU capture changing signals as reliable state.
