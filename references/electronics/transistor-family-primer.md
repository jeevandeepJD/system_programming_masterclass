# Transistor Family Primer — What Changed, and Why CPUs Use CMOS

This is a curiosity appendix, not a required electronics detour. Its purpose
is to place the MOSFET used in the CPU lessons among the other transistor
families you may remember.

## First, what job are we asking the device to do?

A transistor can be used as:

- a controlled switch for digital logic;
- a small-signal amplifier;
- a power-control device;
- an analog current source or active load;
- part of an oscillator, regulator, sensor interface, or radio circuit.

There is no universally “best transistor.” Device choice follows voltage,
current, frequency, gain, input-drive, density, power, manufacturing, and cost
requirements. A CPU asks for billions of tiny switches that can be fabricated
together, switch quickly, and consume little static power. That requirement
strongly favors CMOS.

## BJT — bipolar junction transistor

An NPN or PNP BJT uses both electron and hole transport—hence *bipolar*. A
small change in base-emitter voltage causes an exponential change in
collector current. Introductory circuit analysis often describes a BJT as
“current controlled” because useful collector current normally requires base
current:

```text
base drive → collector-emitter conduction
```

That phrase is a circuit-model convenience, not the complete device physics.
BJTs provide useful gain and transconductance and remain important in analog,
RF, high-speed, and mixed-signal circuits. Bipolar logic families also existed,
including TTL and ECL. Their drive and static-power trade-offs are less suited
to packing modern general-purpose CPUs at CMOS-scale density.

## JFET — junction field-effect transistor

A JFET controls a semiconductor channel by expanding or shrinking a
reverse-biased junction depletion region. Its gate normally draws very little
steady current. A common JFET is naturally conducting at zero gate-source
voltage, so an applied gate voltage pinches the channel toward cutoff:

```text
zero gate bias → channel normally available
reverse gate bias → depletion region grows → channel narrows
```

JFETs remain useful where low noise, high input impedance, or particular analog
behavior matters. They are not the dominant device for dense complementary
digital logic.

## MOSFET — metal-oxide-semiconductor field-effect transistor

The MOSFET controls a channel through an insulated gate. Changing gate voltage
moves charge and establishes an electric field; ideally, maintaining a steady
gate voltage needs almost no DC gate current.

### Enhancement and depletion modes

- An **enhancement-mode** MOSFET is normally off at zero gate-source bias and
  requires the appropriate gate voltage to create or strengthen a channel.
  Modern CMOS logic primarily uses complementary enhancement-mode NMOS and
  PMOS devices.
- A **depletion-mode** MOSFET has a channel at zero gate-source bias and needs
  opposite-polarity control to reduce conduction. It appears in specialized
  circuits and historical logic styles but is not the default CMOS switch in
  this course.

### NMOS and PMOS together

CMOS pairs complementary devices:

```text
input low:   PMOS pull-up conducts, NMOS pull-down stops
input high:  NMOS pull-down conducts, PMOS pull-up stops
```

Ideally, a stable inverter output has no direct conducting path from supply to
ground. Real CMOS still consumes leakage power and consumes dynamic energy
while capacitances charge and discharge. CMOS won because complementary
restoring logic combines manufacturability, density, noise margin, and
relatively low static power.

## Planar MOSFET, FinFET, and GAAFET

These names describe evolving MOSFET geometries, not three unrelated Boolean
devices.

### Planar MOSFET

In a planar device, the channel lies near the flat silicon surface and the
gate controls it mainly from one side. As dimensions shrank, electrostatic
control became harder: leakage and short-channel effects increased.

### FinFET

A FinFET raises the channel into a narrow fin. The gate wraps around multiple
sides of that fin:

```text
planar: gate over channel
FinFET: gate around the fin's sides and top
```

The stronger electrostatic control reduces leakage and supports continued
scaling. Drive strength can be adjusted partly through the number and geometry
of fins. To software, it is still hidden beneath the same logic-gate and ISA
abstractions.

### GAAFET / nanosheet transistor

A gate-all-around FET surrounds the channel more completely. Modern forms may
use stacked horizontal nanosheets or nanowires. Greater gate coverage improves
electrostatic control as dimensions continue shrinking.

FinFET and GAAFET technology changes delay, leakage, voltage, density,
variability, and manufacturing constraints. It does not change the meaning of
an `ADD` instruction. Those effects influence the physical implementation
that must satisfy the architecture.

## Power-device relatives

Power MOSFETs and IGBTs are optimized for switching substantial voltage and
current in converters, motor drives, and power systems. An IGBT combines a
MOS-gated input with bipolar conduction characteristics. These devices solve a
different problem from dense on-chip CPU logic: safely controlling energy
rather than integrating billions of minimum-size logic switches.

## The useful systems-programmer mental model

```text
device family and geometry
  → voltage/current/leakage/delay characteristics
  → logic-gate and storage-cell implementation
  → datapath, cache, clock, and power constraints
  → microarchitecture
  → ISA-visible behavior
  → compiler, kernel, and applications
```

Kernel code normally observes the upper layers: architectural registers,
memory ordering, timers, performance counters, thermal/frequency behavior,
machine checks, and device interfaces. The transistor family influences those
constraints, but software does not command an individual FinFET or GAAFET.

## Keep these distinctions

- **BJT, JFET, and MOSFET** identify different device structures and control
  behavior.
- **NMOS and PMOS** are complementary MOSFET polarities used together in CMOS.
- **Enhancement and depletion** describe whether a MOS channel is normally
  absent or present under the reference bias.
- **FinFET and GAAFET** are improved MOSFET geometries for electrostatic
  control and scaling.
- **CMOS** is a complementary circuit-design approach, not one transistor.
- None of these names alone determines CPU performance; circuit,
  microarchitecture, process, packaging, memory, workload, cooling, and
  software all matter.

## Selected references

- Patt and Patel, *Introduction to Computing Systems*, device-to-logic
  sections for the digital abstraction.
- R. Jacob Baker, *CMOS: Circuit Design, Layout, and Simulation*, selected
  MOSFET and CMOS chapters.
- Weste and Harris, *CMOS VLSI Design*, selected device, inverter, scaling,
  and circuit chapters.
- Current foundry or processor-vendor technology explanations when a specific
  process generation matters.
