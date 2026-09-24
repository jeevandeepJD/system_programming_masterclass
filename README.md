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

Open [`WEEKLY_PROGRESS.md`](WEEKLY_PROGRESS.md). It records the current module,
unfinished evidence, and exact next action.

Current curriculum position:

- Stage 1 — Foundations of Computation
- Topic — Electricity, States, Bits, and Information
- Current module — Week 1

## Repository map

```text
.
├── README.md                 repository entry point
├── WEEKLY_PROGRESS.md        weekly module resume tracker
├── AGENT_CONTEXT.md          teaching workflow and decision log
├── MASTERCLASS.md            detailed historical handoff
├── Systems_Engineering_Masterclass_Curriculum_Tracker.docx
│                             authoritative curriculum tracker
├── weekly/
│   ├── week-001/
│   │   ├── overview.md       weekly cover, roadmap, and mastery goal
│   │   ├── lessons/          seven editable daily sections
│   │   └── challenges/       HTML, Python, C, and terminal labs
│   └── pdf/                  one consolidated PDF per week
├── tracker/                  topic-specific exercises and evidence
├── source-materials/
│   ├── README.md             catalog of selected references
│   └── library/              local third-party library; Git-ignored
└── bin/
    ├── build_lesson.py       Markdown-to-PDF renderer
    ├── build_week.py         overview + daily sections → one weekly PDF
    └── week                  weekly lesson/challenge launcher
```

## Sources of truth

1. `Systems_Engineering_Masterclass_Curriculum_Tracker.docx` controls the
   curriculum sequence, objectives, exercises, and mastery checks. Its
   original week labels describe ordering, not calendar deadlines.
2. `WEEKLY_PROGRESS.md` records where the learner actually stopped.
3. `AGENT_CONTEXT.md` records teaching preferences and operational decisions.
4. `MASTERCLASS.md` preserves detailed history and prior discoveries.

## Weekly use

Build and open the latest weekly PDF:

```bash
cd /path/to/masterclass
./bin/week lesson
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
python3 ./bin/generate_quizzes.py
```

Sections also include coding challenges when implementation provides useful
Build evidence. These starter labs use TODOs, prediction prompts, and
self-checks rather than presenting completed answers immediately.

Open or serve the optional challenge:

```bash
./bin/week challenge
```

List all available weekly artifacts:

```bash
./bin/week list
```

Build a particular week manually:

```bash
python3 ./bin/build_week.py 1
```

The weekly builder uses `pypdf` to preserve literal relative link annotations
inside the merged PDF (`python3-pypdf` on Fedora).

Select another week:

```bash
MC_WEEK=2 ./bin/week lesson
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
