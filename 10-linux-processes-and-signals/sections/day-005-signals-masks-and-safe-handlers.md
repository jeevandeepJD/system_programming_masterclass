# Day 5 — Signals, Masks, and Safe Handlers

**Target time:** approximately 3 hours

> The program is in the middle of an ordinary function. Something else —
> a terminal key, another process, or the kernel itself — needs this
> process to notice an event before that function returns. A signal is
> how the kernel names that event, holds it until the thread can take it,
> and enters a user function without the program having called it.

Day 4's exit word is a fact about a task that has already stopped running
user code. A signal is a fact about a task that is still running. The
handler that runs is ordinary user code: it uses the process's address
space, the C calling convention, and the same instruction set as `main`.
What is unusual is the entry. The kernel builds a frame so that a later
return resumes the interrupted context. That shape is the one Week 1 gave
exceptions, and the one ToyOS implements with an interrupt stack and
`iretq`. The signal frame and the `rt_sigreturn` path are architectural.
The pending set, the blocked set, and the choice of disposition are kernel
policy shared across x86-64, AArch64, and RISC-V.

## 1. Syntax needed for this source

A signal number becomes one bit. `1 << (sig - 1)` builds that bit,
`|=` posts it, and `& ~blocked` keeps the bits the task has not masked.
`(*p)->signal` follows a `task_struct **` from the task array. Delivery
is a call, `do_signal`, whose arguments are the registers the syscall
frame already saved (`eax` through `eip`, `esp`, and the segment
registers). The handler runs because those saved slots are rewritten.

Sketch:

```c
unsigned long bit = 1UL << (sig - 1);
p->signal |= bit;
if (p->signal & ~p->blocked)
        deliver(p);
```

## 2. Linux 0.12 first: a signal bitmask and `do_signal`

A signal has to sit somewhere until the task can take it. Linux 0.12
uses one `long` on the task, `signal`, and a second `long`, `blocked`.
`send_sig` posts a bit. The scheduler wakes an interruptible sleeper
when any unblocked bit is set. `do_signal` in `kernel/signal.c` then
builds the user return. Printed pages are in the 0.12 commentary; the
[0.12 map](../../references/EARLY_LINUX_012_MAP.md) is the index. The
signal chapter is 8.7, and the post is in `kernel/exit.c`.

`kernel/exit.c`, `send_sig`, printed page 396:

```c
if ((sig >= SIGSTOP) && (sig <= SIGTTOU))
        p->signal &= ~(1<<(SIGCONT-1));
/* Actually deliver the signal */
p->signal |= (1<<(sig-1));
return 0;
```

`kernel/sched.c`, printed page 357. `_BLOCKABLE` still lets `SIGKILL`
and `SIGSTOP` through:

```c
if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
(*p)->state==TASK_INTERRUPTIBLE)
        (*p)->state=TASK_RUNNING;
```

Trace: the bit is set in `p->signal`. On the way back to user mode,
`do_signal` (printed page 386) loads `current->sigaction[signr - 1]`
and, for a caught signal, arranges the saved `eip` so the return enters
the handler.

What carries forward is a pending set, a blocked set, and a user
function entered without the program having called it. One global
`long` and a direct edit of the 80386 frame are the 0.12 mechanism.

ToyOS already leaves a fault by building a frame and returning with
`iretq`
([Week 6](../../06-toyos-bootstrap-and-exceptions/sections/day-006-tss-ist-page-fault-and-gdb.md)).
0.12 aims that return at a user handler address.

## 3. Evolution: why the early design changed

Linux 0.12 stores pending signals in one machine word. On the way back
to user mode, `do_signal` rewrites the 80386 frame so the next return
enters the handler. That is enough for one thread and a small standard
set.

The set outgrew one word. `sigset_t` is a wider bitmap. Real-time
signals queue a `siginfo` for each posting, while standard signals still
coalesce into one bit. Threads split the pending state: a private mask
and pending set per task, and one shared pending set plus one
disposition table for the thread group. Delivery is still a return to
user mode, but each architecture builds its own `rt_sigframe` and
returns through `rt_sigreturn`. `signalfd` lets a thread read the same
pending events as a file descriptor.

What survived is the asynchronous contract. The kernel records an event
the program did not call, holds it while the mask blocks it, and enters
a user function only at a safe return. The 6.12/current path below is
`__send_signal_locked`, `get_signal`, and the per-ISA frame.

The excerpts below come from Linux 6.12, tag `v6.12`, commit
`adc218676eef`. Record `uname -r` next to that id. Names and line
numbers move. The application contract is `sigaction(2)` and the
async-signal-safe list, and these functions are one kernel's
implementation of it. Develop against this 6.12/current path.

---

## 4. Why the mechanism exists

A process spends almost all of its time in user code or in a syscall the
user code asked for. The scheduler, a terminal driver, a parent calling
`kill`, and a fault in this process itself can all need attention at a
moment the C abstract machine thinks of as "between two sequence points
inside a function the programmer wrote."

The hardware already has a way to leave the current instruction stream:
the exception and interrupt path from
[Week 1](../../01-foundations-to-cpu/sections/day-007-control-flow-exceptions-and-privilege.md).
That path enters the kernel. A signal is the process-visible continuation
of the same idea. The kernel records an event against the task, and at a
safe moment — return to user mode, or a syscall that is willing to be
interrupted — it may transfer control to a user function the process
installed.

Three stored facts make that possible:

```text
disposition     what should happen for this signal number
                (ignore, default action, or a user handler)
                process-wide, in sighand->action[]

blocked mask    which numbers this thread must hold pending
                per thread, in task->blocked

pending set     which numbers have been posted and not yet
                taken: thread-directed in task->pending,
                process-directed in signal->shared_pending
```

A blocked signal stays pending. The mask postpones delivery. It leaves
the event recorded. Generation, the pending bit, selection of a thread,
and delivery are separate steps. A handler running is the delivery step
for the "catch" disposition. Default action for a fatal signal is the
path into Day 4's `do_group_exit`. Default action for a stop signal is
the job-control stop Day 6 depends on.

Traditional signals below `SIGRTMIN` coalesce. `legacy_queue` is true
when the number is already in the pending bitmap, and `__send_signal_locked`
then leaves the existing entry in place. One pending `SIGUSR1` stands
for "at least one `SIGUSR1`." A `sig_atomic_t` flag written by the
handler means the same thing: the event happened. Real-time signals,
numbered from `SIGRTMIN`, have queueing rules and can carry a `siginfo`
payload. Today's lab uses `SIGUSR1` and `SIGUSR2`, which are in the
coalescing range.

`SIGKILL` and `SIGSTOP` keep a kernel disposition. The lab's check is
that `sigaction` of a `SIGKILL` handler fails with `EINVAL`.

---

## 5. Posting the signal: `__send_signal_locked`

`kernel/signal.c`, `__send_signal_locked`, tree `adc218676eef`, lines 1073–1095:

```c
static int __send_signal_locked(int sig, struct kernel_siginfo *info,
				struct task_struct *t, enum pid_type type, bool force)
{
	struct sigpending *pending;
	struct sigqueue *q;
	int override_rlimit;
	int ret = 0, result;

	lockdep_assert_held(&t->sighand->siglock);

	result = TRACE_SIGNAL_IGNORED;
	if (!prepare_signal(sig, t, force))
		goto ret;

	pending = (type != PIDTYPE_PID) ? &t->signal->shared_pending : &t->pending;
	/*
	 * Short-circuit ignored signals and support queuing
	 * exactly one non-rt signal, so that we can get more
	 * detailed information about the cause of the signal.
	 */
	result = TRACE_SIGNAL_ALREADY_PENDING;
	if (legacy_queue(pending, sig))
		goto ret;
```

The caller holds `sighand->siglock`. `prepare_signal` applies the
disposition filters (a signal the process ignores is dropped here, with
the special cases `prepare_signal` contains for `SIGCONT` and stop
signals). The `type` argument chooses the queue. `PIDTYPE_PID` posts to
the target thread's private pending set. Any wider type posts to the
thread group's shared pending set, which is how a signal aimed at the
process can be taken by whichever thread is eligible.

`legacy_queue` (lines 1068–1071) is `(sig < SIGRTMIN) && sigismember(...)`.
A second non-realtime signal, while one is already pending, returns
before a new queue entry is allocated. That is the coalesce.

When the signal is new, the function allocates a `sigqueue` for the
`siginfo` details, or skips that allocation for `SIGKILL` and for kernel
threads. It then sets the bitmap bit and wakes a thread. Lines 1166–1188:

```c
out_set:
	signalfd_notify(t, sig);
	sigaddset(&pending->signal, sig);

	/* Let multiprocess signals appear after on-going forks */
	if (type > PIDTYPE_TGID) {
		struct multiprocess_signals *delayed;
		hlist_for_each_entry(delayed, &t->signal->multiprocess, node) {
			sigset_t *signal = &delayed->signal;
			/* Can't queue both a stop and a continue signal */
			if (sig == SIGCONT)
				sigdelsetmask(signal, SIG_KERNEL_STOP_MASK);
			else if (sig_kernel_stop(sig))
				sigdelset(signal, SIGCONT);
			sigaddset(signal, sig);
		}
	}

	complete_signal(sig, t, type);
ret:
	trace_signal_generate(sig, info, t, type != PIDTYPE_PID, result);
	return ret;
}
```

`sigaddset` is the pending bit a later `sigpending` can observe.
`complete_signal` chooses a thread to wake. The block under
`type > PIDTYPE_TGID` records the signal for forks that are in progress
so a signal aimed at a process group is visible to children created
during the send. For the lab's `raise` of one thread, `type` is
`PIDTYPE_TGID` or `PIDTYPE_PID`, and the private or shared pending bit
is the observation.

---

## 6. Choosing a thread: `complete_signal`

`kernel/signal.c`, `complete_signal`, tree `adc218676eef`, lines 994–1027:

```c
static void complete_signal(int sig, struct task_struct *p, enum pid_type type)
{
	struct signal_struct *signal = p->signal;
	struct task_struct *t;

	/*
	 * Now find a thread we can wake up to take the signal off the queue.
	 *
	 * Try the suggested task first (may or may not be the main thread).
	 */
	if (wants_signal(sig, p))
		t = p;
	else if ((type == PIDTYPE_PID) || thread_group_empty(p))
		/*
		 * There is just one thread and it does not need to be woken.
		 * It will dequeue unblocked signals before it runs again.
		 */
		return;
	else {
		/*
		 * Otherwise try to find a suitable thread.
		 */
		t = signal->curr_target;
		while (!wants_signal(sig, t)) {
			t = next_thread(t);
			if (t == signal->curr_target)
				/*
				 * No thread needs to be woken.
				 * Any eligible threads will see
				 * the signal in the queue soon.
				 */
				return;
		}
		signal->curr_target = t;
```

`wants_signal` (lines 977–992) rejects a thread that has the signal
blocked, a thread with `PF_EXITING`, and — except for `SIGKILL` — a
thread that is stopped or traced or already has a pending signal and is
not current. The lab is single-threaded, so the group is empty and the
early return applies when the only thread is blocked: the bit stays
pending, and the thread will dequeue it when it next returns toward user
mode with the mask clear. That is the pending-mode experiment. `raise`
posts `SIGUSR1` while the mask blocks it. `sigpending` shows membership.
The handler's flag is still clear, because delivery has not run.

When a thread is chosen, the rest of `complete_signal` (lines 1030–1065)
either starts a fatal group exit, for a signal whose default action kills
the group, or calls `signal_wake_up` so the chosen thread notices
`TIF_SIGPENDING` and leaves its sleep. `SIGKILL` wakes even a thread in
an uninterruptible section of the wake path. The comment at lines 1060–1063
states the ordinary case: the signal is already queued, and the wake
tells the thread to dequeue it.

---

## 7. Delivery: `get_signal` on the way back to user mode

Waking a thread runs the kernel. Entering the handler happens when the
architecture's return-to-user path calls `get_signal`. In this tree those
callers are:

```text
arch/x86/kernel/signal.c      arch_do_signal_or_restart   line 333
arch/arm64/kernel/signal.c    do_signal                    line 1395
arch/riscv/kernel/signal.c    arch_do_signal_or_restart   line 402
```

Each calls `get_signal(&ksig)` — x86-64 at line 337, AArch64 at line
1434, RISC-V at line 437 — and, when it returns true, `handle_signal`.
The shared function starts at `kernel/signal.c` line 2683. It runs task
work, notifies parents of continued or stopped children, and honors
`SIGNAL_GROUP_EXIT` by forcing `SIGKILL`. The dequeue that matters for
a caught signal is here, lines 2789–2824:

```c
		/*
		 * Signals generated by the execution of an instruction
		 * need to be delivered before any other pending signals
		 * so that the instruction pointer in the signal stack
		 * frame points to the faulting instruction.
		 */
		type = PIDTYPE_PID;
		signr = dequeue_synchronous_signal(&ksig->info);
		if (!signr)
			signr = dequeue_signal(&current->blocked, &ksig->info, &type);

		if (!signr)
			break; /* will return 0 */

		if (unlikely(current->ptrace) && (signr != SIGKILL) &&
		    !(sighand->action[signr -1].sa.sa_flags & SA_IMMUTABLE)) {
			signr = ptrace_signal(signr, &ksig->info, type);
			if (!signr)
				continue;
		}

		ka = &sighand->action[signr-1];

		/* Trace actually delivered signals. */
		trace_signal_deliver(signr, &ksig->info, ka);

		if (ka->sa.sa_handler == SIG_IGN) /* Do nothing.  */
			continue;
		if (ka->sa.sa_handler != SIG_DFL) {
			/* Run the handler.  */
			ksig->ka = *ka;

			if (ka->sa.sa_flags & SA_ONESHOT)
				ka->sa.sa_handler = SIG_DFL;

			break; /* will return non-zero "signr" value */
```

`dequeue_signal` skips members of `current->blocked`. A pending, blocked
`SIGUSR1` stays in the bitmap through this call, `get_signal` returns
false, and the architecture path returns to the interrupted user context
with no handler. After `sigprocmask` clears the block, the next
return-to-user observes the bit, dequeues it, finds a handler other than
`SIG_DFL` and `SIG_IGN`, and returns true. The architecture code then
builds the frame.

Synchronous signals — faults produced by an instruction — are dequeued
first so the saved instruction pointer refers to the faulting
instruction. `SIGUSR1` from `raise` is asynchronous relative to the
surrounding C, and it takes the ordinary `dequeue_signal` path.

`ptrace_signal` is the hook Day 7's debugger uses: a traced task can
stop here and let the tracer rewrite or swallow the number, unless the
disposition has `SA_IMMUTABLE`. The lab runs untraced, so the installed
`sigaction` is the disposition that wins.

After this excerpt, a default-action signal falls through to ignore,
stop, or `do_group_exit` (the fatal path at the `fatal:` label, line
2881, which calls `do_group_exit` at line 2918). A caught signal breaks
out of the loop. `get_signal` then stores `ksig->sig` (line 2923) and
returns `signr > 0` (line 2928). `signal_delivered`, whose comment
begins at line 2931, blocks the delivered signal and the
extra set in `sa_mask` unless `SA_NODEFER` is set. Nested delivery of
the same number waits until the handler returns. That is a kernel mask
update, and it is why a handler that re-raises its own signal usually
sees the second instance after it returns.

---

## 8. Sleeping until a signal is pending: `pause`

A thread that has nothing to do until a signal arrives has to sleep in
the kernel, in a state the signal wake can leave. This tree still
provides the old call.

`kernel/signal.c`, tree `adc218676eef`, lines 4684–4708:

```c
#ifdef __ARCH_WANT_SYS_PAUSE

SYSCALL_DEFINE0(pause)
{
	while (!signal_pending(current)) {
		__set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	return -ERESTARTNOHAND;
}

#endif

static int sigsuspend(sigset_t *set)
{
	current->saved_sigmask = current->blocked;
	set_current_blocked(set);

	while (!signal_pending(current)) {
		__set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	set_restore_sigmask();
	return -ERESTARTNOHAND;
}
```

`pause` sleeps in `TASK_INTERRUPTIBLE` until `signal_pending` is true,
then returns `-ERESTARTNOHAND`. The C library reports that as `EINTR`.
The architecture path runs the handler on the way out when the signal
is caught. `sigsuspend` stores the old mask, installs the caller's set
for the sleep, and `set_restore_sigmask` puts the old mask back. The
lab's pending experiment orders the steps with the mask itself: block,
post, observe `sigpending`, then unblock.

---

## 9. The handler is ordinary user code, entered asynchronously

Once the frame is in place, exception return enters the handler with
the user stack pointer aimed at that frame. The handler is a C function
compiled with the platform calling convention. Section 10 names the
register that carries the signal number. The compiler's abstract machine
assumes one control path through a function except where the program
names a call. A handler is a second entry the compiler did not see at
the interrupted point.

If the interrupted function holds a libc lock, a half-updated `malloc`
chunk, or `printf`'s buffer, a handler that calls the same function
uses objects mid-update. POSIX names the async-signal-safe functions:
the ones specified to be callable from a handler because they leave
that libc state alone. `printf`, `malloc`, and most of stdio are
outside the list. `signal-safety(7)` on the system you run is the list
to read.

The lab's "handler ran" flag is a `volatile sig_atomic_t`. That type is
the C type specified for an asynchronous store of one value that a later
ordinary read can observe. `volatile` makes the store a visible side
effect. Week 11's atomics are the tool for cross-thread memory; this
flag is the tool for one thread and a handler.

The self-pipe moves the event onto a normal control path. The handler
writes one byte to a nonblocking pipe — `write` of that size is
async-signal-safe — after saving `errno` and before restoring it, so a
failed write leaves the interrupted function's error intact. The byte
sits in the kernel pipe buffer. A later `read` on the ordinary path,
where `printf` and allocation are in their usual relationship, consumes
it. A full pipe returns `EAGAIN` from that write and the handler still
returns; the lab sends one `SIGUSR2` and expects one byte. Close the
pipe after delivery can no longer observe the descriptors. The
single-threaded lab raises, finishes the one delivery, then closes.

---

## 10. Architecture lens: the frame and `rt_sigreturn`

This is the week's architectural section for signals. Process groups on
Day 6 will not need one. Here the kernel has to materialize a user-mode
call, and the calling convention, the stack alignment, the syscall
instruction that returns from the handler, and the amount to rewind a
restarted syscall are properties of the ISA.

### x86-64

`struct rt_sigframe` in `arch/x86/include/asm/sigframe.h` (lines 59–64)
holds `pretcode`, a `ucontext uc`, and a `siginfo info`, with floating
point state after the struct. `x64_setup_rt_frame` in
`arch/x86/kernel/signal_64.c` writes that frame. Tree `adc218676eef`,
lines 171–191:

```c
	/* x86-64 should always use SA_RESTORER. */
	if (!(ksig->ka.sa.sa_flags & SA_RESTORER))
		return -EFAULT;

	frame = get_sigframe(ksig, regs, sizeof(struct rt_sigframe), &fp);
	uc_flags = frame_uc_flags(regs);

	if (!user_access_begin(frame, sizeof(*frame)))
		return -EFAULT;

	/* Create the ucontext.  */
	unsafe_put_user(uc_flags, &frame->uc.uc_flags, Efault);
	unsafe_put_user(0, &frame->uc.uc_link, Efault);
	unsafe_save_altstack(&frame->uc.uc_stack, regs->sp, Efault);

	/* Set up to return from userspace.  If provided, use a stub
	   already in userspace.  */
	unsafe_put_user(ksig->ka.sa.sa_restorer, &frame->pretcode, Efault);
	unsafe_put_sigcontext(&frame->uc.uc_mcontext, fp, regs, set, Efault);
	unsafe_put_sigmask(set, frame, Efault);
	user_access_end();
```

`get_sigframe` chooses the user stack, including an alternate signal
stack when the disposition asked for one. The saved `sigcontext` is a
copy of the interrupted user registers. `pretcode` is the restorer the
handler will return to. On x86-64 the kernel requires `SA_RESTORER`:
the C library supplies a small user stub, usually in the vDSO, whose
job is to invoke `rt_sigreturn`.

`setup_signal_shadow_stack` runs before the live registers change.
Lines 201–212 then aim `pt_regs` at the handler:

```c
	/* Set up registers for signal handler */
	regs->di = ksig->sig;
	/* In case the signal handler was declared without prototypes */
	regs->ax = 0;

	/* This also works for non SA_SIGINFO handlers because they expect the
	   next argument after the signal number on the stack. */
	regs->si = (unsigned long)&frame->info;
	regs->dx = (unsigned long)&frame->uc;
	regs->ip = (unsigned long) ksig->ka.sa.sa_handler;

	regs->sp = (unsigned long)frame;
```

`ip` becomes the handler. `sp` becomes the frame, so the `ret` at the
end of the handler pops `pretcode` and jumps to the restorer. `di`,
`si`, and `dx` are the System V argument registers: signal number,
`siginfo` pointer, `ucontext` pointer. When user mode resumes, the CPU
fetches the handler as if the program had called it. The call is a
kernel edit of the register file.

`SYSCALL_DEFINE0(rt_sigreturn)` in the same file, lines 246–276, undoes
that edit. `restore_altstack` runs before `restore_sigcontext`, then
`restore_signal_shadow_stack`:

```c
SYSCALL_DEFINE0(rt_sigreturn)
{
	struct pt_regs *regs = current_pt_regs();
	struct rt_sigframe __user *frame;
	sigset_t set;
	unsigned long uc_flags;

	frame = (struct rt_sigframe __user *)(regs->sp - sizeof(long));
	if (!access_ok(frame, sizeof(*frame)))
		goto badframe;
	if (__get_user(*(__u64 *)&set, (__u64 __user *)&frame->uc.uc_sigmask))
		goto badframe;
	if (__get_user(uc_flags, &frame->uc.uc_flags))
		goto badframe;

	set_current_blocked(&set);

	if (restore_altstack(&frame->uc.uc_stack))
		goto badframe;

	if (!restore_sigcontext(regs, &frame->uc.uc_mcontext, uc_flags))
		goto badframe;

	if (restore_signal_shadow_stack())
		goto badframe;

	return regs->ax;

badframe:
	signal_fault(regs, frame, "rt_sigreturn");
	return 0;
```

`regs->sp - sizeof(long)` recovers the frame after the restorer's
syscall entry has pushed a return slot. `restore_sigcontext` copies the
saved user registers back into `pt_regs`. The mask saved at delivery is
restored. The next return to user mode resumes the interrupted
instruction stream. A bad frame takes `signal_fault` (line 275) and the
process receives `SIGSEGV`. Forging a frame from user C is a way to
corrupt that restore. The lab proves the handler ran. It leaves the
frame bytes on the stack for the kernel and the C library.

`arch_do_signal_or_restart` in `arch/x86/kernel/signal.c` (line 333)
rewinds a restartable syscall by `regs->ip -= 2` (lines 351 and 356),
because the x86-64 `syscall` instruction is two bytes. That constant is
an ISA fact. It is why this
lesson's frame discussion is architectural and Day 6's process groups
are not.

### AArch64 and RISC-V

Both architectures build a different frame and return through their own
`rt_sigreturn`.

AArch64's `setup_return` in `arch/arm64/kernel/signal.c`, lines
1252–1306, sets the handler's entry registers and the link register:

```c
static void setup_return(struct pt_regs *regs, struct k_sigaction *ka,
			 struct rt_sigframe_user_layout *user, int usig)
{
	__sigrestore_t sigtramp;

	regs->regs[0] = usig;
	regs->sp = (unsigned long)user->sigframe;
	regs->regs[29] = (unsigned long)&user->next_frame->fp;
	regs->pc = (unsigned long)ka->sa.sa_handler;

	/*
	 * Signal delivery is a (wacky) indirect function call in
	 * userspace, so simulate the same setting of BTYPE as a BLR
	 * <register containing the signal handler entry point>.
	 * Signal delivery to a location in a PROT_BTI guarded page
	 * that is not a function entry point will now trigger a
	 * SIGILL in userspace.
	 *
	 * If the signal handler entry point is not in a PROT_BTI
	 * guarded page, this is harmless.
	 */
	if (system_supports_bti()) {
		regs->pstate &= ~PSR_BTYPE_MASK;
		regs->pstate |= PSR_BTYPE_C;
	}

	/* TCO (Tag Check Override) always cleared for signal handlers */
	regs->pstate &= ~PSR_TCO_BIT;

	/* Signal handlers are invoked with ZA and streaming mode disabled */
	if (system_supports_sme()) {
		/*
		 * If we were in streaming mode the saved register
		 * state was SVE but we will exit SM and use the
		 * FPSIMD register state - flush the saved FPSIMD
		 * register state in case it gets loaded.
		 */
		if (current->thread.svcr & SVCR_SM_MASK) {
			memset(&current->thread.uw.fpsimd_state, 0,
			       sizeof(current->thread.uw.fpsimd_state));
			current->thread.fp_type = FP_STATE_FPSIMD;
		}

		current->thread.svcr &= ~(SVCR_ZA_MASK |
					  SVCR_SM_MASK);
		sme_smstop();
	}

	if (ka->sa.sa_flags & SA_RESTORER)
		sigtramp = ka->sa.sa_restorer;
	else
		sigtramp = VDSO_SYMBOL(current->mm->context.vdso, sigtramp);

	regs->regs[30] = (unsigned long)sigtramp;
}
```

`pc` is the handler, `sp` is the frame, `x0` is the signal number, and
`x30` is the restorer, so a normal function return branches to
`sigreturn` machinery. When the CPU supports SME, the same function
disables ZA and streaming mode before it writes that restorer.
`SYSCALL_DEFINE0(rt_sigreturn)` is at line 964 of the same file. Branch
Target Identification and tag-check override are architectural state
this kernel writes because a handler entry has to look like a real
indirect call on that CPU. x86-64 has no `PSTATE.BTYPE`
field to set. The pending-bit logic in `kernel/signal.c` is unchanged.

RISC-V's `setup_rt_frame` in `arch/riscv/kernel/signal.c`, lines 344–375:

```c
	/* Set up to return from userspace. */
#ifdef CONFIG_MMU
	regs->ra = (unsigned long)VDSO_SYMBOL(
		current->mm->context.vdso, rt_sigreturn);
#else
	/*
	 * For the nommu case we don't have a VDSO.  Instead we push two
	 * instructions to call the rt_sigreturn syscall onto the user stack.
	 */
	if (copy_to_user(&frame->sigreturn_code, __user_rt_sigreturn,
			 sizeof(frame->sigreturn_code)))
		return -EFAULT;

	addr = (unsigned long)&frame->sigreturn_code;
	/* Make sure the two instructions are pushed to icache. */
	flush_icache_range(addr, addr + sizeof(frame->sigreturn_code));

	regs->ra = addr;
#endif /* CONFIG_MMU */

	/*
	 * Set up registers for signal handler.
	 * Registers that we don't modify keep the value they had from
	 * user-space at the time we took the signal.
	 * We always pass siginfo and mcontext, regardless of SA_SIGINFO,
	 * since some things rely on this (e.g. glibc's debug/segfault.c).
	 */
	regs->epc = (unsigned long)ksig->ka.sa.sa_handler;
	regs->sp = (unsigned long)frame;
	regs->a0 = ksig->sig;                     /* a0: signal number */
	regs->a1 = (unsigned long)(&frame->info); /* a1: siginfo pointer */
	regs->a2 = (unsigned long)(&frame->uc);   /* a2: ucontext pointer */
```

`epc` is the PC that exception return will resume. `ra` is the return
address, aimed at the vDSO `rt_sigreturn` when the kernel has an MMU.
A no-MMU build copies a two-instruction stub and flushes the icache
before `ra` aims at it. `SYSCALL_DEFINE0(rt_sigreturn)` is at line 231
of the same file. `arch_do_signal_or_restart` (line 402) sets
`restart_addr` to `continue_addr - 4` and then stores it in `regs->epc`
(line 428). x86-64 subtracted 2 from `ip` in place.

```text
                x86-64              AArch64               RISC-V
handler PC      regs->ip            regs->pc              regs->epc
stack           regs->sp            regs->sp              regs->sp
signal number   regs->di            regs->regs[0]         regs->a0
return tramp    frame->pretcode     regs->regs[30]        regs->ra
sigreturn       rt_sigreturn        rt_sigreturn          rt_sigreturn
                (line 246)          (line 964)            (line 231)
syscall rewind  ip -= 2             pc = restart_addr     epc = restart_addr
```

The shared idea is one sentence: the kernel writes a user frame and a
user PC so that exception return enters the handler, and `rt_sigreturn`
later copies the saved context back. The registers, the trampoline, and
the rewind distance are per ISA.

### ToyOS

[Week 6](../../06-toyos-bootstrap-and-exceptions/sections/day-006-tss-ist-page-fault-and-gdb.md)
gives a fault a known stack through the IST and returns with `iretq`.
The IDT stub builds a frame so the handler can run and so `iretq` can
restore the interrupted context. Linux signal delivery is that pattern
with the handler in user mode:

```text
ToyOS fault                         Linux caught signal
───────────                         ───────────────────
CPU traps, IST may switch stack     CPU traps or syscall returns
kernel builds a frame               kernel builds a user rt_sigframe
handler runs on a kernel stack      handler runs in the user address space
iretq restores the saved context    rt_sigreturn restores sigcontext
                                    then the kernel returns to user mode
```

ToyOS records the fault with an explicit marker and returns through
`iretq` in one address space. Linux adds a `sigaction` table and a
per-thread blocked mask, and it returns through `rt_sigreturn`. Week 8's
`int 0x80` path is the shape of that return entry: user code crosses
with a call number, and the kernel checks the frame (`access_ok`,
`__get_user`) before trusting it. Which number is pending, and which
thread is eligible, stays in `kernel/signal.c`.

---

## 11. Execution trace: blocked `SIGUSR1`, then the self-pipe

This is the sequence inside
[`../challenges/day-005-signals-safe-handlers.sh`](../challenges/day-005-signals-safe-handlers.sh).
The wrapper runs the canonical modes and sends signals only to the
current process or a child it created.

```text
install sigaction for SIGUSR1
sigprocmask(SIG_BLOCK) adds SIGUSR1 to current->blocked
raise(SIGUSR1)
    __send_signal_locked
        pending bit set
        legacy_queue will collapse a second SIGUSR1
        complete_signal: the only thread blocks it, so no delivery yet
sigpending shows SIGUSR1
handler flag is still 0

sigprocmask unblocks SIGUSR1
return toward user mode
    arch_do_signal_or_restart / do_signal
        get_signal dequeues SIGUSR1
        disposition is the user handler
        x64_setup_rt_frame / setup_return / setup_rt_frame
            save registers into the user frame
            set PC to the handler, SP to the frame
    exception return
handler stores volatile sig_atomic_t
handler returns
    restorer issues rt_sigreturn
    restore_sigcontext copies the saved registers back
interrupted user code continues
flag is 1
```

The self-pipe mode is the same delivery with a different body:

```text
SIGUSR2 handler
    save errno
    write one byte to the nonblocking pipe
    restore errno
    return
ordinary read consumes the byte
ordinary code prints the result
```

The checked failures sit beside those traces. `sigaction` for `SIGKILL`
returns `EINVAL`. `kill` of a pid the lab chooses so that it names no
process returns `ESRCH`. Signal zero asks about permission and existence
and posts nothing. `SA_RESTART` is set on the lab disposition; the
architecture return path applies it per syscall return code. The lab's
evidence is the handler flag and the pipe byte.

Run the wrapper from the Week 10 directory. It builds through the
project Makefile, runs without root, and targets the current process or
a descendant. Predict that `SIGUSR1` is pending while blocked and that
the flag changes after the unblock, and that `SIGUSR2` produces one
pipe byte on the normal path.

---

## 12. Mastery: Explain · Draw · Observe · Build

The wrapper is
[`../challenges/day-005-signals-safe-handlers.sh`](../challenges/day-005-signals-safe-handlers.sh).
A passing run shows the pending bit and the pipe byte. The explanations
below are still yours to write. These notes leave mastery open.

### Explain

1. Where does a blocked signal live between `raise` and the unblock?
2. Why does `legacy_queue` make a second `SIGUSR1` invisible as a
   separate event?
3. Why is the handler ordinary user code, and why does that fact
   restrict it to async-signal-safe functions?
4. What does the self-pipe move, and which context is allowed to call
   `printf`?
5. What does x86-64 store in `pretcode`, and what does `rt_sigreturn`
   copy back into `pt_regs`?
6. Why do AArch64 and RISC-V have their own `rt_sigreturn` even though
   `get_signal` is shared?

### Draw

Draw the pending-bit trace from section 11, and a second picture of the
user stack at the moment `regs->ip` (or `pc`, or `epc`) first points at
the handler. Label the saved context, the trampoline, and the
interrupted instruction the trampoline will resume. Mark which boxes
are ISA-specific.

### Observe

Run the wrapper. Record the pending membership before the unblock, the
flag afterward, and the pipe byte. Record `uname -r`. If you open
`kernel/signal.c` or `arch/<isa>/kernel/signal.c`, confirm the tree id
`adc218676eef` or say which tree you actually have before quoting a
line.

### Build

In the canonical program, keep the handler's work on the safe list.
Write a short note that names one libc function the handler leaves to
the ordinary path, and why the pipe is the handoff. Keep signals aimed
at the lab process or its children.
