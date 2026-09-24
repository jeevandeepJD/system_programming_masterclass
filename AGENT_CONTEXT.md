# Systems Engineering Masterclass — Agent Context

## Purpose

This file is the persistent operating framework for any AI agent continuing
the Systems Engineering Masterclass.

It is **not model training code** and does not modify an AI model's weights.
It is a durable context and decision log that should be read at the beginning
of each masterclass session.

Use it together with:

1. `Systems_Engineering_Masterclass_Curriculum_Tracker.docx` — authoritative
   curriculum order, objectives, exercises, and mastery checkpoints.
2. `masterclass/MASTERCLASS.md` — detailed historical handoff and learning
   record.
3. This file — current operating rules, preferences, suggestions, and updates.

## Required agent workflow

Before creating or continuing a lesson:

1. Read this file.
2. Read the current checkpoint in `MASTERCLASS.md`.
3. Consult the relevant week in the authoritative `.docx` tracker.
4. Inspect existing weekly lessons and challenges to avoid repetition.
5. Read `WEEKLY_PROGRESS.md` for the exact resume point and unfinished work.
6. Continue from demonstrated understanding, not merely from the calendar.

After every user suggestion, correction, preference, or planning update:

1. Apply it to the work immediately when appropriate.
2. Append a short entry to the **Decision and Update Log** below.
3. Update the consolidated rules in this file if the change affects future
   sessions.
4. Update `MASTERCLASS.md` only when learning progress or the broader
   curriculum checkpoint meaningfully changes.

Do not claim that a preference is saved unless the relevant Markdown file was
actually updated.

## Personal GitHub backup

The active masterclass checkout is a git repo with remote `jdssh`:

`git@github.com:jeevandeepJD/system_programming_masterclass.git`

After a **major milestone** (completed week, mastery checkpoint, or similarly
durable learning snapshot), commit the relevant masterclass files if needed
and push to `jdssh` for personal reference:

```bash
git push jdssh master
```

Do not push every small lesson edit. Do not push secrets, private keys, or
unrelated home-directory files.

## Curriculum authority

- The `.docx` tracker is the source of truth for sequence and scope.
- The course restarted from the first curriculum topic.
- Tracker week labels define curriculum order only; they are not deadlines or
  assumptions about how many calendar days the learner studies.
- Earlier out-of-order work remains useful historical evidence but does not
  automatically complete tracker weeks.
- A week advances through understanding and evidence, not elapsed time.
- User-entered confidence, hours, status, and mastery remain the user's honest
  self-assessment; do not invent or auto-fill them.

## Weekly module workflow

For each curriculum week/module:

1. Create a coherent lesson as Markdown under:
   `weekly/lessons/`
2. Render a sequence-numbered PDF under:
   `weekly/pdf/`
3. Open the PDF in the editor.
4. When useful, create reinforcement under:
   `weekly/challenges/`
5. Choose the activity format based on the concept:
   - interactive HTML simulation,
   - quiz,
   - coding or debugging problem,
   - terminal experiment,
   - tracing or diagram exercise,
   - prediction-and-observation challenge.
6. Interactive material is optional. Do not force HTML when another format
   teaches the concept better.
7. Prefer the loop:
   **predict → run/observe → explain → connect to the larger system**.
8. Do not reveal challenge solutions before the learner attempts them unless
   requested.
9. A week is a mastery module, not a seven-day deadline. Preserve the exact
   stopping point when the learner needs more time.

## Teaching and writing style

- Teach from first principles: WHY before HOW.
- Write like a knowledgeable systems mentor at a whiteboard.
- Keep notes natural, conversational, varied, and easy to read.
- Do not make lessons monotonic, bland, or mechanically templated.
- Begin with a motivating question or concrete problem.
- Introduce terminology only after establishing why the idea is needed.
- Mix short passages, diagrams, examples, predictions, and deeper technical
  explanations.
- Use plain language without sacrificing correctness.
- Connect concepts across layers:
  physics → logic → CPU → machine code → C → OS → kernel → hardware.
- Connect to Linux, kernel, QEMU/KVM, and virtualization experience where it
  genuinely clarifies the topic.
- Encourage “Where does that actually exist?” and similar questions.
- Distinguish specification, encoding, physical implementation, runtime
  state, and software policy.
- Enthusiasm should come from discovery, not artificial praise or excessive
  punctuation.

## Mastery framework

For major concepts, gather evidence across four dimensions:

1. **Explain** — explain it from first principles without notes.
2. **Draw** — draw the complete path or relationship.
3. **Observe** — demonstrate it on Linux, hardware, or a simulator.
4. **Build** — implement or simulate a simplified version.

Use exercises to expose gaps rather than merely generate a score.

## Current checkpoint

- Stage 1 — Foundations of Computation
- Topic — Electricity, States, Bits, and Information
- Active branch: `master`
- Current module: Week 1 — From Physical State to CPU Control
- Week 1 contains seven daily sections in one consolidated PDF.
- Weekly modules are the approved official format.

## Decision and Update Log

### 2026-09-15 — Curriculum authority

The user selected `Systems_Engineering_Masterclass_Curriculum_Tracker.docx`
as the authoritative 66-week plan and asked to restart from the beginning.

### 2026-09-16 — Daily lesson format

The user requested one daily lesson PDF displayed in the editor, followed by
hands-on reinforcement when useful. Challenges may be HTML, quizzes, coding,
terminal work, or another suitable format; interactivity is optional.

### 2026-09-16 — Writing tone

The user requested natural, engaging, easy-to-understand notes rather than
monotonic or bland textbook prose.

### 2026-09-16 — Personal GitHub backup

The user linked the masterclass repository to
`git@github.com:jeevandeepJD/system_programming_masterclass.git` (remote
`jdssh`) and asked to push after major milestones for personal reference.

### 2026-09-16 — Persistent update framework

The user requested a Markdown context/framework file that records every
suggestion and update. This file was created as the append-only operational
decision log. Future agents must update it after each such instruction.

### 2026-09-16 — Git repository linked

The user reported that the `masterclass` folder is now linked to their Git
repository. Treat lesson sources, challenges, tracker artifacts, context
files, and supporting scripts as version-controlled work. Do not create
commits, push changes, rewrite history, or modify remotes unless the user
explicitly asks.

### 2026-09-16 — Daily Git publication workflow

The user requested that all relevant daily generated material be pushed:
lesson Markdown, rendered PDFs, optional challenges, exercises, scripts, and
context updates. Before publishing each daily batch, show or determine the
pending changes and let the user choose whether they should become a separate
commit or be incorporated into the current HEAD. Never assume an amend,
squash, merge, or history rewrite. For the current update, the user selected
a **separate commit**.

### 2026-09-16 — Local source-material library

The user provided a ZIP and standalone learning materials in `~/Downloads`
and asked for curriculum-relevant items to be selected, renamed, and
organized in the masterclass folder.

- Local library: `source-materials/library/`
- Trackable catalog: `source-materials/README.md`
- The imported library is Git-ignored by default because it contains
  third-party books and code; do not push it without explicit user direction.
- Use these resources selectively for lessons. Cite exact chapter/page
  ranges instead of assigning whole books.
- Kernel/driver books may contain outdated APIs; verify examples against the
  current kernel documentation and source.
- The archive's nested Git metadata, swap files, executables, object files,
  shared libraries, and crash dumps were excluded.

### 2026-09-17 — Use source materials while preparing notes

The user explicitly approved referring to the organized source-material
library when preparing lessons. For each lesson:

- begin with the authoritative tracker objective, not a book's chapter order;
- consult the most relevant local book or lab collection;
- synthesize explanations in the masterclass's natural mentor tone rather
  than copying source text;
- cite the book and exact chapter/page range when a lesson materially relies
  on it;
- cross-check time-sensitive Linux/kernel details against current official
  documentation or source;
- continue using experiments and observed behavior as evidence rather than
  treating a book as unquestionable authority.

### 2026-09-17 — Source materials are flexible, not strict

The local books and labs are optional references, not curriculum constraints.
If they are outdated, incomplete, unclear, or conflict with current practice,
use up-to-date internet sources instead. Prefer primary sources such as:

- current Linux kernel documentation and source,
- current architecture/vendor manuals,
- standards, RFCs, and official project documentation,
- maintained upstream repositories and release notes.

Clearly distinguish stable foundational concepts from version-specific
details. Include source links and the relevant version/date in daily notes
when freshness matters. Do not browse merely to replace timeless
first-principles explanations; browse when recency or verification adds real
value.

### 2026-09-17 — Daily resume tracker and repository README

The user requested a daily tracker so study can resume exactly where it
stopped. `DAILY_PROGRESS.md` is now the concise source of truth for the
current daily position, pending evidence, questions, and next action. Update
it after every completed or interrupted study session. The root `README.md`
provides the repository entry point and directs readers to that tracker.

### 2026-09-17 — Day 2 continuity and Day 3 generation

The user requested Day 2 be regenerated with explicit continuity from Day 1,
and requested Day 3. Day 2 now retrieves Day 1's voltage-to-bit model before
introducing encoding and meaning. Day 3 is a mastery workshop rather than an
automatic jump to the next topic, because progression depends on evidence.

### 2026-09-17 — Sequence-only daily progression

The user may skip calendar days and requested that daily study artifacts use
only a continuous day count. Daily lessons, PDFs, and challenges now use
`day-NNN-*` names with no dates or week numbers. The launcher opens the
highest available day by default and accepts `MC_DAY=N` for an earlier day.
The curriculum tracker still controls topic order, but its week labels are
not a schedule. Progress displays should show **Day N + current topic**, not a
date or study-week number.

### 2026-09-17 — Seven-lesson preview

The user asked to see notes covering a full week-like span. Since daily
artifacts are sequence-based, Days 4–7 were prepared to complete a seven-day
preview together with existing Days 1–3. Prepared future lessons must not
move the learner's resume point automatically. `DAILY_PROGRESS.md` should
separate **current resume point** from **prepared next lessons**.

### 2026-09-17 — Historical problem-solving path to the modern CPU

The user wants more than descriptions of modern components. Lessons must
reconstruct the questions, constraints, and reasoning that led from early
calculation to modern computers:

- why humans wanted calculation to be mechanized;
- how counting tools, mechanical calculators, punched media, Boolean algebra,
  telegraphy, relays, vacuum tubes, transistors, and integrated circuits each
  solved a limitation of the previous approach;
- how arithmetic circuits emerge from switches and logic;
- why storage, a clock, control, and conditional behavior are needed;
- why stored programs and instruction sets exist;
- how a CPU performs simple calculations through datapath and control;
- how machine instructions, assembly, compilers, operating systems, and
  applications build on that machinery.

Every lesson should state the point of the current topic and connect it to the
end-to-end path. Historical material should explain engineering motivation,
not become a detached list of names and dates. Where historical attribution
is nuanced, avoid simplistic “one person invented the computer” stories.

Also preserve an important precision: a conventional computer does not
literally “think.” It executes physical state transitions according to
circuits and encoded instructions. Higher-level reasoning and AI emerge from
software, algorithms, learned parameters, and enormous compositions of those
simple operations.

### 2026-09-17 — Accelerated pacing for early foundations

The user finds the early daily units too small and wants roughly three of the
original narrow lessons combined into one study day so the course reaches
advanced systems material sooner. Increase early-day density while preserving
the causal chain and hands-on evidence. Do not simply concatenate three PDFs:
remove repetition, merge retrieval/mastery sections, and target a coherent
approximately three-hour concept-plus-lab unit. Acceleration may reduce
calendar time, but must not skip prerequisites or mark mastery without
evidence.

The user asked to preserve the existing natural tone, historical reasoning,
and detailed explanations. Rather than replacing the accepted material
directly, the accelerated version is being developed on
`experiment/accelerated-foundations` for comparison. The chosen experiment
maps original Days 1–3 into accelerated Day 1, original Days 4–7 into
accelerated Day 2, and begins Boolean logic on accelerated Day 3. Merge only
after explicit approval.

### 2026-09-22 — Additional Linux learning materials

The active checkout is currently `/home/jd/Desktop/masterclass`; scripts and
documentation should resolve paths relative to the repository instead of
assuming `~/masterclass`.

The user added seven files for source review. SHA-256 verification showed
that the CS:APP, OSTEP, and TLPI PDFs were exact duplicates of existing local
library copies, so those root-level duplicates were removed. Four unique and
relevant EPUBs were renamed and added to the ignored local library:

- *Hands-On System Programming with Linux* (Kaiwan Billimoria, 2018)
- *Linux Kernel Programming*, 2nd ed. (Kaiwan Billimoria, 2024)
- *Linux Kernel Programming Part 2: Character Device Drivers and Kernel
  Synchronization* (Kaiwan Billimoria, 2021)
- *Linux Kernel Debugging* (Kaiwan Billimoria, 2022)

Use the 2024 second edition as the strongest local kernel-programming
companion, while still checking current upstream documentation/source. Use
the Part 2 and Debugging books as targeted references for drivers,
interrupts, synchronization, tracing, sanitizers, Oops/panic, KGDB, and
kdump/crash. The source catalog contains the detailed curriculum mapping and
age caveats.

### 2026-09-22 — EPUB-to-PDF conversion

At the user's request, the four newly added EPUBs were converted to
searchable A4 PDFs with page numbers using Calibre 9.13.0. The original EPUBs
remain beside the PDFs. Calibre must be run with `PYTHONNOUSERSITE=1` on this
VM because a user-installed `lxml` version otherwise conflicts with Fedora's
`html5-parser`/system `libxml2`. Prefer the PDF paths in lesson citations so
page references remain stable.

### 2026-09-23 — Instructor-led Linux/kernel training set

The user added 18 PDFs from an earlier training course plus the authoritative
curriculum DOCX. The DOCX now lives at the repository root. The PDFs were
renamed and organized under `source-materials/library/training/kaiwantech/`:

- seven 2024 kernel-internals modules (01–04, 06, 08, 09);
- a four-part, mostly 2015-era memory-management series;
- cgroup slides and cgroup v2 notes;
- programmer checklist and kdump/crash notes;
- Makefile and Git references;
- Jonathan Corbet's OSS Europe 2023 kernel report.

Preserve the missing module numbers 05 and 07. Prefer the 2024 modules where
they overlap older material. Treat the memory-management series as
conceptual/legacy and verify all implementation details against current
kernel source. Treat the 2023 kernel report as a historical snapshot. The
source catalog contains exact curriculum mappings and freshness warnings.

### 2026-09-23 — Continue both pacing branches

The user needs more time to choose a pacing model and asked that lessons
continue on both branches. Keep their curriculum scope aligned so comparison
is fair:

- `master`: Days 8–10 split Boolean logic and logic gates into three
  90–120-minute lessons.
- `experiment/accelerated-foundations`: accelerated Day 3 covers the same
  Boolean-logic checkpoint in one approximately three-hour unit.

Do not merge either pacing model based only on file generation. Wait for the
learner's explicit preference after reviewing or attempting both.

### 2026-09-23 — Complete accelerated seven-day material set

At the user's request, the accelerated branch now contains Days 1–7:

1. physical state through representation and meaning;
2. signed numbers, bitwise reasoning, overflow, and ALU bridge;
3. Boolean logic and physical gates;
4. adders, mux/decoder/encoder, storage, clocking, and registers;
5. minimal CPU, tiny ISA, and fetch/decode/execute;
6. ISA/microarchitecture, instruction encoding, machine code, and
   x86-64/RISC-V comparison;
7. branches/loops, privilege, exceptions/interrupts, vectoring, system calls,
   and safe userspace observation.

Each is designed as an approximately three-hour concept-plus-lab unit.
Prepared material does not imply completion. Keep `master` and the
accelerated branch separate until the learner decides pacing explicitly.

### 2026-09-23 — Weekly modules replace daily lessons on experiment

The user prefers weekly organization over daily sequencing. The weekly
conversion was isolated on `experiment/weekly-modules`, preserving both
`master` and `experiment/accelerated-foundations`. Weekly artifacts live
under `weekly/`, progress is tracked in `WEEKLY_PROGRESS.md`, and `bin/week`
opens/builds a selected module using `MC_WEEK=N`.

“Week” means a mastery module, not a calendar deadline. The learner may take
more than seven days. Keep both daily branches unchanged while the weekly
structure is evaluated.

### 2026-09-23 — One PDF contains seven daily sections

The user clarified that “weekly” means one weekly PDF containing seven days'
worth of material—not relabeling each existing day as a separate week.
`experiment/weekly-modules` now groups the original accelerated Day 1–7
sources and labs under `weekly/week-001/`. `bin/build_week.py` renders the
weekly overview plus all seven daily sections and merges them into one
`weekly/pdf/week-001-*.pdf`. Keep daily source sections for maintainability,
but present one consolidated PDF to the learner.

### 2026-09-24 — Three complete weekly packages

After removing incomplete untracked Week 2–5 drafts from an interrupted
generation, the user requested three complete weeks. The weekly branch now
contains:

- Week 1: physical state through CPU control and privilege;
- Week 2: x86-64/RISC-V assembly, ABI, memory hierarchy/cache, layout/NUMA,
  and the C abstract machine;
- Week 3: pointers/lifetime/aggregates/atomics through compilation, linking,
  ELF loading, and program startup.

Each week has one overview, seven maintainable daily source sections, labs,
and one consolidated PDF. Prepared Weeks 2–3 do not advance the learner's
resume point or imply mastery.

### 2026-09-24 — Clickable challenge links in weekly PDFs

The user requested challenge links inside the PDFs. `bin/build_week.py` now
appends each day's relevant challenge links immediately after that daily
lesson section. C, assembly, Python, and shell labs include copy-paste
commands using repository-relative paths. Every challenge link is relative
to the consolidated PDF's location (for example,
`../week-001/challenges/day-002-overflow-lab.c`), including directly
openable HTML labs. A GitHub source link is retained as a fallback. Do not
embed machine-specific `file://` or `localhost` URLs. `bin/build_week.py`
uses `pypdf` (`python3-pypdf` on Fedora) to preserve literal relative URIs
after PDF merging.

Rebuild a weekly PDF after adding or renaming a challenge so its generated
links remain accurate.

### 2026-09-24 — Interactive theory checks after major sections

The user requested simple interactive trivia/MCQs after major theory
sections. Every daily section in Weeks 1–3 now has a generated,
self-contained `day-NNN-theory-check.html` with four concept-specific MCQs.
Answers and explanations remain hidden until submission.

Quiz source data lives in each module's `quizzes.json`;
`bin/generate_quizzes.py` regenerates the HTML. Treat quiz scores as
diagnostic retrieval evidence, not mastery. Weekly PDF generation places each
quiz link after its matching daily lesson alongside practical labs.

### 2026-09-24 — Weekly format promoted to master

The user approved the weekly format as the official course structure.
`experiment/weekly-modules` is promoted to `master`. Preserve earlier pacing
approaches as explicit backups:

- `backup/narrow-daily` — the former master branch;
- `backup/accelerated-daily` — the former accelerated-foundations branch.

Interactive theory checks use conversational labels such as “Now check your
understanding,” appear after the relevant lesson section, and keep practical
labs alongside them. Continue future work on `master` using weekly packages.
