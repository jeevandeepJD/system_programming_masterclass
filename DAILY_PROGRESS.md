# Daily Learning Progress

This is the short resume tracker. Update it after every study session so the
next session begins from the learner's actual understanding rather than the
calendar.

The authoritative curriculum remains
`Systems_Engineering_Masterclass_Curriculum_Tracker.docx`.

## Current resume point

- **Stage:** 1 — Foundations of Computation
- **Topic:** Electricity, States, Bits, and Information
- **Current lesson:** Day 3 — Mastery Workshop
- **Topic status:** In progress; mastery not yet self-confirmed
- **Next action:** Read Day 3, run the terminal mastery challenge, reproduce
  the one-byte Linux experiment, and report weak areas/questions
- **Do not advance yet:** Day 4 begins the next topic only after the learner
  assesses this checkpoint

## Prepared next lessons

These lessons are available as a seven-day preview, but **prepared does not
mean completed**. The resume point remains Day 3 until learner evidence is
recorded. All seven lessons now include the historical problem being solved
and an explicit bridge toward arithmetic circuits, stored instructions, and
the CPU.

### Day 4 — How Can Bits Become a Negative Number?

- Lesson: `daily/lessons/day-004-how-bits-become-negative-numbers.md`
- PDF: `daily/pdf/day-004-how-bits-become-negative-numbers.pdf`
- Focus: signed magnitude, one's complement, two's complement, ranges, and
  the same-adder insight

### Day 5 — Bits as Switches

- Lesson: `daily/lessons/day-005-bitwise-tools-and-register-masks.md`
- PDF: `daily/pdf/day-005-bitwise-tools-and-register-masks.pdf`
- Challenge: `daily/challenges/day-005-register-mask-lab.c`
- Focus: AND, OR, XOR, NOT, shifts, masks, packed fields, and register
  read-modify-write reasoning

### Day 6 — When the Answer Does Not Fit

- Lesson: `daily/lessons/day-006-overflow-sign-extension-and-flags.md`
- PDF: `daily/pdf/day-006-overflow-sign-extension-and-flags.pdf`
- Challenge: `daily/challenges/day-006-overflow-lab.c`
- Focus: fixed-width arithmetic, unsigned wraparound, signed overflow, carry
  versus overflow, promotions, and sign extension

### Day 7 — Mastering Signed Bits

- Lesson: `daily/lessons/day-007-signed-bits-mastery-workshop.md`
- PDF: `daily/pdf/day-007-signed-bits-mastery-workshop.pdf`
- Focus: integrated interpretation, register decoding, arithmetic, extension,
  C experiments, and mastery explanation

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

### Day 1 — From Physical State to a Bit

- Lesson: `daily/lessons/day-001-physical-state-to-bit.md`
- PDF: `daily/pdf/day-001-physical-state-to-bit.pdf`
- Focus: physical state, continuous voltage, threshold ranges, noise margin,
  bits, and `2ⁿ` patterns
- Learner evidence reported: not yet recorded
- Resume note: Day 2 explicitly retrieves and extends these ideas

### Day 2 — The Bits Stay Still; the Meaning Changes

- Lesson: `daily/lessons/day-002-bits-patterns-meaning.md`
- PDF: `daily/pdf/day-002-bits-patterns-meaning.pdf`
- Challenge: `daily/challenges/day-002-bit-lab.html`
- Focus: pattern versus meaning, encoding, information, hexadecimal, and one
  pattern interpreted multiple ways on Linux
- Learner evidence reported: not yet recorded
- Resume note: complete Day 2 experiments before treating Day 3 as assessment

### Day 3 — Mastery Workshop

- Lesson: `daily/lessons/day-003-mastery-workshop.md`
- PDF: `daily/pdf/day-003-mastery-workshop.pdf`
- Challenge: `daily/challenges/day-003-mastery.py`
- Focus: retrieval, conversion, prediction, Linux observation, drawing, and
  mastery explanation
- Learner evidence reported: pending
- Exact next step: run:

  ```bash
  python3 ~/masterclass/daily/challenges/day-003-mastery.py
  ```

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
