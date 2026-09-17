# Systems Engineering Masterclass

A first-principles, experiment-driven path from physical state and digital
logic to CPUs, C, operating systems, the Linux kernel, drivers,
virtualization, distributed systems, cloud platforms, and AI infrastructure.

The learning loop is:

> **question → learn enough → predict → build/run → observe → explain → connect**

## Resume here

Open [`DAILY_PROGRESS.md`](DAILY_PROGRESS.md). It records the current lesson,
unfinished evidence, and exact next action.

Current curriculum position:

- Stage 1 — Foundations of Computation
- Week 1 — Electricity, States, Bits, and Information
- Day 3 — Week 1 mastery workshop

## Repository map

```text
.
├── README.md                 repository entry point
├── DAILY_PROGRESS.md         short daily resume tracker
├── AGENT_CONTEXT.md          teaching workflow and decision log
├── MASTERCLASS.md            detailed historical handoff
├── daily/
│   ├── lessons/              editable Markdown lessons
│   ├── pdf/                  rendered daily lesson PDFs
│   └── challenges/           optional HTML, Python, coding, or lab exercises
├── tracker/                  week-specific exercises and evidence
├── source-materials/
│   ├── README.md             catalog of selected references
│   └── library/              local third-party library; Git-ignored
└── bin/
    ├── build_lesson.py       Markdown-to-PDF renderer
    └── today                 daily lesson/challenge launcher
```

## Sources of truth

1. `Systems_Engineering_Masterclass_Curriculum_Tracker.docx` controls the
   66-week curriculum sequence, objectives, exercises, and mastery checks.
2. `DAILY_PROGRESS.md` records where the learner actually stopped.
3. `AGENT_CONTEXT.md` records teaching preferences and operational decisions.
4. `MASTERCLASS.md` preserves detailed history and prior discoveries.

## Daily use

Build and open the lesson for the current date:

```bash
~/masterclass/bin/today lesson
```

Open or serve the optional challenge:

```bash
~/masterclass/bin/today challenge
```

List all available daily artifacts:

```bash
~/masterclass/bin/today list
```

Build a particular lesson manually:

```bash
python3 ~/masterclass/bin/build_lesson.py \
  ~/masterclass/daily/lessons/YYYY-MM-DD-lesson-name.md
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

Daily lessons should cite specific chapters/pages or current web sources when
they materially rely on them.
