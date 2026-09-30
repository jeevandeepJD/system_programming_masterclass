# Day 1 — The MOSFET as a Controlled Switch

**Target time:** approximately 2–3 hours

- Historical and device model: 65–75 minutes
- Predictions, sketches, and worked reasoning: 30–40 minutes
- ngspice experiment and evidence: 55–70 minutes
- Explain/Draw/Observe/Build checkpoint: 20–30 minutes

> A CPU appears to manipulate symbols. At its lowest active layer, however,
> changing a bit means moving charge through devices whose conductance depends
> on voltage. What kind of device can make that reliable enough to repeat
> billions of times?

## Why this day exists

Earlier lessons treated a transistor as an ideal controlled switch. That
abstraction was useful: it let us derive Boolean functions without stopping
for device physics. It also hid the questions that determine whether a gate
works in silicon:

- What does the control voltage actually change?
- Is a MOSFET ever perfectly open or perfectly closed?
- Why is there a threshold rather than one exact switching voltage?
- Why does a conducting transistor still have a voltage drop and finite
  current?
- Where does the output voltage come from?
- Why does changing an output take time?

Today we replace the ideal switch with a small, useful electrical model. The
goal is not semiconductor-device specialization and not quantum mechanics.
The goal is enough physical grounding to reason honestly about CMOS gates,
delay, power, and restored logic.

The path is:

```text
voltage on a gate terminal
    → electric field changes channel conductivity
    → current moves charge
    → node voltage changes
    → another device senses that voltage
    → a switching network realizes logic
```

The last arrow belongs mainly to Day 2. Today earns the first four.

---

## 1. The engineering problem before MOS logic

Relays provided controllable conducting paths, but their moving contacts were
large, slow, noisy, and subject to wear. Vacuum tubes removed mechanical
motion and enabled much faster electronic switching, but occupied substantial
space and consumed considerable power. Bipolar transistors made switching
solid-state, smaller, and more reliable.

The metal-oxide-semiconductor field-effect transistor, or **MOSFET**, offered
another decisive property: its control terminal is insulated from the
conducting channel by a dielectric. Ideally, steady gate current is nearly
zero. A gate is controlled primarily by voltage and electric field rather
than by a continuous input current.

MOS devices also fit planar fabrication and scaling extremely well. When
complementary n-channel and p-channel devices are paired as CMOS, stable
logic states can ideally avoid a direct DC path from the supply to ground.
That combination—density, manufacturability, and low static power—made CMOS
the dominant foundation of large digital integrated circuits.

This history is not a story in which each technology instantly replaced the
last. Relays, tubes, bipolar devices, and MOS devices overlap in time and
remain useful in different applications. The important engineering sequence
is that each technology changed the limits on speed, size, reliability,
drive, and power.

---

## 2. Four terminals, three voltages, one useful abstraction

An n-channel MOSFET has four physical terminals:

```text
                  drain D
                     │
gate G ── electric field controls channel
                     │
                  source S

body or bulk B influences the channel too
```

Digital schematics often tie the nMOS body to the lowest supply and the pMOS
body to the highest supply, then visually suppress the body connection. Do
not forget that it exists. Body voltage can shift threshold through the
**body effect**, and source/drain junctions create parasitic diodes.

For a first nMOS model, use:

```text
VGS = VG - VS      gate-to-source voltage
VDS = VD - VS      drain-to-source voltage
VTH                threshold parameter
```

The gate dielectric blocks an ordinary conductive path from gate to channel,
but the gate, oxide, and semiconductor form a capacitor. Applying `VGS`
changes the electric field and therefore the charge distribution beneath the
gate.

For an enhancement-mode nMOS:

- low `VGS`: no strong inversion channel; drain current is very small;
- `VGS` above threshold: a conducting channel forms;
- larger gate overdrive `VGS - VTH`: the channel generally conducts more
  strongly.

This is the switch intuition:

```text
gate LOW   → nMOS weak/off → path approximately open
gate HIGH  → nMOS on       → path approximately conducting
```

It is an approximation, not a new physical mode that erases analog behavior.

### A necessary vocabulary correction

Engineers often say “current flows from drain to source” for conventional
current in an nMOS. Electrons, which carry the channel current, drift in the
opposite direction. Circuit analysis normally uses conventional current.
Choose one convention and label it rather than silently changing directions.

---

## 3. Threshold is not the gate's logic threshold

The MOSFET threshold voltage `VTH` is a device-model quantity associated with
the onset of strong inversion. It is **not** all of the following:

- a perfect boundary below which current is exactly zero;
- the input voltage at which every inverter changes logical state;
- a universal constant for all transistors;
- necessarily half the supply voltage;
- a guarantee that the transistor is a low-resistance switch.

Below threshold, real MOSFETs have **subthreshold current**. Above threshold,
drive strength grows continuously. Threshold varies with fabrication process,
temperature, body bias, device geometry, and the definition/measurement
method. Small devices also exhibit effects omitted by the long-channel model.

Later, the CMOS inverter's switching point will emerge from competition
between its nMOS and pMOS currents. That circuit-level point depends on both
devices and their sizing, not merely on one device's `VTH`.

Keep three ideas separate:

```text
device threshold      model of channel formation
logic input limits    voltages guaranteed to be interpreted safely
inverter trip region  where pull-up and pull-down compete
```

Collapsing these into one “threshold” causes persistent confusion.

---

## 4. The smallest device model a systems programmer needs

We need a behavioral map, not a device-design derivation:

| Condition | Useful first approximation | Important caveat |
|---|---|---|
| gate well below threshold | channel is weak; switch looks open | real devices leak |
| gate above threshold | channel conductance rises | there is no one fixed “on resistance” |
| conducting with small drain-source voltage | behaves roughly resistively | resistance depends on bias and geometry |
| conducting while drain-source voltage is large | current is limited by device behavior | “saturation” does not mean an ideal closed switch |

The labels **cutoff**, **triode/linear**, and **saturation** name operating
regions of a simplified MOS model. A transistor can move through several of
them during one gate transition. For this course, their value is explanatory:
they tell us why a MOSFET is not a mechanical contact and why delay/current
change throughout a transition.

Modern transistors need much richer models for design. We intentionally stop
before short-channel equations, fabrication details, and process-specific
parameters. The lab's simple Level 1 model exposes the shape of behavior; it
does not predict a commercial CPU.

---

## 5. Predict the transistor curves

Imagine holding `VDS` at three values while sweeping `VGS` from 0 V to 3.3 V.

Before simulation, sketch current on the vertical axis and `VGS` on the
horizontal axis.

1. Below `VTH`, what does the simple model predict?
2. Which fixed `VDS` should permit the largest current?
3. At high `VGS`, could the smallest `VDS` case enter the linear region?
4. Should current jump to one fixed “on” value at threshold?

Now imagine fixing `VGS` high and increasing `VDS`:

```text
small VDS                     larger VDS
resistive/triode  ─────────→  saturation region
```

The curve bends because the channel is not a mechanical contact with one
resistance. The systems-level observation is enough: changing a control
voltage continuously changes available drive current, and that current sets
how quickly downstream capacitance can move to a new logic level.

---

## 6. An nMOS used as a pull-down switch

Connect a resistor to the supply and an nMOS to ground:

```text
             VDD
              │
           resistor
              │
              ├──── VOUT
              │
             nMOS
              │
             GND

VIN controls the nMOS gate
```

When `VIN` is low, the nMOS is weak. The resistor pulls `VOUT` toward `VDD`.
When `VIN` is high, the nMOS conducts and removes charge from the output node,
pulling it toward ground.

This circuit inverts, but it is not modern complementary CMOS:

- in the LOW-output state, current continuously runs through the resistor and
  nMOS;
- the resistor is a weak pull-up and can make rising transitions slow;
- output LOW depends on a resistance ratio and is not exactly ground;
- a practical integrated resistor can consume substantial area.

It remains an excellent bridge from one transistor to a restoring gate.

### Voltage does not “flow”

When the nMOS turns on, voltage does not pour out of the node. Current moves
charge. The output node has capacitance—intentional and parasitic. Its voltage
is related to stored charge:

```text
Q = C V
I = dQ/dt = C dV/dt
```

Therefore:

```text
dV/dt = I/C
```

A finite current needs finite time to change voltage across capacitance. This
is the physical seed of propagation delay.

### First-order RC intuition

When the transistor is off, the output charges through the resistor. If the
load is approximated as a capacitor:

```text
VOUT(t) ≈ VDD(1 - e^(-t/RC))
```

The time constant is `τ = RC`. After one time constant the node has moved
about 63% of the way toward its final value. It never waits for a symbolic
“bit assignment”; its analog voltage changes continuously.

When the nMOS turns on, its nonlinear channel provides a discharge path.
An `RON × C` estimate is useful but approximate because `RON` changes as
`VGS` and `VDS` change.

---

## 7. pMOS: complementary polarity, not “backward nMOS”

A p-channel MOSFET is naturally described relative to its source, usually
connected toward the high rail:

```text
VSG = VS - VG
VSD = VS - VD
```

It turns on strongly when `VSG` exceeds the magnitude of its threshold:

```text
gate LOW relative to source  → pMOS on
gate HIGH near source        → pMOS off
```

This opposite control polarity makes pMOS useful as a pull-up:

```text
VDD ── pMOS ── output
```

Do not infer that source and drain labels can always be swapped without
consequence. MOS structures can be geometrically similar, but body
connections, parasitic diodes, bias conditions, and model conventions matter.

Electron mobility is generally higher than hole mobility, so for the same
geometry an nMOS is usually stronger than a pMOS. CMOS libraries often make
the pMOS wider to balance rising and falling behavior. “Wider” also increases
capacitance, area, and dynamic energy; sizing is a trade-off.

---

## 8. The gate terminal draws no DC current—almost

The ideal insulated gate suggests zero input current. For digital reasoning,
the better sentence is:

> A MOS gate ideally draws negligible steady-state conductive current, but it
> has capacitance and real devices leak.

Changing gate voltage requires charging or discharging gate capacitance.
Thus an upstream gate must deliver transient current. At rest, leakage can
come from subthreshold conduction, gate tunneling, reverse-biased junctions,
and other mechanisms. Leakage becomes especially important at high
transistor counts and small geometries.

This distinction will matter when discussing power:

```text
steady ideal gate input current       approximately zero
current while changing gate voltage   definitely not zero
real static leakage                   not zero
```

---

## 9. Lab — see the curve and then use the device

The lab is:

`04-cpu-and-chip-design/challenges/day-001-mosfet-switch-lab.cir`

It performs two analyses:

1. a DC sweep of gate voltage for nMOS devices held at several drain
   voltages;
2. a transient simulation of a resistor-loaded nMOS inverter.

It uses a deliberately simple Level 1 model. No external model files, GUI,
or network access are required.

### Predict

Write down:

1. At what approximate gate voltage should drain current become noticeable in
   this model?
2. Rank the three fixed-`VDS` current curves near `VGS = 1.0 V`.
3. When the pulse input rises, should `VOUT` rise or fall?
4. Which output transition should the resistor make relatively slow?
5. Will output LOW be exactly 0 V?

### Simulate

From the repository root:

```bash
mkdir -p /tmp/mosfet-switch-lab
ngspice -b \
  -o /tmp/mosfet-switch-lab/ngspice.log \
  04-cpu-and-chip-design/challenges/day-001-mosfet-switch-lab.cir
```

The netlist writes its data into `/tmp/mosfet-switch-lab/`:

```text
nmos-transfer.dat      text columns: VGS and drain-current curves
nmos-switch-transient.dat
ngspice.log            analysis messages and measured values
```

The `.dat` files are whitespace-separated numeric text suitable for a
spreadsheet, Python, gnuplot, or careful inspection with `less`. This avoids
requiring a graphical waveform viewer.

Inspect the first rows:

```bash
less /tmp/mosfet-switch-lab/nmos-transfer.dat
less /tmp/mosfet-switch-lab/nmos-switch-transient.dat
cat /tmp/mosfet-switch-lab/ngspice.log
```

Optional plot with gnuplot, if installed:

```bash
gnuplot -persist <<'PLOT'
set key left top
set xlabel "VGS (V)"
set ylabel "ID (A)"
plot "/tmp/mosfet-switch-lab/nmos-transfer.dat" \
  using 2:3 with lines title "VDS=0.05 V", \
  using 2:4 with lines title "VDS=0.50 V", \
  using 2:5 with lines title "VDS=3.30 V"
PLOT
```

With `wr_singlescale`, ngspice writes one analysis scale followed by the
requested vectors. The transfer columns are the DC sweep scale, `VGS`, and
the three drain currents. The header names them explicitly.

### Observe

Find evidence for each statement:

- current varies continuously rather than switching between only zero and one
  fixed value;
- the simple model's below-threshold current is effectively zero apart from
  ngspice's tiny numerical conductances because the model omits realistic
  subthreshold leakage;
- low `VDS` eventually limits current differently from high `VDS`;
- the transistor discharges output capacitance when on;
- the resistor restores the output HIGH when the transistor turns off;
- the rise and fall are not instantaneous or necessarily symmetric.

Use the measured rise and fall times in the log, but inspect the waveform data
too. A scalar measurement can conceal an unexpected waveform.

### Explain

Complete these sentences in your own words:

1. Raising `VGS` changes drain current because …
2. `VTH` is not a perfect digital boundary because …
3. `VOUT` changes only when charge moves because …
4. The output returns HIGH after nMOS turn-off because …
5. This resistor-loaded inverter wastes static power when LOW because …

---

## 10. Read the netlist as an engineering model

The most important lines are conceptually:

```spice
.model NDEV NMOS (LEVEL=1 VTO=0.70 KP=200u ...)
Mcurve drain gate 0 0 NDEV W=10u L=1u
```

SPICE MOSFET terminals are ordered:

```text
Mname drain gate source body model parameters
```

Getting source/body order wrong can produce a simulation that runs but models
the wrong circuit.

The fixed drain-voltage sources let ngspice measure current at selected
`VDS` values. The resistor-loaded output includes a capacitor:

```text
VDD → resistor → VOUT → capacitor and nMOS → ground
```

The capacitor does not mean output capacitance comes from one explicit
component in a chip. It stands in for drain junction capacitance, wiring,
downstream gate capacitance, and any intentional load.

### Model versus measurement

The simulation can establish what this netlist and model predict. It cannot
establish:

- the characteristics of a particular manufactured transistor;
- safe voltages for arbitrary hardware;
- a modern process's leakage or short-channel behavior;
- exact CPU gate delay.

Simulation is an executable consequence of assumptions. Always name the
assumptions.

---

## 11. Bridge quickly back to CPU and software

A processor does not expose `VGS` or drain current to normal software.
Designers compose transistors into characterized gates, gates into datapaths
and storage, and those blocks into an implementation of an ISA. Yet every
architectural state change still rests on the mechanism from this lesson:

```text
software instruction
  → decoded control signals
  → selected transistor networks conduct
  → charge moves on internal nodes
  → voltages settle into valid ranges
  → a clocked element captures new state
```

For example, an integer add can switch operand-selection logic, many adder
stages, result-routing logic, and register-write controls. The transistors do
not understand addition. Their local currents collectively make the voltage
pattern that the architecture calls a result.

This explains three system facts we will revisit:

- switching more capacitance costs more energy;
- finite current and capacitance limit clock speed;
- violating voltage, temperature, or timing assumptions can break the
  architectural abstraction and appear to software as incorrect execution.

We now leave single-device depth and move to restoring gates.

---

## 12. Explain, Draw, Observe, Build

### Explain

Without notes:

1. Explain how gate voltage controls an nMOS channel.
2. Distinguish cutoff, triode, and saturation in the teaching model.
3. Explain why a MOSFET is not an ideal switch.
4. Explain why the insulated gate still causes dynamic current upstream.
5. State pMOS turn-on conditions using source-relative voltage.
6. Explain why output voltage changes require movement of charge.

### Draw

Draw and label:

1. an nMOS with drain, gate, source, and body;
2. `VGS`, `VDS`, and conventional drain current;
3. qualitative `ID` versus `VGS` curves at three `VDS` values;
4. the resistor-loaded nMOS inverter with output capacitance;
5. charge entering the output node on a LOW-to-HIGH output transition and
   leaving on a HIGH-to-LOW transition.

### Observe

Submit:

- the exact ngspice command and version;
- both generated data files and the log;
- your five predictions;
- two correct predictions and one surprise;
- one limitation caused by the Level 1 model.

### Build

Make a copy in `/tmp`, leaving the course file unchanged:

```bash
cp 04-cpu-and-chip-design/challenges/day-001-mosfet-switch-lab.cir \
  /tmp/mosfet-switch-lab/experiment.cir
```

Choose one modification:

- double the output capacitance;
- double the pull-up resistance;
- halve nMOS width;
- change `VTO` while clearly recording that this changes the model.

Predict the direction of rise time, fall time, and steady output levels.
Edit the `/tmp` copy, rerun it, and explain the result. Do not search for one
magic “correct” delay; connect the changed parameter to current, charge, or
resistance.

---

## 13. Mastery checkpoint

Close the lesson and answer:

> How can a voltage at one terminal control a different current path, and how
> can that current path turn an analog node voltage into the physical basis of
> a bit?

A strong answer includes:

- electric-field control through an insulated gate;
- channel formation as a continuous device behavior;
- finite on conductance and nonzero off leakage in real devices;
- current moving charge into or out of node capacitance;
- voltage as a consequence of charge and capacitance;
- external pull-up or pull-down networks setting the final level;
- thresholds imposed by the next stage, not by nature declaring a bit.

If the answer says only “HIGH means on,” return to the transistor curves.

---

## Mental model at the end

```text
The gate does not send a Boolean command into the transistor.
A voltage creates an electric field.
The field changes channel charge and conductivity.
Conductivity permits current.
Current changes stored charge on circuit nodes.
Stored charge determines voltage.
Later stages interpret voltage ranges as logic.
```

The MOSFET is valuable precisely because this analog chain can be arranged so
that digital abstractions become robust.

---

## Why? notebook

1. Why is a MOSFET gate voltage-controlled even though changing it requires
   current?
2. Why does `VTH` not define a complete logic interface?
3. Why can an “on” nMOS be in either saturation or triode operation?
4. Why does a larger output capacitance increase transition time?
5. Why is an output node not automatically HIGH merely because its pull-down
   turned off?
6. Why does a resistor-loaded inverter consume static power in one state?
7. Why is pMOS described with `VSG` more naturally than a positive `VGS`?
8. Why can a simple SPICE model be useful even when it is not process
   accurate?
9. Why does a wider transistor usually drive more strongly, and what costs
   accompany width?
10. Where does the energy stored on an output capacitor go when an nMOS pulls
    the node LOW?

---

## References used selectively

Device and circuit foundations:

- R. Jacob Baker, *CMOS: Circuit Design, Layout, and Simulation*, 4th ed.,
  Wiley-IEEE Press, chapters 6–9. Used for MOSFET operating-region and
  inverter-level foundations.
- Neil H. E. Weste and David Harris, *CMOS VLSI Design: A Circuits and Systems
  Perspective*, 4th ed., chapters 2–3. Used for the switch, capacitance,
  delay, and CMOS-design mental models.
- Behzad Razavi, *Design of Analog CMOS Integrated Circuits*, 2nd ed.,
  chapter 2. Useful for a careful introduction to MOS device operation
  without requiring a fabrication course.

Primary/current tool references:

- ngspice project documentation and current manual, including batch mode,
  MOS Level 1 models, `dc`, `tran`, `meas`, and `wrdata`:
  <https://ngspice.sourceforge.io/docs.html>
- ngspice MOSFET model overview:
  <https://ngspice.sourceforge.io/docs/ngspice-html-manual/manual.xhtml>
- Nobel Prize scientific background on the transistor and integrated-circuit
  engineering lineage:
  <https://www.nobelprize.org/prizes/physics/1956/summary/>

Historical context:

- Computer History Museum, “The Silicon Engine,” for the development from
  transistor invention through MOS and integrated-circuit scaling:
  <https://www.computerhistory.org/siliconengine/>

The lab's generic Level 1 parameters are educational and are not taken from a
commercial process design kit.

**Next bridge:** pair an nMOS pull-down with a pMOS pull-up so each stable
state is actively restored, then measure where the resulting inverter treats
an input as safely LOW, uncertain, or safely HIGH.
