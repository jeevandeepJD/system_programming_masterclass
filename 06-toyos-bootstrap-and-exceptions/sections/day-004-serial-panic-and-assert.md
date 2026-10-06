# Day 4 — COM1 Serial, Panic, and Assert

**Target time:** approximately 2½–3 hours

> When the screen and filesystem do not exist, what evidence can the kernel
> emit before it deliberately stops?

ToyOS uses the first 16550-compatible serial port, conventionally COM1 at I/O
base `0x3f8`. QEMU connects that byte stream to the host process's standard
output. This is both an early diagnostic path and the observable channel for
regression tests.

---

## 1. Initializing the UART

`serial_init` performs programmed I/O with `outb`. It:

1. disables UART interrupts;
2. enables divisor-latch access;
3. writes divisor 1;
4. selects eight data bits, no parity, one stop bit;
5. configures and clears FIFOs;
6. sets modem-control outputs.

Those writes target a 16550-style interface implemented by QEMU in this test
environment. The x86 `in`/`out` instructions and I/O privilege behavior are
architecture/platform mechanisms. The selected baud divisor and polling
driver are implementation choices.

Exact physical serial timing is not observed when QEMU routes the emulated
device to a host stream. The useful evidence is ordered bytes from the
emulated guest.

## 2. Polling and liveness

`serial_putc` waits until line-status bit `0x20` says the transmit holding
register is empty, then writes one byte. Polling keeps the early path small:
no IRQ controller, receive path, queue, or scheduler is required.

The trade-off is liveness. If the modeled device never becomes ready, the
kernel spins forever. There is no timeout in this primitive. The outer test
harness supplies a 12-second host timeout so automation cannot hang
indefinitely.

`serial_write` inserts carriage return before newline. That is terminal
presentation policy, not a C or x86 requirement.

## 3. Formatting without libc

`serial_hex` emits exactly sixteen hexadecimal digits after `0x`, exposing
the full 64-bit value. `serial_dec` uses a fixed 21-byte local array and
constructs digits backward.

These routines avoid dynamic allocation and variadic formatting. Their
bounded behavior is easier to reason about while exception handling may
already be under stress. They are not general `printf` replacements.

## 4. Panic is policy

`panic_at` prints:

```text
PANIC: <message> at <file>:<line>
```

It then writes a code to QEMU's `isa-debug-exit` device. The CPU does not
define “panic.” It defines instructions, exceptions, and halt behavior;
ToyOS decides that an unrecoverable invariant violation should report and
terminate this test VM.

The Makefile configures the device at its default I/O port. QEMU reports a
host status derived from the guest value. The harness expects status 33 for
normal completion, 35 for expected architecture faults, and 37 for the
expected assertion profile. These numbers are test-protocol evidence, not
portable process exit semantics for kernels.

## 5. Assertion versus architectural exception

`ASSERT(false)` expands to a normal C branch and call to `panic_at`. It is
not an x86 exception. By contrast, division by zero and an unmapped write
cause processor exceptions that enter through the IDT.

This distinction matters in a report:

```text
assertion failure:
  C condition → ToyOS macro → panic_at → serial → debug-exit

page fault:
  memory instruction → CPU translation failure → IDT/stub/frame
  → C handler → serial → debug-exit
```

Both stop this milestone, but their entry mechanisms and evidence differ.

## 6. Regression harness as evidence

`scripts/check-qemu.sh` creates a temporary host log, launches QEMU under
`timeout`, captures status, prints the log, checks the expected status, and
searches for an exact serial marker. It removes the temporary file on exit.

The two-part check matters. A marker without the expected exit might precede
a later hang. An exit status without the marker might come from the wrong
path. Together they identify the tested success protocol more narrowly.

The harness still cannot prove every serial byte, ordering under concurrency,
or behavior on physical UART hardware.

## 7. Practical challenge

Use `challenges/day-004-serial-panic-assert.sh` from the repository root.

**Purpose:** compare normal completion with the deliberate assertion path.

**Predict:** list the serial lines common to both runs, the first line that
diverges, and each expected host status.

**Run:** the wrapper invokes the existing normal Week 6 check and expected
assert profile. It also prints the serial/panic source region so output can be
correlated with implementation.

**Evidence:** preserve `WEEK6: PASS`, `INJECT: assertion failure`,
`PANIC: assertion failed: false`, `EXPECTED-FAULT: assert PASS`, and both
log-check markers. The panic line's exact source line can change when the
canonical file changes; report the observed value rather than memorizing it.

## 8. Debugging discipline

Early logs should be:

- deterministic enough for exact checks;
- explicit about milestone and event;
- usable before allocation and interrupts;
- bounded where possible;
- honest about whether a result is pass, expected fault, or panic.

A log statement is evidence that execution reached the write. It is not proof
that every earlier subsystem is fully correct. Prefer several independent
checks over one celebratory banner.

---

## Mastery: Explain · Observe · Build

### Explain

- Why does polling reduce dependencies but risk hanging?
- Why is `ASSERT` not an architectural exception?
- Why does a kernel test need both a serial marker and host status?
- Why does emulated COM1 output not measure real UART timing?

### Observe

Create a two-column trace for normal and assertion profiles. Mark the last
shared event, first divergent event, termination mechanism, and host-visible
evidence.

### Build

Design, on paper, one additional bounded diagnostic such as printing `RSP`.
Specify where it is safe, what it would prove, and what it would not prove.
Do not edit the canonical project for this challenge.

## Optional references

Use current 16550-compatible UART documentation and QEMU's
`isa-debug-exit` documentation for exact device behavior. The current Intel
or AMD manuals define port-I/O instructions. Local kernel-debugging examples
may illustrate panic policy, but are not specifications.
