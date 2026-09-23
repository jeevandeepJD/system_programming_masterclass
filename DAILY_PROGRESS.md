# Daily Learning Progress

This is the short resume tracker. Update it after every study session so the
next session begins from the learner's actual understanding rather than the
calendar.

The authoritative curriculum remains
`Systems_Engineering_Masterclass_Curriculum_Tracker.docx`.

## Current resume point

- **Branch:** `experiment/accelerated-foundations`
- **Purpose:** compare a denser early-course format before changing `master`
- **Current evaluation:** accelerated Day 1
- **Next action:** read accelerated Day 1 and compare its clarity, depth,
  pacing, and workload with the former three narrow lessons on `master`
- **Important:** prepared files do not assert learner mastery

## Accelerated sequence

### Day 1 — Foundations: From State to Meaning

- Lesson: `daily/lessons/day-001-foundations-from-state-to-meaning.md`
- PDF: `daily/pdf/day-001-foundations-from-state-to-meaning.pdf`
- Approximate workload: 3 hours
- Combines former narrow Days 1–3
- Focus: historical motivation, physical state, voltage ranges, binary,
  patterns, number systems, encoding, information, Linux observation, and
  integrated mastery

### Day 2 — Signed Numbers and Bitwise Reasoning

- Lesson: `daily/lessons/day-002-signed-numbers-and-bitwise-reasoning.md`
- PDF: `daily/pdf/day-002-signed-numbers-and-bitwise-reasoning.pdf`
- Challenges:
  - `daily/challenges/day-002-register-mask-lab.c`
  - `daily/challenges/day-002-overflow-lab.c`
- Approximate workload: 3 hours
- Combines former narrow Days 4–7
- Focus: signed encodings, two's complement, bitwise operations, register
  fields, overflow, flags, extension, C behavior, and ALU bridge

### Day 3 — Boolean Logic and Physical Gates

- Lesson: `daily/lessons/day-003-boolean-logic-and-physical-gates.md`
- PDF: `daily/pdf/day-003-boolean-logic-and-physical-gates.pdf`
- Challenge: `daily/challenges/day-003-logic-gate-lab.html`
- Approximate workload: 3 hours
- Focus: Boole, Shannon, relays, transistors, truth tables, six gates, De
  Morgan's laws, universal gates, combinational logic, half-adder preview,
  and CPU/ALU bridge

### Day 4 — Adders, Selection, and Memory

- Lesson: `daily/lessons/day-004-adders-selection-and-memory.md`
- PDF: `daily/pdf/day-004-adders-selection-and-memory.pdf`
- Challenge: `daily/challenges/day-004-digital-circuits-lab.html`
- Approximate workload: 3 hours
- Focus: half/full adders, ripple carry, mux/decoder/encoder, feedback,
  latches/flip-flops/clocks/registers, and a physical-to-logical 3+5 trace

### Day 5 — Building a Minimal CPU

- Lesson: `daily/lessons/day-005-building-a-minimal-cpu.md`
- PDF: `daily/pdf/day-005-building-a-minimal-cpu.pdf`
- Challenge: `daily/challenges/day-005-tiny-cpu.py`
- Approximate workload: 3 hours
- Focus: architectural state, clocked state updates, PC/IR/registers/ALU,
  datapath versus control, tiny ISA design, fetch/decode/execute, and
  cycle-by-cycle instruction traces

### Day 6 — Instruction Sets and Machine Code

- Lesson: `daily/lessons/day-006-isa-and-machine-code.md`
- PDF: `daily/pdf/day-006-isa-and-machine-code.pdf`
- Challenges:
  - `daily/challenges/day-006-fictional-isa.py`
  - `daily/challenges/day-006-machine-code-lab.c`
- Approximate workload: 3 hours
- Focus: ISA versus microarchitecture, encoding, opcodes/operands/immediates,
  addressing, load/store, careful RISC/CISC comparison, x86-64/RISC-V
  sequences, ELF disassembly, and fictional ISA design

### Day 7 — Control Flow, Exceptions, and Privilege

- Lesson: `daily/lessons/day-007-control-flow-exceptions-and-privilege.md`
- PDF: `daily/pdf/day-007-control-flow-exceptions-and-privilege.pdf`
- Challenge: `daily/challenges/day-007-exception-flow-lab.c`
- Approximate workload: 3 hours
- Focus: flags/branches/loops, calls preview, privilege, interrupts versus
  synchronous exceptions, x86 fault/trap/abort classification, vectoring,
  saved context, handler return, signals, and system-call transitions

Days 1–7 now form one complete accelerated material set. They are prepared
ahead; no day is marked complete until learner evidence is recorded.

## Pacing comparison checkpoint

Both branches now reach the same Boolean-logic mastery checkpoint:

- Accelerated branch: Day 3, one approximately three-hour lesson.
- Master branch: Days 8–10, three 90–120-minute lessons.

Compare explanation quality, fatigue, retention after a break, and completion
of hands-on evidence—not page count alone. No branch is approved for merging
until the learner decides explicitly.

## Current mastery target

Explain, without notes:

> Why can the same bit pattern represent a number, character, instruction, or
> pixel?

Required evidence:

- [ ] Explain why voltage ranges and noise margins matter
- [ ] Derive why `n` independent bits produce `2ⁿ` patterns
- [ ] Complete and check the 25 conversions
- [ ] Draw physical state → logical bit → pattern → interpretation → meaning
- [ ] Reproduce the Linux one-pattern/multiple-interpretations experiment
- [ ] Explain the mastery checkpoint aloud without notes
- [ ] Record confidence, time spent, status, and reflection here

## Daily log

### Accelerated Day 1 evaluation

- Lesson sections completed:
- Linux observation completed:
- Integrated challenge completed:
- Compared with narrow format: yes / no
- Natural tone preserved: yes / partly / no
- Historical reasoning sufficient: yes / partly / no
- Workload manageable in one study day: yes / too short / too long
- Material that felt rushed:
- Material that still felt repetitive:
- Decision: merge accelerated branch / revise it / keep original format

## Session update template

Copy this block after each study session:

```markdown
### Day N — Lesson title

- Lesson sections completed:
- Challenge/lab completed:
- Predictions versus observations:
- Mistakes and why they happened:
- Strongest idea:
- Remaining confusion:
- Why? notebook additions:
- Can explain mastery checkpoint without notes: yes / partly / no
- Confidence (self-rated): __/5
- Time spent:
- Exact next action:
```
