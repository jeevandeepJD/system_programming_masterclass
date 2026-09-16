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
4. Inspect existing daily lessons and challenges to avoid repetition.
5. Continue from demonstrated understanding, not merely from the calendar.

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

The `~/masterclass` folder is a git repo with remote `jdssh`:

`git@github.com:jeevandeepJD/system_programming_masterclass.git`

After a **major milestone** (completed week, mastery checkpoint, or similarly
durable learning snapshot), commit the relevant masterclass files if needed
and push to `jdssh` for personal reference:

```bash
git -C ~/masterclass push jdssh master
```

Do not push every small lesson edit. Do not push secrets, private keys, or
unrelated home-directory files.

## Curriculum authority

- The `.docx` tracker is the source of truth for sequence and scope.
- The course restarted from Week 1 on 15 September 2026.
- Earlier out-of-order work remains useful historical evidence but does not
  automatically complete tracker weeks.
- A week advances through understanding and evidence, not elapsed time.
- User-entered confidence, hours, status, and mastery remain the user's honest
  self-assessment; do not invent or auto-fill them.

## Daily lesson workflow

For each study day:

1. Create a focused lesson as Markdown under:
   `~/masterclass/daily/lessons/`
2. Render a dated PDF under:
   `~/masterclass/daily/pdf/`
3. Open the PDF in the editor.
4. When useful, create reinforcement under:
   `~/masterclass/daily/challenges/`
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
- Week 1 — Electricity, States, Bits, and Information
- Current daily position: Week 1, Day 2
- Day 1 and Day 2 lesson PDFs exist.
- Day 2 has an optional interactive Bit Lab.
- Do not advance until the learner has read/attempted the material and shared
  questions or evidence.

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

The user linked `~/masterclass` to
`git@github.com:jeevandeepJD/system_programming_masterclass.git` (remote
`jdssh`) and asked to push after major milestones for personal reference.

### 2026-09-16 — Persistent update framework

The user requested a Markdown context/framework file that records every
suggestion and update. This file was created as the append-only operational
decision log. Future agents must update it after each such instruction.
