# Weekly Learning Progress

This is the short resume tracker. Update it after every study session so the
next session begins from the learner's actual understanding rather than the
calendar.

The authoritative curriculum remains
`Systems_Engineering_Masterclass_Curriculum_Tracker.docx`.

## Current resume point

- **Branch:** `experiment/weekly-modules`
- **Purpose:** evaluate mastery-oriented weekly modules before changing
  `master`
- **Current module:** Week 1
- **Next action:** work through Week 1 at a comfortable pace, complete its
  observation/mastery evidence, and record the exact stopping point
- **Important:** prepared files do not assert learner mastery

## Accelerated sequence

### Week 1 — Foundations: From State to Meaning

- Lesson: `weekly/lessons/week-001-state-bits-and-meaning.md`
- PDF: `weekly/pdf/week-001-state-bits-and-meaning.pdf`
- Approximate workload: 3 hours
- Matches tracker Week 1 in one coherent module
- Focus: historical motivation, physical state, voltage ranges, binary,
  patterns, number systems, encoding, information, Linux observation, and
  integrated mastery

### Week 2 — Signed Numbers and Bitwise Reasoning

- Lesson: `weekly/lessons/week-002-signed-numbers-and-bitwise-reasoning.md`
- PDF: `weekly/pdf/week-002-signed-numbers-and-bitwise-reasoning.pdf`
- Challenges:
  - `weekly/challenges/week-002-register-mask-lab.c`
  - `weekly/challenges/week-002-overflow-lab.c`
- Approximate workload: 3 hours
- Matches tracker Week 2 in one coherent module
- Focus: signed encodings, two's complement, bitwise operations, register
  fields, overflow, flags, extension, C behavior, and ALU bridge

### Week 3 — Boolean Logic and Physical Gates

- Lesson: `weekly/lessons/week-003-boolean-logic-and-physical-gates.md`
- PDF: `weekly/pdf/week-003-boolean-logic-and-physical-gates.pdf`
- Challenge: `weekly/challenges/week-003-logic-gate-lab.html`
- Approximate workload: 3 hours
- Focus: Boole, Shannon, relays, transistors, truth tables, six gates, De
  Morgan's laws, universal gates, combinational logic, half-adder preview,
  and CPU/ALU bridge

### Week 4 — Adders, Selection, and Memory

- Lesson: `weekly/lessons/week-004-adders-selection-and-memory.md`
- PDF: `weekly/pdf/week-004-adders-selection-and-memory.pdf`
- Challenge: `weekly/challenges/week-004-digital-circuits-lab.html`
- Approximate workload: 3 hours
- Focus: half/full adders, ripple carry, mux/decoder/encoder, feedback,
  latches/flip-flops/clocks/registers, and a physical-to-logical 3+5 trace

### Week 5 — Building a Minimal CPU

- Lesson: `weekly/lessons/week-005-building-a-minimal-cpu.md`
- PDF: `weekly/pdf/week-005-building-a-minimal-cpu.pdf`
- Challenge: `weekly/challenges/week-005-tiny-cpu.py`
- Approximate workload: 3 hours
- Focus: architectural state, clocked state updates, PC/IR/registers/ALU,
  datapath versus control, tiny ISA design, fetch/decode/execute, and
  cycle-by-cycle instruction traces

### Week 6 — Instruction Sets and Machine Code

- Lesson: `weekly/lessons/week-006-isa-and-machine-code.md`
- PDF: `weekly/pdf/week-006-isa-and-machine-code.pdf`
- Challenges:
  - `weekly/challenges/week-006-fictional-isa.py`
  - `weekly/challenges/week-006-machine-code-lab.c`
- Approximate workload: 3 hours
- Focus: ISA versus microarchitecture, encoding, opcodes/operands/immediates,
  addressing, load/store, careful RISC/CISC comparison, x86-64/RISC-V
  sequences, ELF disassembly, and fictional ISA design

### Week 7 — Control Flow, Exceptions, and Privilege

- Lesson: `weekly/lessons/week-007-control-flow-exceptions-and-privilege.md`
- PDF: `weekly/pdf/week-007-control-flow-exceptions-and-privilege.pdf`
- Challenge: `weekly/challenges/week-007-exception-flow-lab.c`
- Approximate workload: 3 hours
- Focus: flags/branches/loops, calls preview, privilege, interrupts versus
  synchronous exceptions, x86 fault/trap/abort classification, vectoring,
  saved context, handler return, signals, and system-call transitions

Weeks 1–7 now form one complete accelerated material set. They are prepared
ahead; no week is marked complete until learner evidence is recorded.

## Pacing comparison checkpoint

Both branches now reach the same Boolean-logic mastery checkpoint:

- Accelerated branch: Week 3, one approximately three-hour lesson.
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

## Weekly log

### Accelerated Week 1 evaluation

- Lesson sections completed:
- Linux observation completed:
- Integrated challenge completed:
- Compared with narrow format: yes / no
- Natural tone preserved: yes / partly / no
- Historical reasoning sufficient: yes / partly / no
- Workload manageable as one weekly module: yes / too short / too long
- Material that felt rushed:
- Material that still felt repetitive:
- Decision: merge accelerated branch / revise it / keep original format

## Session update template

Copy this block after each study session:

```markdown
### Week N — Lesson title

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
