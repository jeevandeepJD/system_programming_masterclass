# Weekly Learning Progress

This is the short resume tracker. Update it after every study session so the
next session begins from the learner's actual understanding rather than the
calendar.

The authoritative curriculum remains
`metadata/curriculum-tracker.docx`.

## Current resume point

- **Branch:** `master`
- **Format:** approved weekly mastery modules
- **Current module:** Week 1
- **Next action:** work through Week 1 at a comfortable pace, complete its
  observation/mastery evidence, and record the exact stopping point
- **Important:** prepared files do not assert learner mastery

## Week 1 package

- Overview: `01-foundations-to-cpu/overview.md`
- Consolidated PDF:
  `01-foundations-to-cpu/week-01-material.pdf`
- Daily lesson sources: `01-foundations-to-cpu/sections/`
- Labs: `01-foundations-to-cpu/challenges/`
- Suggested workload: approximately 21 focused hours, adjustable to mastery

### Daily roadmap inside Week 1

1. **State, Bits, and Meaning** — physical representation, voltage ranges,
   binary, encodings, information, and Linux byte interpretation.
2. **Signed Numbers and Bitwise Reasoning** — two's complement, masks,
   overflow, extension, C behavior, and ALU bridge.
3. **Boolean Logic and Physical Gates** — Boole, Shannon, switching
   technology, gates, truth tables, De Morgan, and combinational logic.
4. **Adders, Selection, and Memory** — full adders, carry propagation,
   mux/decoder/encoder, feedback, clocking, and registers.
5. **Building a Minimal CPU** — PC, IR, registers, ALU, datapath/control,
   tiny ISA, fetch/decode/execute, and cycle traces.
6. **ISA and Machine Code** — encodings, operands, addressing, RISC/CISC,
   x86-64/RISC-V examples, disassembly, and ISA design.
7. **Control Flow, Exceptions, and Privilege** — branches, loops, protection,
   interrupts/exceptions, vectoring, signals, and system-call transitions.

The seven sections are assembled into one PDF. Completion still requires the
theory checks, labs, and mastery evidence; reading the combined file is not
sufficient.

## Foundation-gate policy

- **Active:** Gate A — CPU, C, assembly, and toolchain clarity
- **Prepared but inactive:** Gate B — hardware/software boundary
- **Planned after Gate B:** Gate C — operating-system mental model
- **Deferred until gates:** specialization pivots, proof-of-fit sprints,
  active upstream issue selection, and ToyOS expansion

No gate is completed automatically from reading, quiz scores, or elapsed
time. Required evidence is defined in `metadata/COURSE_ROADMAP.md`.

## Prepared Week 2 package

- Consolidated PDF:
  `02-assembly-memory-and-c/week-02-material.pdf`
- Sources and labs: `02-assembly-memory-and-c/`
- Suggested workload: approximately 21 focused hours
- Daily path:
  1. x86-64 assembly foundations;
  2. stack frames and System V AMD64 calling conventions;
  3. RISC-V and ISA comparison;
  4. RAM, locality, and the memory wall;
  5. cache organization;
  6. alignment, endianness, and NUMA;
  7. C's execution model and data types.

## Prepared Week 3 package

- Consolidated PDF:
  `03-c-toolchain-and-startup/week-03-material.pdf`
- Sources and labs: `03-c-toolchain-and-startup/`
- Suggested workload: approximately 21 focused hours
- Daily path:
  1. pointers, arrays, and strings;
  2. structures, unions, and function pointers;
  3. dynamic memory and lifetime;
  4. qualifiers, atomics, and undefined behavior;
  5. source to object file;
  6. linking and libraries;
  7. ELF loading and `main`.

Weeks 2 and 3 are prepared ahead. They are not marked complete and do not
change the current resume point.

## Prepared Week 4 package

- Consolidated PDF: `04-cpu-and-chip-design/week-04-material.pdf`
- Sources and labs: `04-cpu-and-chip-design/`
- Scope: the minimum transistor/CMOS, timing, RTL, simulation, and synthesis
  depth needed to ground CPU behavior physically
- Important: this is a systems-programmer bridge, not an ASIC specialization

## Prepared Week 5 package

- Consolidated PDF:
  `05-riscv-cpu-microarchitecture/week-05-material.pdf`
- Sources and labs: `05-riscv-cpu-microarchitecture/`
- Scope: RV32I subset, datapath/control, runnable tiny core, pipeline,
  hazards, cache/TLB/MMU, and end-to-end C-to-transistor trace

Weeks 4 and 5 are prepared ahead. They do not change the current resume point
or imply completion.

## Prepared Week 6 package

- Consolidated PDF:
  [`06-toyos-bootstrap-and-exceptions/week-06-material.pdf`](../06-toyos-bootstrap-and-exceptions/week-06-material.pdf)
- Sources and labs:
  [`06-toyos-bootstrap-and-exceptions/`](../06-toyos-bootstrap-and-exceptions/)
- Continuous project:
  [`projects/toyos/`](../projects/toyos/)
- Scope: freestanding x86-64 C, ELF/linker contract, GRUB2 Multiboot2 boot,
  long-mode entry, serial/panic/assert diagnostics, IDT exception stubs,
  TSS/IST, page-fault decoding, and QEMU/GDB fault evidence
- Project profile: `CONFIG_MILESTONE=6`, kept runnable from the same source
  tree used by Week 7

## Prepared Week 7 package

- Consolidated PDF:
  [`07-memory-processes-and-scheduling/week-07-material.pdf`](../07-memory-processes-and-scheduling/week-07-material.pdf)
- Sources and labs:
  [`07-memory-processes-and-scheduling/`](../07-memory-processes-and-scheduling/)
- Continuous project:
  [`projects/toyos/`](../projects/toyos/)
- Scope: Multiboot2 memory-map parsing, bounded physical-frame allocation,
  4 KiB mapping, a small kernel heap, saved task contexts, cooperative
  switching, and PIT/PIC-driven preemption
- Project profile: `CONFIG_MILESTONE=7`, extending the Week 6 kernel rather
  than forking a second implementation

## Prepared Week 8 package

- Package entry:
  [`08-user-mode-syscalls-and-ipc/overview.md`](../08-user-mode-syscalls-and-ipc/overview.md)
- Sources and labs:
  [`08-user-mode-syscalls-and-ipc/`](../08-user-mode-syscalls-and-ipc/)
- Continuous project:
  [`projects/toyos/`](../projects/toyos/)
- Scope: one ring-3 process, a process-owned address space, embedded ELF
  loading, bounded user copying, a small syscall/descriptor model, and a
  bounded pipe
- Project profile: `CONFIG_MILESTONE=8`, extending the same Week 6–7 kernel
- Boundary: syscall entry deliberately uses a DPL3 `int 0x80` interrupt gate,
  not `SYSCALL`/`SYSRET`; there is no fork/exec, blocking IPC, shared-memory
  IPC, or implemented signal delivery

## Prepared Week 9 package

- Package entry:
  [`09-filesystems-and-virtual-devices/overview.md`](../09-filesystems-and-virtual-devices/overview.md)
- Sources and labs:
  [`09-filesystems-and-virtual-devices/`](../09-filesystems-and-virtual-devices/)
- Continuous project:
  [`projects/toyos/`](../projects/toyos/)
- Scope: a tiny ramfs/VFS with inode, open-file offset, descriptor, and
  ring-3 `open`/`read`/`write` paths; plus PCI bus-0 enumeration, QEMU EDU
  BAR0 mapping, and checked MMIO liveness
- Project profile: `CONFIG_MILESTONE=9`, extending the same continuous kernel
- Boundary: there is no actual block layer, page cache, journaling, device
  IRQ operation, or DMA

Weeks 6–9 are prepared ahead. They do not change the current Week 1 resume
point, complete any foundation gate, or imply learner mastery. Week 7 contains
ring-0 **kernel tasks**, while Week 8 introduces only one deliberately bounded
user process. Week 9 demonstrates ramfs/VFS and PCI/MMIO boundaries without
claiming a production storage or device stack.

## Approved follow-on track

After Weeks 4–5, begin a guided x86-64 ToyOS under QEMU using C and minimal
assembly. The purpose is reconstruction and understanding, not producing a
novel or production operating system. Keep the CPU/chip depth connected to
systems mechanisms and avoid turning the course into an HDL/ASIC specialty.

## Pace check

Weekly modules intentionally cover a broad scope. Evaluate retention,
fatigue, and completed evidence—not only how quickly the PDF reaches advanced
topics. Slow down or split a work session whenever understanding becomes
shallow.

## Current mastery target

Explain, without notes:

> Starting from a physical voltage state, explain how a computer represents
> bits, calculates `3 + 5`, stores the result, fetches and decodes an
> instruction requesting that operation, changes control flow, and safely
> enters an operating-system handler.

Required evidence:

- [ ] Explain why voltage ranges and noise margins matter
- [ ] Derive signed representation and bitwise/register-field operations
- [ ] Build/trace logic gates, a full adder, ripple carry, and a register
- [ ] Trace `3 + 5` from inputs through stored result
- [ ] Run the Tiny-8 CPU and explain adjacent state transitions
- [ ] Encode/decode fictional instructions and inspect a real ELF
- [ ] Trace a branch/loop and draw exception/system-call control flow
- [ ] Run the safe exception-flow lab and distinguish exception from signal
- [ ] Attempt each section's interactive theory check and record weak concepts
- [ ] Explain the mastery checkpoint aloud without notes
- [ ] Record confidence, time spent, status, and reflection here

## Weekly log

### Accelerated Week 1 evaluation

- Lesson sections completed:
- Labs completed:
- Weekly mastery explanation attempted:
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
### Week N — work-session checkpoint

- Daily section and subsection reached:
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
