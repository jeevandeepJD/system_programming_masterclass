# Day 4 — Wait, Zombies, and Reparenting

**Target time:** approximately 3 hours

> The child has finished. Its instructions will never run again. The parent
> still needs a number: how the child ended. That number has to live
> somewhere after the child's address space is gone.

Days 1–3 built a live task: identity in `task_struct`, a duplicated context
after `fork`, and a replaced ELF image after `exec`. Today follows the task
after user execution ends. The kernel keeps a dead task until a parent
collects the status, because the exit code is kernel state. The program that
produced it has already been dismantled.

## 1. Syntax needed for this source

`do_exit` stores through `current` and does not return to the caller.
The `for` loop walks every open index. `if (current->filp[i])` tests a
pointer. `current->state = TASK_ZOMBIE` and `current->exit_code = code`
write two members and leave the task allocated. `sys_waitpid` later
reads `p->exit_code` and copies that integer out with `put_fs_long`.

Sketch:

```c
for (i = 0; i < NR_OPEN; i++)
        if (current->filp[i])
                sys_close(i);
current->state = TASK_ZOMBIE;
current->exit_code = code;
```

## 2. Linux 0.12 first: `do_exit` keeps a zombie and an exit word

The parent needs a number after the child's memory is gone. Linux 0.12
makes that obvious: `do_exit` frees the page tables and the open files,
then leaves the task in the array with `TASK_ZOMBIE` and `exit_code`.
`sys_waitpid` copies the word and only then calls `release`. Printed
pages are in the 0.12 commentary; the
[0.12 map](../../references/EARLY_LINUX_012_MAP.md) places `kernel/exit.c`
at pages 391–403.

`kernel/exit.c`, `do_exit`, printed page 399:

```c
free_page_tables(get_base(current->ldt[1]),get_limit(0x0f));
free_page_tables(get_base(current->ldt[2]),get_limit(0x17));
for (i=0 ; i<NR_OPEN ; i++)
        if (current->filp[i])
                sys_close(i);
iput(current->pwd);
current->pwd = NULL;
iput(current->root);
current->root = NULL;
iput(current->executable);
current->executable = NULL;
iput(current->library);
current->library = NULL;
current->state = TASK_ZOMBIE;
current->exit_code = code;
```

Trace: drop code, data, files, cwd, root, and the executable inode.
The task stays. `schedule()` runs later in the same function (printed
page 401), and `do_exit` does not return. In `sys_waitpid`, the
`TASK_ZOMBIE` arm (printed page 403) does
`put_fs_long(p->exit_code, stat_addr)` and `release(p)`, then returns
the child's pid.

What carries forward is the retained exit word and the wait that
collects it. The LDT bases and `iput` of a single executable inode are
the 0.12 way of releasing the image.

ToyOS has one user image and an `iretq` back into it. It has no second
task holding an `exit_code` for a parent. 0.12 is where that retained
record shows up in a few assignments.

## 3. Evolution: why the early design changed

Linux 0.12 keeps a zombie slot and an exit word, and it reparents to
`init` (task 1) when the parent is already gone. That is enough while
every task is one thread in one global pid space and the only reaper is
init.

Threads broke the "one task, one process" picture. A thread group
shares the exit word the parent waits for, and a non-leader thread can
exit without ending the group. Pid namespaces made "init" a per-namespace
`child_reaper`, not a single global task 1. Service managers needed a
third choice: an ancestor that opts in as a child subreaper, so orphans
do not jump all the way to the namespace init.

Pidfds added a file-descriptor name for the same wait. A parent can
wait on a pidfd as well as on a pid integer, including a thread-specific
waiter when the group is still alive. The integer can be recycled. The
descriptor refers to that task.

What survived is the wait contract. The kernel keeps an exit word after
the address space is gone, and some parent collects it. The 6.12/current
path below is `exit_notify`, `wait4`, and the reaper walk.

The excerpts below come from Linux 6.12, tag `v6.12`, commit
`adc218676eef`. Record `uname -r` beside that id before you treat a line
number as true of the machine you are running. Function names and call
order move between releases. The POSIX wait contract is what a C program
may rely on; these functions show one implementation of that contract.
Develop against this 6.12/current path.

---

## 4. Why the exit code has to outlive the program

A parent creates a child so that it can later learn what the child did. The
interesting fact often arrives at the end: the child called `_exit(23)`, or
a fatal signal ended it. If the kernel deleted the task at the instant user
execution stopped, that fact would vanish with the task.

The fact also cannot stay in the child's virtual memory. `do_exit` drops the
address space before any parent reads the result. After `exit_mm`, there is
no user mapping in which a C variable could still hold the code. The parent
and the child have separate address spaces from the `fork` on
[Day 2](day-002-fork-cow-and-lineage.md). The parent cannot simply load the
child's stack.

The fact lives in the task, in `exit_code`, and the task stays allocated
until someone in the parent relationship consumes it. That retained task
is the zombie: a kernel record with a PID, an exit word, and enough
accounting for `wait` to copy out. Its user instruction stream is
finished.

This is the lifetime rule from Week 3. A value is usable while its
storage lives. After `exit_mm`, the child's C objects are gone. The exit
word's storage is the task structure, and its lifetime runs until a
waiter collects it or the kernel sees that the waiter has declined it.

---

## 5. Two syscalls put the word in, one family of syscalls takes it out

The C library's `exit` and `_exit` are ordinary functions until they issue a
system call. In this tree the two kernel entries sit together.
`kernel/exit.c`, tree `adc218676eef`, lines 1053–1056 and 1097–1102:

```c
SYSCALL_DEFINE1(exit, int, error_code)
{
	do_exit((error_code&0xff)<<8);
}
```

```c
SYSCALL_DEFINE1(exit_group, int, error_code)
{
	do_group_exit((error_code & 0xff) << 8);
	/* NOTREACHED */
	return 0;
}
```

`exit_group` shifts the status the same way and calls `do_group_exit`
(line 1062), which is `__noreturn`. The shift keeps eight bits of user
status in the high byte and leaves the low byte clear for a normal exit. `WEXITSTATUS`
shifts that byte back down. `WIFEXITED` is the predicate that checks the
low byte first.

`exit` ends the calling thread. `exit_group` records `group_exit_code`,
marks `SIGNAL_GROUP_EXIT`, and brings the other threads to `do_exit` as
well. A single-threaded lab child has one thread, so either entry reaches
the same teardown. The kernel's object is still a task inside a signal
group; Week 11 takes up the other threads.

`do_exit` is marked `__noreturn`. The user instruction pointer that entered
the syscall is a return address the kernel will not use to resume that
program. The kernel stack of this task runs the teardown. When teardown
finishes, `do_exit` calls `do_task_dead` (line 990). The task is off the
path that fetches user instructions.

Fatal signals arrive at the same function from the other direction.
`get_signal`, which Day 5 follows, calls `do_group_exit` with the signal
number for a default fatal action. That number occupies the low bits of the
exit word. `do_notify_parent` in `kernel/signal.c` reads the shape:

```c
	task_cputime(tsk, &utime, &stime);
	info.si_utime = nsec_to_clock_t(utime + tsk->signal->utime);
	info.si_stime = nsec_to_clock_t(stime + tsk->signal->stime);

	info.si_status = tsk->exit_code & 0x7f;
	if (tsk->exit_code & 0x80)
		info.si_code = CLD_DUMPED;
	else if (tsk->exit_code & 0x7f)
		info.si_code = CLD_KILLED;
	else {
		info.si_code = CLD_EXITED;
		info.si_status = tsk->exit_code >> 8;
	}
```

`kernel/signal.c`, `do_notify_parent`, tree `adc218676eef`, lines 2100–2112.
A clear low seven bits means a normal exit, and the status is the high byte.
A nonzero low seven bits means a signal. Bit 7 (`0x80`) marks a core dump.
The userspace macros `WIFEXITED`, `WIFSIGNALED`, `WEXITSTATUS`, and
`WTERMSIG` decode this layout. They are part of the C library's reading of
an ABI word, in the same way Week 2 treated a machine encoding as something
the C abstract machine does not spell out by itself.

Use the predicate before the extractor. `WEXITSTATUS` on a signaled status
shifts bits that encode a signal number. `WTERMSIG` on a normal exit reads
bits that were left clear. `WIFSTOPPED` and `WIFCONTINUED` belong to job
control, which Day 6 introduces; they are separate from the zombie word.

---

## 6. `do_exit` releases the user program and keeps the task

`kernel/exit.c`, function `do_exit`, tree `adc218676eef`, lines 923–951:

```c
	tsk->exit_code = code;
	taskstats_exit(tsk, group_dead);

	exit_mm();

	if (group_dead)
		acct_process();
	trace_sched_process_exit(tsk);

	exit_sem(tsk);
	exit_shm(tsk);
	exit_files(tsk);
	exit_fs(tsk);
	if (group_dead)
		disassociate_ctty(1);
	exit_task_namespaces(tsk);
	exit_task_work(tsk);
	exit_thread(tsk);

	/*
	 * Flush inherited counters to the parent - before the parent
	 * gets woken up by child-exit notifications.
	 *
	 * because of cgroup mode, must be called before cgroup_exit()
	 */
	perf_event_exit_task(tsk);

	sched_autogroup_exit_task(tsk);
	cgroup_exit(tsk);
```

Read the calls as a demolition order.

`tsk->exit_code = code` stores the result while the task structure still
owns it. The calls after it drop resources the dead program must release.

`exit_mm` drops the memory descriptor: page tables, the ELF mappings from
[Day 3](day-003-exec-elf-and-inheritance.md), the stack, the heap, and the
copy-on-write pages from Day 2. The ELF file on disk remains a file. The
process image was the mapping, and the mapping is gone. A zombie keeps a
PID and an exit word after `/proc/PID/exe` and the user mappings are
finished.

`exit_files` and `exit_fs` drop the file table and the filesystem context.
Week 9's file objects are reference-counted; the task's table was one
holder. A parent that still holds its own reference to a shared open-file
description keeps that description. The child's table dies with the child.

`disassociate_ctty`, when this is the last thread in the group, separates
the task from its controlling terminal. Day 6 uses that fact: a session
leader's exit changes the terminal's owner. The comment above
`perf_event_exit_task` records this tree's ordering: counters flush before
the parent is woken by the child-exit notification.

This excerpt stops before `exit_notify`. The same function calls
`exit_notify(tsk, group_dead)` at line 959, after the breakpoint flush
and `exit_tasks_rcu_start`. Notification is where the task becomes a zombie and where children
of this task receive a new parent. Resource teardown and parent
notification run in that order, as two steps of one `__noreturn` function.
`do_exit` panics if the last thread of global init is exiting (lines
907–909). The interrupt-context and idle-task panics live in
`make_task_dead` (lines 1007–1010), which then calls `do_exit`. The lab
stays on ordinary children.

---

## 7. `exit_notify` publishes the zombie

`kernel/exit.c`, function `exit_notify`, tree `adc218676eef`, lines 731–767:

```c
static void exit_notify(struct task_struct *tsk, int group_dead)
{
	bool autoreap;
	struct task_struct *p, *n;
	LIST_HEAD(dead);

	write_lock_irq(&tasklist_lock);
	forget_original_parent(tsk, &dead);

	if (group_dead)
		kill_orphaned_pgrp(tsk->group_leader, NULL);

	tsk->exit_state = EXIT_ZOMBIE;
	/*
	 * sub-thread or delay_group_leader(), wake up the
	 * PIDFD_THREAD waiters.
	 */
	if (!thread_group_empty(tsk))
		do_notify_pidfd(tsk);

	if (unlikely(tsk->ptrace)) {
		int sig = thread_group_leader(tsk) &&
				thread_group_empty(tsk) &&
				!ptrace_reparented(tsk) ?
			tsk->exit_signal : SIGCHLD;
		autoreap = do_notify_parent(tsk, sig);
	} else if (thread_group_leader(tsk)) {
		autoreap = thread_group_empty(tsk) &&
			do_notify_parent(tsk, tsk->exit_signal);
	} else {
		autoreap = true;
	}

	if (autoreap) {
		tsk->exit_state = EXIT_DEAD;
		list_add(&tsk->ptrace_entry, &dead);
	}
```

Hold the order. First the dying task's children are reparented
(`forget_original_parent`). Then, if the whole thread group is dead,
`kill_orphaned_pgrp` may signal a process group that has lost its
connection to the session. Day 6 explains that signal. Then the task's
`exit_state` becomes `EXIT_ZOMBIE`. If the thread group is not empty,
`do_notify_pidfd` wakes `PIDFD_THREAD` waiters before the parent is
notified.

`EXIT_ZOMBIE` is `0x00000020` in `include/linux/sched.h` in this tree. The
procfs state table in `fs/proc/array.c` prints that report bit as
`Z (zombie)`. Day 7 reads that string. The letter `Z` is a projection of
`exit_state`, produced when someone reads `/proc`. The authoritative
transition is this assignment under `tasklist_lock`.

`do_notify_parent` wakes a parent blocked in `wait` and delivers `SIGCHLD`
according to the parent's disposition. `autoreap` comes back true when the
parent has arranged to discard the status: `SIGCHLD` ignored, or
`SA_NOCLDWAIT`, as the comment in `do_notify_parent` (lines 2119–2133)
describes. In that case `exit_state` moves straight to `EXIT_DEAD` and the
task is queued for `release_task`. The zombie is the state in which a
parent who still wants the word can find it. A parent who has declined the
word lets the kernel release the task inside this function.

A non-leader thread takes the `else` branch and autoreaps. The thread-group
leader is the task `wait` reports for the process. Sub-threads fold their
departure into the group. That is why a zombie you can `wait` for is a
group leader whose exit state is still `EXIT_ZOMBIE`.

`release_task` (line 241) is the later free: credentials, signal state, PID
links, and eventually the task structure. Until it runs, the PID still
names this task. After it runs, the PID may be recycled. A number you
wrote down earlier can then name a different task. The lab treats the PID
as valid only between the observation and the `waitpid` that consumes it.

---

## 8. `wait4` copies the word into the parent's address space

The parent learns the word through `wait4`. In this tree libc's
`waitpid` is expected to call it: the comment above
`SYSCALL_DEFINE3(waitpid)` says `sys_waitpid` remains for compatibility.

`kernel/exit.c`, `SYSCALL_DEFINE4(wait4)`, tree `adc218676eef`, lines
1875–1886:

```c
SYSCALL_DEFINE4(wait4, pid_t, upid, int __user *, stat_addr,
		int, options, struct rusage __user *, ru)
{
	struct rusage r;
	long err = kernel_wait4(upid, stat_addr, options, ru ? &r : NULL);

	if (err > 0) {
		if (ru && copy_to_user(ru, &r, sizeof(struct rusage)))
			return -EFAULT;
	}
	return err;
}
```

`kernel_wait4` (line 1816) turns the pid argument into a pid type. A
positive pid selects `PIDTYPE_PID`. Zero and a negative pid select a
process group (`PIDTYPE_PGID`), which is how a shell waits for every
member of a job. `-1` selects `PIDTYPE_MAX`, every eligible child.
The function then fills a `wait_opts` and calls `do_wait`.

The status crosses back to user memory inside `kernel_wait4`.
`kernel/exit.c`, lines 1845–1856:

```c
	wo.wo_type	= type;
	wo.wo_pid	= pid;
	wo.wo_flags	= options | WEXITED;
	wo.wo_info	= NULL;
	wo.wo_stat	= 0;
	wo.wo_rusage	= ru;
	ret = do_wait(&wo);
	put_pid(pid);
	if (ret > 0 && stat_addr && put_user(wo.wo_stat, stat_addr))
		ret = -EFAULT;

	return ret;
```

`wo.wo_stat` was filled from the zombie in `wait_task_zombie` (lines
1233–1235): the group exit code when `SIGNAL_GROUP_EXIT` is set, and
`p->exit_code` otherwise. The same function, before that copy, does a
`cmpxchg` of `exit_state` from `EXIT_ZOMBIE` to `EXIT_DEAD` (or
`EXIT_TRACE` if a ptracer still needs the task). The comment at lines
1164–1172 says the point: one thread owns the reap. After a successful
`cmpxchg`, the status word has been sampled and the task is on the way
to `release_task` unless a tracer still holds it.

`put_user` is the Week 8 boundary in production form. The pointer
`stat_addr` is a user virtual address in the parent's address space. The
kernel checks it and writes one integer. A bad pointer yields `-EFAULT`.
After a successful return, `status` is an ordinary C object whose
representation is the ABI word from `exit_code`. The syscall return
register carries the child's PID, or `-1` / `0` for the error and
`WNOHANG` cases. Which register that is depends on the ISA; section 14
names them. The meaning of the PID and the status word is kernel policy.

`waitpid` returning the PID is success. Returning `0` with `WNOHANG` means
no selected child has changed state yet. Returning `-1` with `ECHILD` means
there is no child matching the request. The lab's injected failure waits
once, consumes the only child, waits again, and requires `ECHILD`. `EINTR`
means a signal arrived while the parent slept; Day 5 is the event model
for that retry.

---

## 9. Execution trace: exit 23, the letter Z, then the reap

This is the zombie half of `challenges/day-004-wait-zombies-orphans.sh`.
The wrapper builds the canonical project and runs the bounded child. PIDs
change every run. The sequence of kernel states does not.

```text
parent                         child                         kernel task
──────                         ─────                         ───────────
fork ─────────────────────────► new task, shared files,
                                private COW address space
                                user code runs
                               _exit(23)
                                  │
                                  ▼
                               sys_exit_group
                               do_group_exit
                               do_exit
                                  │  exit_code = 23 << 8
                                  │  exit_mm, exit_files, exit_fs
                                  ▼
                               exit_notify
                                  │  exit_state = EXIT_ZOMBIE
                                  │  do_notify_parent → SIGCHLD
                                  ▼
                               do_task_dead
                               (no user instructions left)

read /proc/PID/stat ──────────────────────────────────────► state letter Z
                                                               from EXIT_ZOMBIE

waitpid(pid, &status, 0)
   │
   ▼
kernel_wait4 → do_wait → wait_task_zombie
   cmpxchg EXIT_ZOMBIE → EXIT_DEAD
   wo_stat = exit_code
   put_user(wo_stat, &status)
   return pid in the syscall return register

WIFEXITED(status) is true
WEXITSTATUS(status) is 23
release_task frees the task
PID may be reused later
```

Walk it once with the objects named.

1. `fork` creates a second task. Day 2's rule still holds: separate
   descriptor tables, shared open-file descriptions, copy-on-write pages.
2. The child reaches `_exit(23)`. The syscall argument registers are
   saved on the kernel entry frame and then left behind: this user
   context will not resume.
3. `do_exit` stores `(23 & 0xff) << 8` in `exit_code` and `exit_mm`
   drops the child's page tables.
4. `exit_notify` sets `EXIT_ZOMBIE` and notifies the parent. A parent
   blocked in `wait` wakes. A parent polling `/proc` can observe `Z`.
5. The lab polls only a child it created, for at most 100
   one-millisecond delays, then calls `waitpid`. The poll glimpses
   kernel state. The `waitpid` consumes it.
6. `wait_task_zombie` wins the `cmpxchg`, and `put_user` writes the
   word into the parent's `status` object, in the parent's address space.
7. The parent applies `WIFEXITED` and then `WEXITSTATUS` and checks 23.
8. `release_task` drops the last kernel record. After that, the PID
   may be reused for a different task.

The signal-termination case in the same wrapper follows the same skeleton
with a different exit word: low bits hold the signal number, `WIFSIGNALED`
is the predicate, and `WTERMSIG` is the extractor. Normal exit 7 and a
signaled death stay distinguishable because they set different bits in the
same word.

---

## 10. Reparenting: the parent can disappear first

A zombie is a task with an uncollected exit word. An orphan is a task
whose `real_parent` has changed because the old parent exited while this
task still existed. A child can be reparented while it is still running
and later become a zombie of the new parent. A zombie can be reparented
too, so the new parent is the one that collects the word.

The kernel does this in `forget_original_parent`, called from
`exit_notify` before the dying task is marked `EXIT_ZOMBIE`. The children
must already have a live parent by the time this task becomes something a
waiter might release.

`kernel/exit.c`, function `forget_original_parent`, tree `adc218676eef`,
lines 692–725:

```c
static void forget_original_parent(struct task_struct *father,
					struct list_head *dead)
{
	struct task_struct *p, *t, *reaper;

	if (unlikely(!list_empty(&father->ptraced)))
		exit_ptrace(father, dead);

	/* Can drop and reacquire tasklist_lock */
	reaper = find_child_reaper(father, dead);
	if (list_empty(&father->children))
		return;

	reaper = find_new_reaper(father, reaper);
	list_for_each_entry(p, &father->children, sibling) {
		for_each_thread(p, t) {
			RCU_INIT_POINTER(t->real_parent, reaper);
			BUG_ON((!t->ptrace) != (rcu_access_pointer(t->parent) == father));
			if (likely(!t->ptrace))
				t->parent = t->real_parent;
			if (t->pdeath_signal)
				group_send_sig_info(t->pdeath_signal,
						    SEND_SIG_NOINFO, t,
						    PIDTYPE_TGID);
		}
		/*
		 * If this is a threaded reparent there is no need to
		 * notify anyone anything has happened.
		 */
		if (!same_thread_group(reaper, father))
			reparent_leader(father, p, dead);
	}
	list_splice_tail_init(&father->children, &reaper->children);
}
```

`real_parent` is the kinship parent: the task whose `wait` should collect
this child in the ordinary case. `parent` can differ while a tracer is
attached; Day 7's `ptrace` stops use that split. For an untraced task the
two pointers name the same reaper after this loop.

`reparent_leader` (line 663) forces `exit_signal` to `SIGCHLD` so the new
parent receives the usual death notification, and if the child is already
a zombie it notifies the new parent immediately. A zombie whose original
parent died is still a zombie. The wait obligation moved with
`real_parent`. The comment at the top of `forget_original_parent` (lines
685–691) states the two jobs together: make a reaper inherit the children,
and check whether any process group became orphaned.

`find_child_reaper` (line 587) starts from the pid namespace's
`child_reaper`, normally the namespace init. The comment above
`find_new_reaper` (lines 618–624) states the preference: another live
thread in the same thread group, then the nearest ancestor in this pid
namespace that called `prctl` to become a child subreaper, then the init
process of the namespace. The walk itself is:

```c
static struct task_struct *find_new_reaper(struct task_struct *father,
					   struct task_struct *child_reaper)
{
	struct task_struct *thread, *reaper;

	thread = find_alive_thread(father);
	if (thread)
		return thread;

	if (father->signal->has_child_subreaper) {
		unsigned int ns_level = task_pid(father)->level;
		/*
		 * Find the first ->is_child_subreaper ancestor in our pid_ns.
		 * We can't check reaper != child_reaper to ensure we do not
		 * cross the namespaces, the exiting parent could be injected
		 * by setns() + fork().
		 * We check pid->level, this is slightly more efficient than
		 * task_active_pid_ns(reaper) != task_active_pid_ns(father).
		 */
		for (reaper = father->real_parent;
		     task_pid(reaper)->level == ns_level;
		     reaper = reaper->real_parent) {
			if (reaper == &init_task)
				break;
			if (!reaper->signal->is_child_subreaper)
				continue;
			thread = find_alive_thread(reaper);
			if (thread)
				return thread;
		}
```

`kernel/exit.c`, `find_new_reaper`, tree `adc218676eef`, lines 625–654.
The quote stops at the end of the `for` loop. After that walk, the same
function returns `child_reaper`.

Step 2 is why "the new parent is PID 1" is a weak observation inside a
container or a service manager. The pid namespace's init is PID 1 *in that
namespace*. An ancestor that set the subreaper flag is preferred while the
walk stays inside the same pid level. The lab therefore creates its own
subreaper and checks adoption against that process, which is a fact the
program controls.

---

## 11. The subreaper flag is a prctl on the signal struct

`kernel/sys.c`, inside the `prctl` switch, tree `adc218676eef`, lines
2605–2618:

```c
	case PR_GET_TID_ADDRESS:
		error = prctl_get_tid_address(me, (int __user * __user *)arg2);
		break;
	case PR_SET_CHILD_SUBREAPER:
		me->signal->is_child_subreaper = !!arg2;
		if (!arg2)
			break;

		walk_process_tree(me, propagate_has_child_subreaper, NULL);
		break;
	case PR_GET_CHILD_SUBREAPER:
		error = put_user(me->signal->is_child_subreaper,
				 (int __user *)arg2);
		break;
```

`is_child_subreaper` marks this task as willing to adopt descendants.
`has_child_subreaper` marks descendants so `find_new_reaper` knows a walk
is worth doing. `fork.c` in this tree sets the child's `has_child_subreaper` from the
parent's `has_child_subreaper` or `is_child_subreaper` (lines 2564–2570). The propagate walk
updates existing descendants when the flag is turned on.

The lab calls `prctl(PR_SET_CHILD_SUBREAPER, 1)` in the top process, then
creates a middle child and a grandchild. A readiness pipe makes the
grandchild record the middle PID before the middle exits. The grandchild
waits, boundedly, until its parent PID changes, reports the before and
after IDs, and exits. The top process waits for both descendants and
checks that it became the grandchild's parent.

```text
top (subreaper)
  │  prctl(PR_SET_CHILD_SUBREAPER, 1)
  │
  ├── middle
  │     └── grandchild
  │           records middle's PID through the pipe
  │
  middle exits
      forget_original_parent
      find_new_reaper sees has_child_subreaper
      walks to top, which has is_child_subreaper
      grandchild->real_parent = top
  │
  grandchild observes the new PPID, writes the report, exits
  top waitpid's both, and both exit words are consumed
```

That adoption is Linux-specific. The portable C and POSIX surface is
`waitpid` and the status macros. `PR_SET_CHILD_SUBREAPER` is the tool this
lab uses so the experiment has a parent it can name without depending on
whatever process happens to be PID 1 in the surrounding namespace.

Reparenting keeps the orphan running. It supplies a task that is allowed
to collect a future exit word. It is a parent-pointer update under
`tasklist_lock`, followed by a list move of `children` onto the reaper.

---

## 12. Zombie and orphan side by side

```text
question                         zombie                         orphan
───────────────────────────────  ────────────────────────────  ────────────────────────────
what changed?                    this task's user execution    this task's real_parent
                                 has ended
where is the evidence?           exit_state == EXIT_ZOMBIE     real_parent pointer
                                 procfs letter Z               procfs PPid
is the task runnable?            no user context remains       yes, if it has not itself
                                                                   exited
who acts next?                   a waiter consumes exit_code   the new parent, later,
                                                               if this task exits
what does the lab check?        Z, then waitpid, exit 23      PPID moves from middle
                                                               to the subreaper
```

A running grandchild is an orphan after the middle exits, and a zombie
only after the grandchild itself exits. The exit-23 child is a zombie of
its original parent while that parent is still polling. A zombie whose
parent exits before `wait` is reparented and still holds the exit word
until the new parent collects it.

The visible zombie window exists because the parent delays `wait`. The
child has already stored the word and lost its address space. The
operation that removes the zombie is a successful wait, or the autoreap
path inside `exit_notify` when the parent has declined `SIGCHLD`.

---

## 13. How the earlier weeks sit inside this path

[Week 1](../../01-foundations-to-cpu/sections/day-007-control-flow-exceptions-and-privilege.md)
treats an exception as the CPU leaving the current instruction stream so
privileged code can run. `exit` is that departure by request. A fatal
signal is the kernel ending the user stream. In both cases the user
registers are captured on a kernel frame and then dropped as a resume
context. User code supplies `exit_code` only as the syscall argument.

`exit_mm` drops the child's page tables. Copy-on-write pages from Day 2
release when their map count drops. The parent's `status` integer is a
store into the parent's mappings. The ELF file stays on disk; the mapping
Day 3 created is what disappears. The PID names the task. The path names
the file.

`_exit` ends the abstract machine for the child. The library `exit` runs
`atexit` handlers before the group-exit syscall; `_exit` goes straight to
the syscall, so the lab's status word matches the integer the child
passed. After `waitpid`, `status` is an ordinary `int` and the `WIF*`
macros read the ABI layout. `fork` created the wait obligation. `exec`
kept the PID, so the word describes the image that is current at
termination.

ToyOS, Weeks 6–9, has one address space and returns with `iretq`
([Week 6](../../06-toyos-bootstrap-and-exceptions/sections/day-006-tss-ist-page-fault-and-gdb.md)).
A fault frame exists so the interrupted context can resume. There is no
second task holding an `exit_code`. Week 8's syscall path is the entry
shape of `exit` and `wait4`. Week 9's file objects are what `exit_files`
releases. The adoption report is a pipe: close the unused end in each
process, because a leftover write end withholds EOF. xv6 stores the
status in the process-table slot, marks the slot a zombie, and has the
parent's `wait` copy it out. An orphan's parent pointer moves to init.
Read a chosen xv6 revision for the functions. Line numbers in this lesson
belong to Linux 6.12, commit `adc218676eef`.

---

## 14. Architecture lens

Zombies, reparenting, subreapers, and the exit-word layout are kernel
policy shared by x86-64, AArch64, and RISC-V in this tree: `do_exit`,
`exit_notify`, `forget_original_parent`, and `wait4`. The ISA changes
the syscall instruction (`syscall`, `svc`, `ecall`) and the return
register for the PID (`rax`, `x0`, or `a0`). Signal delivery on Day 5
is where the ISA shapes a user-stack frame. Process exit discards the
user context and builds no such frame.

---

## 15. What the wrapper is allowed to touch

Run the day from the Week 10 directory through
[`../challenges/day-004-wait-zombies-orphans.sh`](../challenges/day-004-wait-zombies-orphans.sh).
The wrapper resolves the canonical project, builds with the project's
Makefile, and runs the checked modes. It runs without root and without a
network. It creates only descendants of itself, reaps every child it
creates, and uses `PR_SET_CHILD_SUBREAPER` so the orphan's new parent is
the lab process. The zombie poll is bounded, and `waitpid` follows as
soon as `Z` is seen. The lab child's command name is controlled, so its
`/proc/PID/stat` scan matches that name; a general parser finds the last
`)` of the command field.

Predict three things before you run it: a normal exit and a signal death
stay distinguishable after the macros; state `Z` appears and is then
consumed with exit code 23; the grandchild's parent PID changes from the
middle process to the subreaper. PID values may differ. The decoded
status, the state letter, and the adoption relationship are the
invariants.

---

## 16. Mastery: Explain · Draw · Observe · Build

The wrapper is
[`../challenges/day-004-wait-zombies-orphans.sh`](../challenges/day-004-wait-zombies-orphans.sh).
Passing it is evidence about this tree's lab, and it leaves the explanations
below for you to produce. These notes do not record the day as mastered.

### Explain

1. Why does `exit_code` have to live in the task structure after `exit_mm`?
2. Why does `sys_exit` shift the user code by eight bits before `do_exit`?
3. What does `exit_notify` set, and when does it move the same task on to
   `EXIT_DEAD` inside that function?
4. What does `wait_task_zombie`'s `cmpxchg` guarantee about two waiters?
5. Why can a grandchild's new parent be a subreaper other than the
   namespace init?
6. Which of zombie and orphan describes `exit_state`, and which describes
   `real_parent`?

### Draw

Draw the exit-23 trace from section 9 with your own PID numbers filled in
after a run. Mark the moment user virtual memory disappears, the moment
`EXIT_ZOMBIE` is stored, and the moment `put_user` writes the parent's
`status`. On a second diagram, draw the subreaper tree before and after
the middle process exits, with `real_parent` arrows.

### Observe

Run the wrapper. Record `uname -r`, the tree id `adc218676eef` if you
checked the sources, the decoded statuses, the `Z` observation, and the
before/after parent IDs. Then open `kernel/exit.c` at `do_exit`,
`exit_notify`, `forget_original_parent`, and `wait4` in a tree you have
identified, and confirm the names still exist before you quote a line.

### Build

In the canonical project, keep the existing reap on every success path.
Add a notebook paragraph for the injected double-wait: the first
`waitpid` consumes the child, the second returns `-1` with `ECHILD`.
Explain which kernel state makes the second call fail. Keep the
experiment inside descendants of the lab process, and keep the zombie
window bounded by a wait.
