# Day 7 — `/proc`, strace, GDB, and the Comparison

**Target time:** approximately 3 hours

> The kernel objects from this week are real, and user code cannot see
> them by reading its own variables. `/proc`, strace, and GDB are three
> ways to ask the kernel to describe a task. Each one stops at a
> different layer, and each one is evidence only after you have named
> the kernel, the binary, and the question.

Days 1–6 followed a task from identity through `fork`, `exec`, the exit
word, signal delivery, and process-group membership. Today those
mechanisms become a report. The report says which fact came from a
generated procfs file, which came from a ptrace stop, which came from
an ELF symbol, and which came from a checked assertion in the lab
program. A tool transcript with no version and no scope is a story
about a different machine.

## 1. Syntax needed for this source

`show_task` receives a task number and a `struct task_struct *`, then
prints members with `printk`, an ordinary call. `p->p_cptr ? p->p_cptr->pid
: -1` follows a child pointer only when it is non-NULL. `*(1019 +
(unsigned long *) p)` treats the task page as an array of words and
reads one saved slot near the high end of that page: the kernel stack
grows down beside the task struct. `show_state` is a loop over
`task[NR_TASKS]`.

Sketch:

```c
for (i = 0; i < NR_TASKS; i++)
        if (task[i])
                printk("pid=%d state=%d\n", task[i]->pid, task[i]->state);
```

## 2. Linux 0.12 first: print the task array, then Bochs and GDB

The early kernel has no `/proc`. To see every task you print the array
you already walked for fork, exit, and signals. `show_state` calls
`show_task` for each live slot. Chapter 17 then runs that same kernel
under Bochs and attaches GDB. Printed pages are in the 0.12 commentary;
the [0.12 map](../../references/EARLY_LINUX_012_MAP.md) is the source
index for the paths those tools stop inside.

`kernel/sched.c`, `show_task`, printed page 354:

```c
printk("%d: pid=%d, state=%d, father=%d, child=%d, ",nr,p->pid,
        p->state, p->p_pptr->pid, p->p_cptr ? p->p_cptr->pid : -1);
printk("    PC=%08X.", *(1019 + (unsigned long *) p));
```

`show_state`, printed pages 354–355, walks the whole table:

```c
printk("\rTask-info:\n\r");
for (i=0;i<NR_TASKS;i++)
        if (task[i])
                show_task(i,task[i]);
```

Trace: one loop, one `printk` per live `task[i]`, showing pid, state,
parent, child, and a PC taken from the task's kernel stack page.

Chapter 17.5 (printed page 1065) runs this kernel in Bochs.
Chapter 17.11 (printed page 1094) rebuilds it with `-g` and talks to
Bochs through the GDB stub, so a breakpoint is a C source line in the
kernel you just read.

What carries forward is the habit of naming the task and reading its
state from outside the program. What changes in 5.10 is the window:
`proc_pid_status` generates text from `task_struct` at read time, and
ptrace parks a live task in `TASK_TRACED` so strace and GDB can print
syscalls and registers. Develop against that procfs and ptrace path.
A `printk` of `task[64]` and a Bochs stub are the 0.12 observation tools.

ToyOS shows the one user image with a serial marker or a GDB remote
stop
([Week 6](../../06-toyos-bootstrap-and-exceptions/sections/day-006-tss-ist-page-fault-and-gdb.md)).
0.12 adds the loop over every task. The 5.10 files below are the
generated `/proc` text and the ptrace stop.

Now move to the 5.10 implementation.

Line-level claims below are from tree `c9b2385d6793`, a 5.10.134-based
kernel. Before you repeat a line number, record `uname -r` and the tree
id you actually opened. Names move. A function that is `proc_pid_status`
here may be split or renamed in a later release, and the string it
prints can gain fields. The man page for procfs is the application
contract. This file is one kernel's generator.

---

## 3. Why the view has to be generated

A task's exit state, real parent, blocked signals, and session id live
in kernel memory, in objects the C abstract machine of the process does
not name. Week 2's process can see its own globals, its stack, and the
bytes it loads and stores. It cannot see `exit_state`. The kernel
therefore builds a text file on demand when someone reads `/proc/PID/status`
or `/proc/PID/stat`.

That read is a syscall like any other. The vfs path from Week 9 resolves
the procfs inode, and the inode's operations call a function that
`seq_printf`s fields out of `task_struct`. Two reads are two
generations. A task can change state between them. The file is a
projection taken while the kernel held the locks that function takes,
and it is a projection of the reader's pid namespace and credentials.
It is fresh text, produced for that `read`.

`fs/proc/base.c` in this tree publishes the name. The task directory
contains an entry of the form `ONE("status", S_IRUGO, proc_pid_status)`
(the occurrences are at lines 3201 and 3545). Opening `status` and
reading it runs `proc_pid_status`.

---

## 4. The state letter is an index into a table

`fs/proc/array.c`, tree `c9b2385d6793`, lines 124–150:

```c
/*
 * The task state array is a strange "bitmap" of
 * reasons to sleep. Thus "running" is zero, and
 * you can test for combinations of others with
 * simple bit tests.
 */
static const char * const task_state_array[] = {

	/* states in TASK_REPORT: */
	"R (running)",		/* 0x00 */
	"S (sleeping)",		/* 0x01 */
	"D (disk sleep)",	/* 0x02 */
	"T (stopped)",		/* 0x04 */
	"t (tracing stop)",	/* 0x08 */
	"X (dead)",		/* 0x10 */
	"Z (zombie)",		/* 0x20 */
	"P (parked)",		/* 0x40 */

	/* states beyond TASK_REPORT: */
	"I (idle)",		/* 0x80 */
};

static inline const char *get_task_state(struct task_struct *tsk)
{
	BUILD_BUG_ON(1 + ilog2(TASK_REPORT_MAX) != ARRAY_SIZE(task_state_array));
	return task_state_array[task_state_index(tsk)];
}
```

The comment values match the flags in `include/linux/sched.h` in this
tree: `__TASK_TRACED` is `0x0008`, `EXIT_DEAD` is `0x0010`, `EXIT_ZOMBIE`
is `0x0020`. Day 4's assignment `tsk->exit_state = EXIT_ZOMBIE` is why a
later read can print `Z (zombie)`. A ptrace stop, below, is why the same
table can print `t (tracing stop)`. The letter in `/proc/PID/stat` is
the first character of that string. `do_task_stat` takes it with
`state = *get_task_state(task)` at line 460. The long form in `status`
prints the whole string.

`get_task_state` indexes the table. It does not run the program, sample
the user instruction pointer, or reconstruct a backtrace. The scheduler
and the exit path already stored a state. Procfs translates the stored
value into text.

---

## 5. `proc_pid_status` walks the task and prints

`fs/proc/array.c`, `proc_pid_status`, lines 408–432:

```c
int proc_pid_status(struct seq_file *m, struct pid_namespace *ns,
			struct pid *pid, struct task_struct *task)
{
	struct mm_struct *mm = get_task_mm(task);

	seq_puts(m, "Name:\t");
	proc_task_name(m, task, true);
	seq_putc(m, '\n');

	task_state(m, ns, pid, task);

	if (mm) {
		task_mem(m, mm);
		task_core_dumping(m, mm);
		task_thp_status(m, mm);
		mmput(mm);
	}
	task_sig(m, task);
	task_cap(m, task);
	task_seccomp(m, task);
	task_cpus_allowed(m, task);
	cpuset_task_status_allowed(m, task);
	task_context_switch_counts(m, task);
	return 0;
}
```

The function is a list of projections. Name, then `task_state`, then
memory if the task still has an `mm`, then signal sets, capabilities,
seccomp, and cpuset. A zombie has already passed `exit_mm` on Day 4, so
the `if (mm)` body is skipped and the file still carries the state line
from `task_state`. That is why `Z` can appear in a file that has no user
address space left to describe.

`task_state` is where Pid and PPid are computed. Lines 163–192:

```c
	rcu_read_lock();
	ppid = pid_alive(p) ?
		task_tgid_nr_ns(rcu_dereference(p->real_parent), ns) : 0;

	tracer = ptrace_parent(p);
	if (tracer)
		tpid = task_pid_nr_ns(tracer, ns);

	tgid = task_tgid_nr_ns(p, ns);
	ngid = task_numa_group_id(p);
	cred = get_task_cred(p);

	task_lock(p);
	if (p->fs)
		umask = p->fs->umask;
	if (p->files)
		max_fds = files_fdtable(p->files)->max_fds;
	task_unlock(p);
	rcu_read_unlock();

	if (umask >= 0)
		seq_printf(m, "Umask:\t%#04o\n", umask);
	seq_puts(m, "State:\t");
	seq_puts(m, get_task_state(p));

	seq_put_decimal_ull(m, "\nTgid:\t", tgid);
	seq_put_decimal_ull(m, "\nNgid:\t", ngid);
	seq_put_decimal_ull(m, "\nPid:\t", pid_nr_ns(pid, ns));
	seq_put_decimal_ull(m, "\nPPid:\t", ppid);
	seq_put_decimal_ull(m, "\nTracerPid:\t", tpid);
```

`PPid` is `real_parent` translated into the reader's pid namespace.
Day 4's reparenting changes `real_parent`, and the next `status` read
prints the new number. `TracerPid` is the ptracer when one is attached,
which is a different parent pointer from `real_parent`. Day 6's session
and process group show up in the `NSpid` / `NSpgid` / `NSsid` lines this
function prints a little later, under `CONFIG_PID_NS` (lines 212–224).
The numbers are namespace-relative. A container and its host can print
different pids for one task.

`task_sig`, called from `proc_pid_status`, prints the pending, shared
pending, blocked, ignored, and caught sets through `render_sigset_t`
(the calls are at lines 298–302). Day 5's three facts — pending,
blocked, disposition — have a text form here. A blocked `SIGUSR1` that
has been posted can show up in `SigPnd` or `ShdPnd` while `SigBlk`
contains the same bit. That is the pending-mode experiment, seen from
procfs in the same form `sigpending` reports.

The lab's `proc_lab` reads its own `status`, the first line of `stat`,
and the `exe` symlink. `exe` is a symlink to the ELF file Day 3 mapped.
A zombie has dropped that mapping. A live process's `readlink` of
`/proc/self/exe` answers "which file was exec'd," which can differ from
the path the shell typed if the file was reached through a link. The
lab checks the open, the read, the close, and the `NUL` terminator. A
deleted-file suffix or an inaccessible link is a property of this
environment when it happens.

`/proc/PID/stat` is a single line and a different function,
`do_task_stat`. It is picky about the command field: the name sits in
parentheses and can contain spaces, so a parser finds the last `)`.
The lab's own child has a controlled name. The instruction pointer in
that line is deliberately uninformative for a running task:

```c
	state = *get_task_state(task);
	vsize = eip = esp = 0;
	permitted = ptrace_may_access(task, PTRACE_MODE_READ_FSCREDS | PTRACE_MODE_NOAUDIT);
	mm = get_task_mm(task);
	if (mm) {
		vsize = task_vsize(mm);
		/*
		 * esp and eip are intentionally zeroed out.  There is no
		 * non-racy way to read them without freezing the task.
		 * Programs that need reliable values can use ptrace(2).
		 *
		 * The only exception is if the task is core dumping because
		 * a program is not able to use ptrace(2) in that case. It is
		 * safe because the task has stopped executing permanently.
		 */
```

`fs/proc/array.c`, `do_task_stat`, lines 460–474. The comment is the
bridge to the next section. User `rip` / `pc` / `epc` and the user stack
pointer change whenever the task runs. A procfs read would sample them
without stopping the task, and the sample would be a race. The kernel
prints zeros and tells you to stop the task with `ptrace` if you need
the registers. The core-dump exception in the comment is the case where
the task has already stopped permanently, so the saved registers stay
still.

---

## 6. A ptrace stop is a task state

strace and GDB are both tracers. They attach with `ptrace`, and the
kernel parks the tracee in `TASK_TRACED` until the tracer says to
continue. That park is the same kind of kernel state as `EXIT_ZOMBIE`:
a value in the task, visible to `wait` and to procfs, with no user
instruction retiring while it holds.

`kernel/signal.c`, `ptrace_stop`, tree `c9b2385d6793`, lines 2133–2160:

```c
	/*
	 * schedule() will not sleep if there is a pending signal that
	 * can awaken the task.
	 */
	set_special_state(TASK_TRACED);

	/*
	 * We're committing to trapping.  TRACED should be visible before
	 * TRAPPING is cleared; otherwise, the tracer might fail do_wait().
	 * Also, transition to TRACED and updates to ->jobctl should be
	 * atomic with respect to siglock and should be done after the arch
	 * hook as siglock is released and regrabbed across it.
	 *
	 *     TRACER				    TRACEE
	 *
	 *     ptrace_attach()
	 * [L]   wait_on_bit(JOBCTL_TRAPPING)	[S] set_special_state(TRACED)
	 *     do_wait()
	 *       set_current_state()                smp_wmb();
	 *       ptrace_do_wait()
	 *         wait_task_stopped()
	 *           task_stopped_code()
	 * [L]         task_is_traced()		[S] task_clear_jobctl_trapping();
	 */
	smp_wmb();

	current->last_siginfo = info;
	current->exit_code = exit_code;
```

`set_special_state(TASK_TRACED)` is the store. `TASK_TRACED` includes
`__TASK_TRACED`, the bit `get_task_state` maps to `t (tracing stop)`.
The comment's two-column picture is the memory-ordering reason for
`smp_wmb`: the tracer's `wait` must observe `TRACED` before it trusts
the stop. `last_siginfo` and `exit_code` hold what the tracer can read
back about why the stop happened. Day 4 used `exit_code` for a
terminating task. Here the same field carries the stop code while the
task is alive and frozen for the tracer.

`get_signal` calls `ptrace_signal` before it honors a caught
disposition (Day 5, around line 2664). A tracer can therefore stop on a
signal, rewrite it, or swallow it, and the tracee stays in `TASK_TRACED`
across that stop. Syscall tracing uses the same state at syscall entry
and exit: the architecture entry path notices the trace flag, and the
task stops where the tracer can read the syscall number out of the ABI
register.

That register is architectural. On x86-64 the number is in `orig_ax`
after a `syscall` instruction. On AArch64 it is in the syscall-number
register the entry code saved, conventionally `x8`. On RISC-V it is
`a7`. strace's table from number to name is a per-ABI table. The stop
itself — `TASK_TRACED`, `wait`, continue — is the shared kernel
mechanism. Day 5's signal frame was architectural because the handler
call is architectural. A ptrace stop is kernel task state, and the
registers the tracer then reads are architectural.

While the tracee is stopped, its user registers sit in the kernel's
`pt_regs` for that task, saved by the trap or syscall entry the way
Week 8 saved registers around `int 0x80` and the way ToyOS saves them
for `iretq`. The tracer issues `ptrace` requests to copy those
registers out. When the tracer continues the task, the kernel returns
to user mode through the normal exception-return path, and the user
instruction stream resumes. Stopping changed the timing. A race that
appears under strace or GDB has been sampled under a scheduler the
tracer perturbed.

---

## 7. strace is that stop, printed as a syscall list

strace attaches, asks to stop at syscall entry and exit, and prints a
decoded line: name, arguments, return value. With `-f` it follows
children, which is how a fork/exec/wait trace stays in one transcript.
The line is the tracer's formatting of register values and of the
kernel's return code. It is evidence about which syscalls the dynamic
loader and libc issued. It is a decoded view, and the spelling depends
on the ABI and on the strace version's tables.

For this week's status mode, a useful trace shows the create call
(`clone` or `clone3`, whichever libc used), the `execve` of the fixed
program, and the parent's `wait4`. An `execve` that returns is a
failure in the old image. An `execve` that is followed by the new
program's own syscalls, and then by an exit of 7, is the success path
Day 4 decoded from the wait word. Match the wait status with `WIFEXITED`
in the program. The trace shows the raw return; the macros are still
how C reads it.

The wrapper traces only the fixed status mode, writes the log under its
temporary report directory, and extracts a few landmarks. If strace is
absent, the honest line is a skip. A skipped tool is a limit on the
report. Inventing a syscall line would attach the report to a machine
you did not run.

Attaching is itself a ptrace stop. The child runs more slowly, and the
order of two runnable tasks can differ from an untraced run. Use the
trace to identify the boundary — before `fork`, before `exec`, inside
the new image, inside a signal, inside `wait` — and keep the timing
claim inside what one trace can support.

---

## 8. GDB is the same stop, plus symbols

GDB attaches or launches through ptrace as well. The extra ingredient
is the ELF symbol table and DWARF line table Week 3 produced at link
time. A breakpoint at `main` is an address the linker assigned to that
symbol, patched into the process so that execution stops and the kernel
reports a ptrace stop. The command file `gdb/week10.gdb` breaks at
`main`, runs, and continues. In batch mode against `proc_lab`, a
successful capture is: this binary, this symbol, this stop, then the
program's own checked marker.

`main` is a C function. The first user instruction after `exec` is the
ELF entry or the dynamic linker, as Week 3's loading lesson followed.
A breakpoint at `main` fires after that startup. Registers at the stop
are the `pt_regs` from section 6, shown with names GDB took from the
ABI and from the debug info. Locals appear when DWARF describes them.
A stripped binary can still have a function symbol and lack the local
names. Say which of those you actually had.

Fork needs an explicit follow-fork setting, because the new task is a
different `task_struct` and GDB is attached to one of them. `exec`
replaces mappings, so the symbols GDB holds have to be reread for the
new image. A signal can be passed, stopped, or swallowed according to
GDB's handle settings, which is `ptrace_signal` from user space. The
command file is evidence when GDB ran. A file that was not executed is
a recipe, and the report says the recipe was not run.

Predict the stop before you look. "Break at `main`, print one argument,
continue, expect the lab marker" is a prediction. A transcript that
confirms it is an observation. A transcript that surprises you names
the first contradicted fact and stops there, which is a better
debugging step than collecting every register.

---

## 9. Version discipline

Write these down before a line-number claim:

```text
uname -r                         the kernel that produced /proc and ptrace
tree id c9b2385d6793             the 5.10.134-based sources these notes quote
config / distributor patches     if you discovered the running tree differs
architecture                     x86-64, AArch64, or RISC-V, for syscall names
compiler, libc, strace, GDB      when a tool's decoding is part of the claim
binary hashes                    the ELF files the lab actually exec'd
```

The running kernel and the tree you are reading can differ. Fedora, the
lab VM, and this Anolis-based tree can carry the same mechanism under
shifted line numbers. When they differ, describe the mechanism at the
level that survived — `EXIT_ZOMBIE` is a retained exit word, `wait4`
copies it out, a signal frame is architectural — and attach line numbers
only to the tree you opened. The notes in Days 4–6 did that for
`c9b2385d6793`. Your report does it for the machine that printed
`uname -r`.

Syscall numbers, `/proc` field order, and GDB's default follow-fork
behavior are versioned. The stable questions are the ones the lab
checks: decoded wait status, `Z` then a reap, pending-then-delivered,
a child whose PGID equals its PID, and the injected errno values.

---

## 10. ToyOS, xv6, and this Linux tree

The comparison is about who holds the exit word, who can stop a task,
and what a debugger can see. It is three concrete designs.

**ToyOS, Weeks 6–9.** One user address space. A fault uses an IDT stub
and, for the vectors ToyOS assigns, an IST stack, and `iretq` resumes
the interrupted context
([Week 6](../../06-toyos-bootstrap-and-exceptions/sections/day-006-tss-ist-page-fault-and-gdb.md)).
The observable markers are explicit: a serial line, a panic string, a
GDB remote stop against the ELF symbols. Week 8 adds a single user
image and an `int 0x80` syscall path, including a pipe, in that one
address space
([Week 8](../../08-user-mode-syscalls-and-ipc/overview.md)). There is no
second task, so there is no `real_parent`, no `EXIT_ZOMBIE`, no
`sigaction` table, and no process group. The frame that resumes a
context is the trap frame. Linux grew a user signal frame and a
`rt_sigreturn` on top of the same idea, and a task list so that a
second task can wait.

**xv6.** A small teaching kernel with a process table. `fork` allocates
a slot and duplicates the caller's user memory. `exec` replaces the
image in that slot and keeps the pid. `exit` stores a status in the
slot and marks it a zombie. `wait` in the parent copies the status out
and frees the slot. An orphan's parent pointer is moved to the init
process. The shell is a loop of `fork`, `exec`, `pipe`, and `wait`,
which is the descriptor half of Day 6 without sessions or a foreground
group. Signals, in the usual base system, are a small kill check on the
way back from a trap, far from Linux's queued `siginfo` and
architecture frame. Read the xv6 revision you have for the real
function names. The idea is the comparison. Pasting a textbook page
would freeze someone else's line numbers into this report.

**This Linux tree.** `task_struct` is the slot, with sharing flags so
threads can share an `mm` and a file table. `do_exit` stores `exit_code`
and `exit_notify` publishes `EXIT_ZOMBIE` until `wait_task_zombie`
copies it through `wait4`. `find_new_reaper` prefers a subreaper over
the namespace init. `__send_signal` posts a pending bit, `get_signal`
chooses a disposition, and the architecture file builds the frame.
`setpgid` and `ksys_setsid` maintain PGID and SID. `proc_pid_status`
prints the result. `ptrace_stop` freezes a live task in `TASK_TRACED`
so strace and GDB can read `pt_regs`. Namespaces, credentials, and
thread groups sit on the same objects. The extra machinery is more
state with an owner, which is why the lab reaps every child and limits
signals to the process family it created.

```text
question            ToyOS                    xv6                     Linux c9b2385d6793
exit word           the one task ends        slot status, zombie     exit_code, EXIT_ZOMBIE
who collects it     the run ends             wait in the parent      wait4 / subreaper
image replace       the loader you wrote     exec in the slot        execve + ELF handler
stop a live task    GDB / explicit marker    debugger on the kernel  TASK_TRACED, ptrace
user resume         iretq                    trap return             exception return,
                                                                     rt_sigreturn for handlers
job name            one context              parent/child            PGID, SID, foreground
text view           serial log               console                 procfs generated
```

One invariant per row is enough. "Who owns the exit status" separates
all three. "Where a pending asynchronous event sits" separates ToyOS's
trap frame from Linux's pending bitmap. "What a debugger stops" is a
GDB remote protocol on ToyOS and a `TASK_TRACED` task on Linux.

---

## 11. Execution trace: one report

This is the shape of
[`../challenges/day-007-debug-report.sh`](../challenges/day-007-debug-report.sh).
The wrapper builds the canonical programs, runs the checked modes, and
writes a temporary directory. It prints the path. It runs without root
and traces only lab processes.

```text
record uname -r, date, compiler, and tool versions
hash the three canonical binaries
read ELF class / machine / type

proc_lab
    open /proc/self/status
        proc_pid_status → task_state → State, Pid, PPid
    open /proc/self/stat
        do_task_stat → one state letter, eip/esp left 0
    readlink /proc/self/exe
        the ELF file this image came from

status mode
    fork, exec, wait, decode exit 7
    optional strace -f
        clone-family, execve, wait4
        or the line "strace unavailable"

optional gdb -batch -x gdb/week10.gdb --args proc_lab
    ptrace stop at main
    continue
    program marker
    or the diagnostic if ptrace policy refused the attach

injected failures
    exec of a missing path → child _exit(127) → parent decodes it
    second waitpid → ECHILD
    sigaction(SIGKILL) → EINVAL
    kill(pid that names nothing, 0) → ESRCH

every child waited for
WEEK10 PASS when the checked invariants hold
```

`WEEK10 PASS` means those invariants held on this run. It is a property
of the programs, the kernel, and the tools that were present. It leaves
the comparison section and the limits for you to write in your own
words.

The injected failures are specified detectors. The missing path is
`ENOENT` in the child, mapped to exit 127; another exec error maps to
126. The parent checks 127, then waits again and requires `ECHILD`,
which is Day 4's consumed-status observation. `SIGKILL` keeps its kernel
disposition, so `sigaction` fails with `EINVAL`. Signal zero posts
nothing and still reports `ESRCH` when the pid names no task. The lab
picks a pid outside the ordinary range for that probe. Those four
results are assertions. They are not a search of every errno.

---

## 12. What the wrapper is allowed to touch

Run the wrapper from the Week 10 directory. It uses the project
Makefile, creates only descendants, reaps them, and sends signals only
where the signal-lab modes already do: to the lab process or its
children. Optional tools stay optional. A ptrace policy that refuses
GDB belongs in the report as a refusal, with the diagnostic the tool
printed.

Predict `WEEK10 PASS`, three hashes, the procfs fields for the lab
process itself, the decoded statuses, and either a real strace/GDB
transcript or an explicit skip. Then write one sentence of comparison
for the exit word, in the form section 10 used, tied to the sources you
opened.

---

## 13. Mastery: Explain · Draw · Observe · Build

The wrapper is
[`../challenges/day-007-debug-report.sh`](../challenges/day-007-debug-report.sh).
A passing run collects evidence. Mastery is the report you can defend,
and these notes do not record it as complete.

### Explain

1. Why does `proc_pid_status` run at read time, and what lock or RCU
   section does `task_state` use while it reads `real_parent`?
2. Why can `status` show `Z (zombie)` after `exit_mm` has dropped the
   address space?
3. Why does `do_task_stat` leave `eip` and `esp` at zero for a running
   task, and what mechanism does its comment name instead?
4. What task state is a strace stop, and which part of the printed line
   is architectural?
5. Why is a breakpoint at `main` later than the first user instruction?
6. What does ToyOS lack that makes `wait4` unnecessary there, and what
   does xv6's process slot store that Linux stores in `exit_code`?

### Draw

Draw the path from `read(/proc/self/status)` to the `State:` line, with
`proc_pid_status`, `task_state`, and `get_task_state` labeled and the
tree id in the corner. Beside it, draw a tracer and a tracee at a
syscall stop: `TASK_TRACED`, `pt_regs`, and the ABI register that holds
the syscall number on the architecture you ran.

### Observe

Run the wrapper. Keep `uname -r`, the tree id if you opened the
sources, the three hashes, the procfs excerpt, the status decode, and
the strace/GDB result or the skip line. Quote a kernel line only from
a tree you identified.

### Build

Write the comparison in your report with one invariant per row, using
section 10's questions. Name the ToyOS marker you would actually look at
(serial line or GDB), the xv6 idea in your own words, and the Linux
function from this tree that implements the same idea. Leave the
wrapper's temporary report path in the write-up so the evidence can be
found again.
