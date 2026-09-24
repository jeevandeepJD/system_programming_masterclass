# Systems Engineering Masterclass

> **Official format on `master`: weekly mastery modules**
>
> Each week contains one consolidated PDF, seven maintainable daily source
> sections, interactive theory checks, and practical labs. Earlier pacing
> experiments remain preserved as `backup/narrow-daily` and
> `backup/accelerated-daily`.

A first-principles, experiment-driven path from physical state and digital
logic to CPUs, C, operating systems, the Linux kernel, drivers,
virtualization, distributed systems, cloud platforms, and AI infrastructure.

The learning loop is:

> **question → learn enough → predict → build/run → observe → explain → connect**

The course also follows the historical problem-solving path: counting and
mechanical calculation → codes and punched media → Boolean logic and
switching → relays and electronic switches → arithmetic and memory circuits →
stored instructions → CPU and instruction sets. History is used to explain
why each abstraction became necessary, not as a list of dates to memorize.

## Resume here

Open [`metadata/WEEKLY_PROGRESS.md`](metadata/WEEKLY_PROGRESS.md). It records the current module,
unfinished evidence, and exact next action.

Current curriculum position:

- Stage 1 — Foundations of Computation
- Topic — Electricity, States, Bits, and Information
- Current module — Week 1

## Approved next direction

After the three prepared review modules:

1. Create `04-cpu-and-chip-design/`.
2. Progress through MOSFET/CMOS, gates, delay/power, sequential timing,
   SystemVerilog, simulation, synthesis, ALU/register-file design, and a
   simple RISC-V CPU.
3. Trace a compiled C task through instructions, datapath control, gates, and
   transistor-level voltage/charge changes.
4. Begin the guided x86-64 ToyOS build-along after the CPU/RTL foundations.

The chip-design and ToyOS tracks meet at the ISA boundary:

```text
transistors → gates → RTL → CPU
                           ↓ ISA
C / assembly → kernel → ToyOS
```

## Repository map

```text
.
├── README.md
├── 01-foundations-to-cpu/
│   ├── week-01-material.pdf
│   ├── overview.md
│   ├── sections/
│   └── challenges/
├── 02-assembly-memory-and-c/
│   ├── week-02-material.pdf
│   ├── overview.md
│   ├── sections/
│   └── challenges/
├── 03-c-toolchain-and-startup/
│   ├── week-03-material.pdf
│   ├── overview.md
│   ├── sections/
│   └── challenges/
├── references/
│   ├── README.md             catalog of selected references
│   └── library/              local third-party library; Git-ignored
├── tools/
│   ├── build_lesson.py       Markdown-to-PDF renderer
│   ├── build_week.py         overview + sections → one topic PDF
│   ├── generate_quizzes.py   theory-check generator
│   └── week                  lesson/challenge launcher
├── metadata/
│   ├── AGENT_CONTEXT.md
│   ├── MASTERCLASS.md
│   ├── WEEKLY_PROGRESS.md
│   └── curriculum-tracker.docx
└── archive/                  superseded prototypes and context PDF
```

## Sources of truth

1. `metadata/curriculum-tracker.docx` controls the
   curriculum sequence, objectives, exercises, and mastery checks. Its
   original week labels describe ordering, not calendar deadlines.
2. `metadata/WEEKLY_PROGRESS.md` records where the learner actually stopped.
3. `metadata/AGENT_CONTEXT.md` records teaching preferences and operational decisions.
4. `metadata/MASTERCLASS.md` preserves detailed history and prior discoveries.

## Weekly use

Build and open the latest weekly PDF:

```bash
cd /path/to/masterclass
./tools/week lesson
```

Each daily section in the consolidated PDF ends with links to its relevant
challenges. C, assembly, Python, and shell labs include copy-paste commands
that use repository-relative paths. Every challenge—including HTML labs—has
a clickable path relative to the PDF's location, so it remains portable when
the repository is moved to another device. GitHub links remain available for
source viewing.

Each daily section also includes a self-contained HTML theory check with four
MCQs. Regenerate all theory checks after editing `quizzes.json`:

```bash
python3 ./tools/generate_quizzes.py
```

Sections also include coding challenges when implementation provides useful
Build evidence. These starter labs use TODOs, prediction prompts, and
self-checks rather than presenting completed answers immediately.

Open or serve the optional challenge:

```bash
./tools/week challenge
```

List all available weekly artifacts:

```bash
./tools/week list
```

Build a particular week manually:

```bash
python3 ./tools/build_week.py 1
```

The weekly builder uses `pypdf` to preserve literal relative link annotations
inside the merged PDF (`python3-pypdf` on Fedora).

Select another week:

```bash
MC_WEEK=2 ./tools/week lesson
```

## Completion standard

A topic is not complete merely because its lesson was read. Major ideas
should pass four tests:

- **Explain** — derive and explain the idea without notes.
- **Draw** — reconstruct the important flow or structure.
- **Observe** — demonstrate it on Linux, hardware, or a simulator.
- **Build** — implement or simulate a simplified form.

The learner—not the agent—records confidence, hours, status, and mastery.

## Reference policy

The local source-material library may be used while preparing lessons, but it
is not strict or automatically authoritative. Outdated details should be
replaced or checked against current primary sources: kernel documentation and
source, architecture manuals, standards, RFCs, and official project
documentation.

Weekly lessons should cite specific chapters/pages or current web sources when
they materially rely on them.
