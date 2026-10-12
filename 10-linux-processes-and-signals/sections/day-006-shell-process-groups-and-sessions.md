# Day 6 — Shell Process Groups and Sessions

**Target time:** approximately 3 hours

> A shell's useful trick is that one keystroke can reach every process in
> a pipeline. `fork` gives each process a PID. The terminal needs a second
> name: the job. That name is a process group, inside a session, with one
> group marked foreground on the controlling terminal.

Days 1–3 created tasks and replaced their ELF images. Day 4 kept an exit
word until a parent collected it. Day 5 showed how a signal becomes a
user-mode entry. Today those pieces become the loop a shell runs: `fork`,
arrange a process group, `exec`, `wait`. The group identity is what lets
the terminal deliver `SIGINT`, `SIGTSTP`, and `SIGQUIT` to a job.

The lab observes this process's session and creates one short-lived child
group. It leaves `setsid`, `tcsetpgrp`, and an interactive read-eval loop
alone, so the terminal you are typing in keeps the foreground group it
already has. The model below is the mechanism. The wrapper is a bounded
look at it.

## 1. Syntax needed for this source

`pid`, `pgrp`, and `session` are integer members. `current->session =
current->pgrp = current->pid` writes the same value through one pointer,
right to left. `task[i]->pgrp` indexes the task array, then a member.
`is_orphaned_pgrp` is a loop over `struct task_struct **`: `(*p)->pgrp`
reads the group, `(*p)->p_pptr->session` follows the parent pointer,
and `return 0` or `return 1` reports what the scan found.

Sketch:

```c
for (i = 0; i < NR_TASKS; i++)
        if (task[i] && task[i]->pid == pid)
                task[i]->pgrp = pgid;
```

## 2. Linux 0.12 first: pid, process group, session, orphan group

A pipeline needs a name that is wider than one pid. Linux 0.12 stores
three integers on the task and scans `task[]` to change them.
`sys_setpgid` moves `pgrp` inside the caller's session. `sys_setsid`
starts a session. `is_orphaned_pgrp` asks whether any member still has
a parent in the same session and a different group. Printed pages are
in the 0.12 commentary; the
[0.12 map](../../references/EARLY_LINUX_012_MAP.md) links the exit and
fork chapters beside this one (`kernel/sys.c` is chapter 8.10).

`kernel/sys.c`, `sys_setsid`, printed pages 421–422:

```c
int sys_setsid(void)
{
        if (current->leader && !suser())
                return -EPERM;
        current->leader = 1;
        current->session = current->pgrp = current->pid;
        current->tty = -1;
        return current->pgrp;
}
```

`sys_setpgid` on the same pages writes `task[i]->pgrp = pgid` when the
target is the caller or its child, in this session, and is not already
a session leader. `kernel/exit.c`, `is_orphaned_pgrp`, printed page 398,
returns 0 as soon as it finds a member whose parent is in the same
session and another group:

```c
if (((*p)->p_pptr->pgrp != pgrp) &&
        ((*p)->p_pptr->session == (*p)->session))
        return 0;
```

If the scan finds no such parent, it `return(1)`. `do_exit` uses that
result to `SIGHUP` and `SIGCONT` a newly orphaned group that still has
a stopped job.

What carries forward is the pair of ids, the session leader, and the
orphaned-group rule. What changes in 5.10 is the table and the tty
object: `setpgid`, `ksys_setsid`, pid namespaces, and the terminal's
foreground group. Develop against `kernel/sys.c` in the 5.10 tree. The
fixed `task[]` scan and `tty = -1` are the 0.12 form.

ToyOS has one user task and no second id for a job. 0.12 is the first
place those extra integers show up next to `pid`.

Now move to the 5.10 implementation.

Excerpts are from tree `c9b2385d6793`, a 5.10.134-based kernel. Record
`uname -r` beside that id. `setpgid` and `setsid` are the POSIX calls a
program may rely on. The C functions in `kernel/sys.c` are one
implementation, and their line numbers belong to this tree.

---

## 3. Why a PID is the wrong name for a job

A pipeline such as `producer | consumer` is two tasks. Each has a PID,
an address space, and a file table. The user thinks of them as one job:
Ctrl-C should interrupt both, Ctrl-Z should stop both, and `fg` should
wake both. A signal aimed at one PID misses the other.

The kernel's answer is a process group, a set of tasks that share a
process-group id (PGID). A signal can be posted to that set. A session
is a set of process groups that share a session id (SID), normally the
descendants of one login shell or one terminal session. The controlling
terminal remembers one of those groups as the foreground group. Keyboard
signals go there.

```text
session (SID)
├── foreground process group (PGID = terminal's tpgid)
│     ├── shell, while it is waiting at the prompt
│     └── or the job the shell has placed in front
└── background process groups
      └── jobs the shell started and did not put in front

controlling terminal
    one foreground PGID
    SIGINT  / SIGQUIT / SIGTSTP  → that PGID
    a background read  → SIGTTIN to the reader's group
    a background write, when the terminal mode says so → SIGTTOU
```

Five numbers answer five questions:

```text
PID     which task
PPID    which real_parent, Day 4's kinship pointer
PGID    which job
SID     which session
tpgid   which group currently owns the terminal's foreground
```

Ancestry and group membership are different relations. A shell can place
siblings, and selected descendants, into one PGID. A child can leave its
parent's group by calling `setpgid`. Day 4's orphan is a changed
`real_parent`. An orphaned *process group* is a group that has lost its
connection to a parent in the same session. `kill_orphaned_pgrp`, below,
exists for that second meaning.

---

## 4. `setpgid` moves a task between groups

A new task inherits its parent's PGID and SID at `fork`. The shell then
moves the child, or the child moves itself, before `exec`. Both sides
often call `setpgid`, because the parent and the child run concurrently
and either may reach the call first. The kernel accepts the call from
either side under the rules in this function.

`kernel/sys.c`, `SYSCALL_DEFINE2(setpgid)`, tree `c9b2385d6793`, lines
1026–1053:

```c
SYSCALL_DEFINE2(setpgid, pid_t, pid, pid_t, pgid)
{
	struct task_struct *p;
	struct task_struct *group_leader = current->group_leader;
	struct pid *pgrp;
	int err;

	if (!pid)
		pid = task_pid_vnr(group_leader);
	if (!pgid)
		pgid = pid;
	if (pgid < 0)
		return -EINVAL;
	rcu_read_lock();

	/* From this point forward we keep holding onto the tasklist lock
	 * so that our parent does not change from under us. -DaveM
	 */
	write_lock_irq(&tasklist_lock);

	err = -ESRCH;
	p = find_task_by_vpid(pid);
	if (!p)
		goto out;

	err = -EINVAL;
	if (!thread_group_leader(p))
		goto out;
```

A zero `pid` means the calling thread group. A zero `pgid` means "become
a group leader whose PGID equals that pid," which is what the lab's
helper child does with `setpgid(0, 0)`. The tasklist lock is held so the
parent pointer cannot change during the permission checks. Only a thread
group leader carries a PGID; a sibling thread gets `-EINVAL`.

The permission half, lines 1055–1080:

```c
	if (same_thread_group(p->real_parent, group_leader)) {
		err = -EPERM;
		if (task_session(p) != task_session(group_leader))
			goto out;
		err = -EACCES;
		if (!(p->flags & PF_FORKNOEXEC))
			goto out;
	} else {
		err = -ESRCH;
		if (p != group_leader)
			goto out;
	}

	err = -EPERM;
	if (p->signal->leader)
		goto out;

	pgrp = task_pid(p);
	if (pgid != pid) {
		struct task_struct *g;

		pgrp = find_vpid(pgid);
		g = pid_task(pgrp, PIDTYPE_PGID);
		if (!g || task_session(g) != task_session(group_leader))
			goto out;
	}
```

Read the branches as the shell's race. If the caller is in the parent's
thread group, the target must share the session, and it must still have
`PF_FORKNOEXEC` set — the flag `fork` sets and `exec` clears. After the
child has exec'd, the parent receives `EACCES` and the window for moving
it has closed. If the caller is the target itself, the `else` branch
allows the call. A session leader (`signal->leader`) stays put. Joining
an existing group requires that group to live in the same session.

The update, after `security_task_setpgid` (lines 1082–1095):

```c
	err = security_task_setpgid(p, pgid);
	if (err)
		goto out;

	if (task_pgrp(p) != pgrp)
		change_pid(p, PIDTYPE_PGID, pgrp);

	err = 0;
out:
	/* All paths lead to here, thus we are safe. -DaveM */
	write_unlock_irq(&tasklist_lock);
	rcu_read_unlock();
	return err;
}
```

`change_pid(..., PIDTYPE_PGID, ...)` is the whole effect on the common
path. PGID is a pid-namespace identity of type `PIDTYPE_PGID`, stored
beside `PIDTYPE_PID` and `PIDTYPE_SID`. It is a kernel object. It is the
same object on every ISA this tree builds.

The lab's helper takes the self-move path: the child calls `setpgid(0, 0)`,
reports PID, PGID, and SID through a pipe, and the parent checks that
PID equals PGID and then waits. That proves a group leader. It leaves
the two-sided race, and a multi-process pipeline, as the next thing a
shell must get right. A production shell calls `setpgid` in both parent
and child and treats "already in the right group" as success.

---

## 5. `setsid` starts a session and drops the terminal

A new session is how a daemon, or a shell that wants a new terminal,
detaches from the session that `fork` inherited. `SYSCALL_DEFINE0(setsid)`
at line 1211 of `kernel/sys.c` calls `ksys_setsid`. The work is:

`kernel/sys.c`, `set_special_pids` and `ksys_setsid`, tree
`c9b2385d6793`, lines 1167–1199:

```c
static void set_special_pids(struct pid *pid)
{
	struct task_struct *curr = current->group_leader;

	if (task_session(curr) != pid)
		change_pid(curr, PIDTYPE_SID, pid);

	if (task_pgrp(curr) != pid)
		change_pid(curr, PIDTYPE_PGID, pid);
}

int ksys_setsid(void)
{
	struct task_struct *group_leader = current->group_leader;
	struct pid *sid = task_pid(group_leader);
	pid_t session = pid_vnr(sid);
	int err = -EPERM;

	write_lock_irq(&tasklist_lock);
	/* Fail if I am already a session leader */
	if (group_leader->signal->leader)
		goto out;

	/* Fail if a process group id already exists that equals the
	 * proposed session id.
	 */
	if (pid_task(sid, PIDTYPE_PGID))
		goto out;

	group_leader->signal->leader = 1;
	set_special_pids(sid);

	proc_clear_tty(group_leader);
```

The new SID is the caller's own PID. `set_special_pids` points both
`PIDTYPE_SID` and `PIDTYPE_PGID` at that pid, so a new session leader is
also a process-group leader of a fresh group. `signal->leader` records
the session-leader bit that `setpgid` later refuses to move.
`proc_clear_tty` drops the controlling terminal. The same function then
returns the new session id (line 1201) when the checks passed.

The failure checks are the reason a shell that wants a new session forks
twice, or forks once and has the child call `setsid`. The caller must
not already be a session leader, and its PID must not already be some
group's PGID. The lab's wrapper is already inside a session — often one
created by the program that launched the terminal — and it leaves that
session in place.

After `setsid`, a session leader may open a terminal and become its
controlling process, under the rules of the tty layer. The terminal's
foreground PGID starts as this new group. `tcsetpgrp` later changes that
foreground id. Those steps are how an interactive shell hands the
keyboard to a job and takes it back. They also rearrange the terminal
the IDE is using if you run them against it. Today's wrapper stops
before that handoff.

Day 4's `do_exit` calls `disassociate_ctty` when the last thread of a
group exits. A session leader's exit is what drops the controlling
terminal for the session. The association is kernel state on the task,
released with the rest of the user-facing resources.

---

## 6. An orphaned process group is a job-control fact

Day 4's `exit_notify` calls `kill_orphaned_pgrp` when the whole thread
group dies, and `reparent_leader` calls it when children move to a new
parent. The function's comment names the POSIX rule.

`kernel/exit.c`, `kill_orphaned_pgrp`, tree `c9b2385d6793`, lines
316–345:

```c
/*
 * Check to see if any process groups have become orphaned as
 * a result of our exiting, and if they have any stopped jobs,
 * send them a SIGHUP and then a SIGCONT. (POSIX 3.2.2.2)
 */
static void
kill_orphaned_pgrp(struct task_struct *tsk, struct task_struct *parent)
{
	struct pid *pgrp = task_pgrp(tsk);
	struct task_struct *ignored_task = tsk;

	if (!parent)
		/* exit: our father is in a different pgrp than
		 * we are and we were the only connection outside.
		 */
		parent = tsk->real_parent;
	else
		/* reparent: our child is in a different pgrp than
		 * we are, and it was the only connection outside.
		 */
		ignored_task = NULL;

	if (task_pgrp(parent) != pgrp &&
	    task_session(parent) == task_session(tsk) &&
	    will_become_orphaned_pgrp(pgrp, ignored_task) &&
	    has_stopped_jobs(pgrp)) {
		__kill_pgrp_info(SIGHUP, SEND_SIG_PRIV, pgrp);
		__kill_pgrp_info(SIGCONT, SEND_SIG_PRIV, pgrp);
	}
}
```

A stopped background job is parked in the kernel, waiting for `SIGCONT`
or for someone to move it to the foreground. If every parent who could
do that has left the session, the job would wait with no process left
that is allowed to foreground it. The kernel then posts `SIGHUP` and
`SIGCONT` to the group. `SIGHUP`'s default action terminates. `SIGCONT`
wakes the stop so the hangup can be delivered. `has_stopped_jobs` (line
304) looks for `SIGNAL_STOP_STOPPED`.

This is a different word from Day 4's orphan process. An orphan process
has a new `real_parent` and keeps running. An orphaned process group may
still contain running tasks; the special signal pair is for the stopped
ones. `get_signal` on Day 5 already consults the same idea: a stop
signal other than `SIGSTOP` is discarded when `is_current_pgrp_orphaned`
is true, so a background job in an orphaned group is not stopped by
`SIGTSTP` with nobody left to continue it. `SIGSTOP` still stops.

`__kill_pgrp_info` is a process-group send. It is the kernel-internal
form of the user rule that a negative pid argument to `kill` names a
group. The lab sends no job-control signals and no negative-pid kills.
The excerpt is here so the exit path from Day 4 and the signal path from
Day 5 meet in one place.

---

## 7. A shell is a loop, plus group identity

For one external command the shell:

1. parses and expands the line in the shell process;
2. creates pipes and opens redirection targets before any child that
   must inherit them;
3. `fork`s;
4. in the child, and usually also in the parent, calls `setpgid` so the
   job has a PGID;
5. if the job is foreground, calls `tcsetpgrp` so the terminal's
   foreground id is that PGID;
6. in the child, `exec`s the new ELF image, which keeps the PID, the
   PGID, and the SID;
7. `wait`s, with the group pid when the job has more than one member,
   and reads the exit word Day 4 decoded;
8. restores the terminal's foreground group to the shell and restores
   the terminal modes it saved.

Builtins such as `cd` change the shell's own filesystem context. Running
them only in a child would change a task that exits immediately, and the
shell's cwd would stay where it was. The shell therefore has a split:
some commands are function calls in the shell process, and some are
images loaded by `exec`.

A pipe has to exist before the children. Each child inherits both ends
and closes the end it will not use. If the shell keeps a write end, the
consumer's `read` waits for an EOF that `exit_files` in the producer
will never complete, because the shell's descriptor is still open. After
`dup2` onto the standard stream, the child closes the original pipe end.
`FD_CLOEXEC` from Day 3 drops descriptors the new image must not see.
`exec` failure is still the child, which reports it through the exit
word — the lab maps a missing path to exit status 127 — and the parent
waits. The parent distinguishes "exec failed" from "the new program
exited" by that status, because a successful `exec` does not return to
the old image.

```text
shell
  │ parse
  │ pipe() / open redirections
  │ fork
  │ setpgid in parent ──────┐
  │                         │ child
  │                         │ setpgid(0, pgid)
  │                         │ dup2, close unused ends
  │                         │ execve → new ELF, same PID/PGID/SID
  │ tcsetpgrp if foreground │
  │ waitpid on the group    │  (exit word via wait4, Day 4)
  │ tcsetpgrp back to shell
  │ print the status the shell's language defines
```

The status a shell prints is a presentation choice on top of the wait
word. A pipeline's reported status might be the last command's status,
or the first nonzero status if the shell's `pipefail` option is on.
That choice is the shell's. The kernel's contribution is one wait word
per task, plus the ability to wait for a PGID because `kernel_wait4`
treats a negative pid as `PIDTYPE_PGID` (Day 4, lines 1618–1620).

`SIGCHLD` tells the shell a child changed state. The handler belongs to
Day 5's discipline: a `sig_atomic_t` or one byte on a self-pipe, and the
job table updated on the ordinary path where locks and `malloc` are
consistent. `waitpid` with `WNOHANG` on that path collects the exit word.
`WUNTRACED` and `WCONTINUED` report stops and continues, which an
interactive job-control shell tracks. The canonical lab checks a normal
exit and a signal death. It leaves the stop/continue wait options for a
later exercise.

The foreground wait is a loop over the group, because a pipeline has
several tasks and any of them can stop. Blocking on one PID misses the
sibling that received `SIGTSTP`. The shell's job table is user memory.
The PGID is the kernel name that makes one `wait` and one signal cover
the members.

---

## 8. Execution trace: one foreground command, then the lab's child group

Imagine the shell is the foreground group and the user runs a single
external program that exits 7. This trace is the mechanism. The wrapper
exercises the fork/exec/wait portion and a separate child-led
`setpgid`, and it leaves `tcsetpgrp` uncalled.

```text
shell PGID=S, foreground
    parse "program"
    fork
        child inherits PGID=S, SID, file table
    parent setpgid(child, child)     child setpgid(0, 0)
        whichever runs first: change_pid(PIDTYPE_PGID)
        the child is now PGID=child, still the same SID
    parent tcsetpgrp(terminal, child)     [a real interactive shell]
        terminal foreground id = child's PGID
    child execve("program")
        Day 3: new mappings, same PID, same PGID, same SID
        PF_FORKNOEXEC clears; further parent setpgid gets EACCES
    program runs, _exit(7)
        Day 4: exit_code = 7<<8, EXIT_ZOMBIE, SIGCHLD
    parent waitpid(-PGID) or waitpid(child)
        put_user of the status word
        WIFEXITED / WEXITSTATUS → 7
    parent tcsetpgrp(terminal, shell)
        foreground group is the shell again
```

The lab's checked status mode follows the middle of that picture: fork,
a fixed exec, wait, decode exit 7. The helper follows the `setpgid(0, 0)`
line only. It reports the child's ids on a pipe, the parent checks that
the child is a group leader, and the parent reaps it. Standard input may
or may not be a terminal in the environment that launched the wrapper.
`isatty` on stdin answers that question for this run. A non-terminal
stdin is a true observation about this process, and the session and
process-group fields in `/proc/self/stat` are still meaningful: the
process sits in whatever session its parent arranged.

What you record from the wrapper:

```text
this process    PID, PPID, PGID, SID, isatty(stdin)
status child    fork → exec → wait → decoded exit 7
helper child    setpgid(0,0) → PID == PGID → parent wait
```

PIDs change every run. The invariant is the relationship: the helper's
PGID equals its PID, the status child's exit word decodes as 7, and
every child the wrapper created is waited for.

---

## 9. Where the earlier weeks sit

**Registers and the ISA.** `setpgid` and `setsid` are ordinary syscalls.
The call number and the return register depend on the architecture, as
on Day 4. The values they pass — pid, pgid — are integers in the ABI
registers. After `change_pid`, the user register file is irrelevant to
the new group. There is no signal frame, no `rt_sigreturn`, and no
rewind of a syscall instruction in this path. x86-64, AArch64, and
RISC-V share `kernel/sys.c` for this policy. Day 5's frame was
architectural because the handler entry has to look like a call on that
CPU. A process group does not.

**Virtual memory and ELF.** `exec` replaces mappings and keeps PGID and
SID. The new image is a different ELF file in the same job. A shell
script versus a binary is a difference in the image Day 3 loaded, and
the job-control ids survive either load.

**The C abstract machine.** The shell is a C program (or a program
compiled to the same syscall ABI) looping on library calls. `errno`
after `setpgid` is how the shell sees `EACCES`, `EPERM`, and `ESRCH`.
The job table is ordinary user memory. Updating it from a `SIGCHLD`
handler would meet Day 5's safety rule, so the handler posts a byte and
the loop updates the table.

**Syscalls.** `fork` copies the ids. `setpgid` and `setsid` change them.
`execve` keeps them and clears `PF_FORKNOEXEC`. `wait4` with a group pid
collects the job. `tcsetpgrp` points the terminal at a group. `kill` with
a negative pid posts to a group, which is the user-facing side of
`__kill_pgrp_info`.

**ToyOS and xv6.** ToyOS has one user address space and an `iretq` back
to it. There is no second task to put in a group, and no terminal
foreground id. xv6's shell is a small loop of `fork`, `exec`, `pipe`,
and `wait`. It builds pipelines by arranging file descriptors. It has
no session, no `setpgid`, and no controlling-terminal foreground. That
is enough to see why the descriptor half of section 7 exists. Linux adds
the group and session objects so a terminal can name a job. Read the
xv6 `sh` you have for the actual loop, and keep today's line numbers
attached to `c9b2385d6793`.

---

## 10. What the wrapper is allowed to touch

Run
[`../challenges/day-006-shell-job-control.sh`](../challenges/day-006-shell-job-control.sh)
from the Week 10 directory. It resolves the canonical project, builds
with the project's Makefile, and runs without root. It prints this
process's PID, parent, process group, session, and whether stdin is a
terminal. It runs the fixed fork/exec/wait status mode. A short-lived
helper creates one child, the child calls `setpgid(0, 0)`, and the
parent checks the report and waits. Every descendant is reaped.

The run leaves `setsid` and `tcsetpgrp` uncalled. It sends no
job-control signal. It implements no interactive read loop. Those are
the operations that would move the IDE terminal's foreground group.
Predict, before the run, that the status child decodes as exit 7 and
that the helper child is a process-group leader. Treat the terminal
answer as a property of the environment that started the wrapper.

---

## 11. Mastery: Explain · Draw · Observe · Build

The wrapper is
[`../challenges/day-006-shell-job-control.sh`](../challenges/day-006-shell-job-control.sh).
A passing run shows ids and one child group. The account of why a
terminal needs a PGID is still yours. These notes leave mastery open.

### Explain

1. What question does PGID answer that PID does not?
2. Why does `setpgid` consult `PF_FORKNOEXEC` on the parent's call?
3. What two pid types does `set_special_pids` retarget, and what does
   `proc_clear_tty` drop?
4. How is an orphaned process group different from Day 4's orphan
   process?
5. Why does a shell call `setpgid` in both the parent and the child?
6. Why is this mechanism the same C code on x86-64, AArch64, and RISC-V?

### Draw

Draw a session with one foreground group of two pipeline members and one
background group. Label SID, PGIDs, and the terminal's foreground id.
Then draw the single-command trace from section 8 up through `exec` and
`wait`, and mark the step the lab actually performs and the step it
leaves to an interactive shell.

### Observe

Run the wrapper. Record PID, PPID, PGID, SID, the stdin terminal result,
the decoded status 7, and the helper's PID-equals-PGID result. Record
`uname -r`. If you quote `setpgid` or `ksys_setsid`, name the tree.

### Build

Write a paragraph that places one pipe, two children, and one PGID into
the loop from section 7, including which descriptors each process
closes. Keep it on paper or in the notebook. The wrapper stays the
bounded run: one status child, one group-leader child, every child
waited for.
