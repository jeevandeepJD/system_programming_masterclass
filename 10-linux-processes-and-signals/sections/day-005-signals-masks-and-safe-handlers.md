# Day 5 — Signals, Masks, Pending State, and Safe Return

**Target time:** approximately 3 hours

> A signal is asynchronous notification with selection, masking, disposition,
> delivery, and return rules. A handler is not an ordinary callback.

## 1. Run both canonical signal modes

Run `challenges/day-005-signals-safe-handlers.sh`. Predict that `SIGUSR1`
becomes pending while blocked, the handler has not run at that point, and
unblocking permits delivery. Predict that the `SIGUSR2` handler writes one
byte to a self-pipe and normal flow prints the result.

The wrapper also runs checked failures: installing a handler for `SIGKILL`
must fail with `EINVAL`, and signal-zero probing of an intentionally impossible
PID must fail with `ESRCH`. It sends no signal to unrelated processes.

## 2. Separate the signal stages

A process- or thread-directed signal is generated, may become pending, is
selected for an eligible thread, is delivered according to its disposition,
and may cause default action, ignoring, or handler execution. A blocked signal
is not “disabled.” It generally remains pending until unblocked, consumed by a
synchronous interface, or otherwise resolved.

Traditional signals generally coalesce: repeated generation while one
instance is pending need not produce matching handler counts. POSIX real-time
signals have queuing and ordering rules. Therefore a single `sig_atomic_t`
flag means “something happened,” not “exactly N events happened.”

Signal masks are per-thread. Dispositions are process-wide. Pending state can
be thread-directed or process-directed. In a multithreaded program, delivery
selection is consequently more complex than “the process runs its handler.”
Today's lab is single-threaded.

## 3. Install with `sigaction`

`sigaction` makes disposition, handler mask, and flags explicit. Initialize
the entire structure, initialize `sa_mask`, choose flags deliberately, and
check the return. The lab uses `SA_RESTART`, but restart behavior is
syscall-specific; never assume every interrupted operation restarts.

During a handler, the delivered signal is ordinarily blocked, along with
signals named in `sa_mask`, unless flags change that behavior. Nested handlers
can invalidate simple state assumptions. `SIGKILL` and `SIGSTOP` cannot be
caught, blocked, or ignored.

For race-free waiting, do not test a flag and then call `pause`: a signal can
arrive between those operations. Block first, test state, and use `sigsuspend`
to atomically replace the mask while sleeping, or use synchronous mechanisms
such as `sigwaitinfo` in a designed signal thread.

## 4. Async-signal-safety

A handler can interrupt code while libc or application locks and invariants
are mid-update. Calling `printf`, `malloc`, most stdio, or ordinary
lock-taking code can deadlock or corrupt state. POSIX defines a limited set of
async-signal-safe functions. Consult the platform documentation; familiarity
is not proof.

The simple handler assigns a `volatile sig_atomic_t`. That type supports the
narrow communication intended here, but `volatile` does not make arbitrary
shared state atomic or establish a general cross-thread memory model.

The self-pipe handler saves `errno`, attempts one `write` of one byte to a
nonblocking pipe, and restores `errno`. Normal flow performs `read`, validation,
stdio, and cleanup. Nonblocking mode prevents a full pipe from trapping the
handler. The sample intentionally ignores a failed handler write, meaning
notifications can be lost if the pipe is full. A production design must make
that coalescing/loss policy explicit.

## 5. Masks and pending evidence

The pending mode installs the disposition before blocking, constructs a set
with `sigemptyset` and `sigaddset`, then calls `sigprocmask(SIG_BLOCK, ...)`.
`raise(SIGUSR1)` generates the signal for the process. `sigpending` and
`sigismember` verify pending membership while `handled` remains zero.
Unblocking allows delivery, after which the checked flag is one.

This sequence is deterministic enough for the single-threaded self-signal.
It does not measure when an externally generated signal would be selected, or
how multiple eligible threads would be chosen. Output from before and after
unblock is the evidence; a generic statement that “signals are immediate” is
false.

## 6. Signal return and architecture lens

To invoke a userspace handler, the kernel arranges saved user context and a
signal frame according to the architecture ABI, changes user registers to
enter the handler, and supplies a return path that invokes signal-return
machinery. Linux validates/restores state on return. Exact frame layout,
register names, trampoline details, and syscall numbers vary across x86-64,
AArch64, and RISC-V.

This is one of the week's two justified architecture lenses. Inspect matching
Linux source under `arch/<isa>/kernel/signal.c` and entry code only for the
running kernel revision. Do not manually call `rt_sigreturn`, forge frames, or
hard-code layout in portable C.

The lab proves handler execution through state and pipe evidence. It does not
dump the signal frame or establish the precise instruction at which delivery
occurred.

## 7. Interrupted operations and lifetime

Syscalls may fail with `EINTR`, restart transparently, complete partially, or
have API-specific rules. Write retry loops only after understanding whether
progress occurred. The process lab advances buffers by successful byte counts;
the signal lab retries the normal-flow read only on `EINTR`.

Handler-visible objects must remain live for every possible delivery. Before
closing the self-pipe, block/remove the signal or otherwise prove no handler
can use the descriptors. The single-threaded sample raises synchronously and
completes one delivery before closing, then sets globals to `-1`; its narrow
control flow supplies that proof. Long-running programs need stronger
shutdown sequencing.

## 8. Source landmarks and comparison

For pinned Linux source inspect `kernel/signal.c`,
`include/linux/signal_types.h`, and matching architecture signal/entry files.
Application contracts come from `sigaction(2)`, `signal-safety(7)`, and POSIX;
internal function names are not ABI.

xv6-riscv's base tree has a far smaller user signal story (often only kill
state checked around trap return); extensions vary, so verify the commit.
ToyOS may have interrupt/trap machinery without POSIX process signals.
Compare implemented mechanisms, not shared vocabulary.

Linux supports queued real-time signals, thread-directed delivery, signalfd,
ptrace interaction, job-control signals, alternate stacks, restart blocks,
and architecture-specific frames. The self-pipe remains useful because it
converts minimal asynchronous notification into ordinary event-loop work.

## 9. Build the causal trace

For `pending`, label generation, blocked mask, pending observation, unblock,
selection, handler assignment, and return. For `self-pipe`, label installed
disposition, nonblocking endpoint, generated signal, safe write, restored
`errno`, normal-flow read, and close.

Then list non-claims: no delivery count, no cross-thread ordering, no full
pipe stress, no frame capture, no real-time queue, and no proof that
`SA_RESTART` restarts all calls.

## Practical challenge

**Purpose:** observe blocked pending state and move handler notification into
normal control flow without unsafe library work.

**Prediction:** `SIGUSR1` is pending before unblock and handled afterward;
`SIGUSR2` contributes one byte through the self-pipe.

**Evidence:** checked output, exact handler source, optional signal trace,
expected `EINVAL`/`ESRCH` failures, kernel release, and artifact hash.

**Limits:** single-threaded, one notification, no saturation, no real-time
signals, no alternate stack, and no captured architecture signal frame.
