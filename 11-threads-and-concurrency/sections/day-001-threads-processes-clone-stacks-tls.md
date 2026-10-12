# Day 1 — Threads, Processes, `clone`, Stacks, and TLS

**Target time:** approximately 3 hours

> `pthread_create` returns, and a second stream of instructions is running in
> the same process. Which objects did the kernel share, which did it allocate,
> and which register now names this thread's private data?

Linux 5.10 excerpts below are from tree `c9b2385d6793` (`Makefile` `VERSION` 5,
`PATCHLEVEL` 10, `SUBLEVEL` 134). Each of those excerpts names its path and
function. The listing in section 2 is Linux 0.12, from the annotated book,
and is not that tree.

Week 10 built one task: an address space, a descriptor table, signal
dispositions, and a single set of registers. Today a second task joins that
same address space. The mechanism is the `clone` syscall you already met as
the implementation behind `fork`, with a different flag set and a new stack
pointer.

---

## 1. Syntax needed for this source

A task is a `struct` reached through a pointer. The task table is an array
of those pointers. `current->state` names one member at a fixed offset in
the object `current` points at. A flag word is a bitmask: one byte can mean
something different from the rest of the word. None of that syntax creates
a second address space or a second core.

```c
struct slot {
    long state;
    unsigned long base;
};
struct slot *table[4];
struct slot *current;

void publish(unsigned long flags)
{
    unsigned long rest = flags & ~0xffUL; /* low byte held a different field */
    current->state = 0;
    table[1]->base = rest;
}
```

The 5.10 excerpts later in this day are the same forms: `task_struct` and
`mm_struct` through pointers, and `kernel_clone_args` filled by member name
(`.stack = newsp`, `.tls = tls`).

## 2. Linux 0.12 first: one task, one address space, one switch

Linux 0.12 allows `NR_TASKS` (64) tasks. Each `struct task_struct`
(`include/linux/sched.h`) holds `state`, an `ldt[3]` (empty, code, data and
stack), and a `tss` with `eip`, `esp`, the general registers, `cr3`, and
`ldt`. `TASK_SIZE` is `0x04000000`, 64MB. `copy_mem` gives the child its own
linear slot and its own descriptors (Zhao, `kernel/fork.c` lines 52–55,
printed p. 408):

```c
new_data_base = new_code_base = nr * TASK_SIZE;
p->start_code = new_code_base;
set_base(p->ldt[1],new_code_base);
set_base(p->ldt[2],new_data_base);
```

The comment on `copy_mem` says the child shares the parent's physical pages
and that only the new page-table entries are created. The new linear base
still makes this a second address space. The shared map follows that page
sharing into the early copy-on-write `fork`.

`schedule` ends in `switch_to(next)` (`kernel/sched.c` line 161). The macro
(`include/linux/sched.h` lines 222–235, printed p. 1000) drops out when `n`
is already `current`; otherwise it exchanges `current` and far-jumps through
the incoming TSS selector:

```c
"xchgl %%ecx,_current\n\t"
"ljmp %0\n\t"
```

The 80386 writes the outgoing registers into the outgoing TSS and loads the
incoming ones. Trace that path as: one task record, its register image, its
own address-space base, then a switch that replaces the core's registers.

What carries forward is that split. ToyOS already built it for two ring-0
tasks: a saved context and a private stack in one kernel map. Section 4
below is that reconstruction. The TSS `ljmp` and the fixed 64MB slot stay
behind with 0.12.

Linux 0.12 cannot teach a shared `mm`, `CLONE_VM`, a thread-group id, or a
TLS base. `fork` here installs a new base. The shared
[Linux 0.12 → Linux 5.10 map](../../references/EARLY_LINUX_012_MAP.md)
(Week 11) is the index for that limit. The sections that follow leave this
switch and read Linux 5.10 at tree `c9b2385d6793`: `kernel_clone`,
`copy_mm` under `CLONE_VM`, private stacks, and the TLS register.

## 3. Why a second instruction stream can share an address space

A function call has one instruction pointer. The callee borrows `%rsp` and
returns. A second thread needs its own instruction pointer at the same
moment, so it needs its own saved registers: one core holds one architectural
register file. It needs its own user stack, because the System V AMD64 stack
grows down from `%rsp` and two streams would overwrite each other's frames.
It needs its own kernel stack, because a syscall or timer interrupt lands on
the current task's kernel stack, the same privilege transition Week 8 already
traced.

The program text, the heap, and the open files already live in the address
space and the kernel objects from Week 10. Two tasks that hold the same
`mm_struct` resolve the same virtual address to the same object. That is why
the worker in
[`thread_lab.c`](../../projects/linux-system-programming/src/thread_lab.c)
can receive `&token`, an automatic `int` in `mode_basic`, and return that
pointer. The creator's stack page is mapped in the shared address space for
as long as that frame is alive, which is why `mode_basic` joins before it
returns.

```text
one mm_struct
┌──────────────────────────────────────────────────────────┐
│  text, rodata, data, heap, libc, vDSO                    │
│                                                          │
│  user stack of task A          user stack of task B      │
│  (creator %rsp)                (newsp from clone)        │
│  TLS block A                   TLS block B               │
└──────────────────────────────────────────────────────────┘
         │                                  │
         ▼                                  ▼
   task_struct A                      task_struct B
   kernel stack A                     kernel stack B
   pt_regs / user registers           pt_regs / user registers
   blocked signal mask                blocked signal mask
   FS base → TLS block A              FS base → TLS block B
```

Per task: general-purpose registers, kernel stack, user stack, TLS base,
blocked-signal mask. Process-wide for a normal pthread: the `mm`, the file
descriptor table, and the signal dispositions. The thread-group id, the
number `getpid` returns, is shared as well.

---

## 4. Tasks you have already built

[Week 7's kernel tasks](../../07-memory-processes-and-scheduling/sections/day-004-kernel-tasks-stacks-contexts.md)
are two ring-0 streams in one address space. Each has a private kernel stack
and a saved context. The page tables stay shared. The separation is the stack
and the registers.

[Week 8](../../08-user-mode-syscalls-and-ipc/sections/day-001-ring3-gdt-tss-iret.md)
gives ToyOS one user address space: its own page-table root, a user stack,
and a kernel stack for the privilege transition. That shape is a separate
address space plus a kernel stack.

[Week 10's `fork`](../../10-linux-processes-and-signals/sections/day-002-fork-cow-and-lineage.md)
is the Linux form of a new address space. `SYSCALL_DEFINE0(fork)` calls
`kernel_clone` with `exit_signal = SIGCHLD` and the rest of
`kernel_clone_args` zeroed. The child gets its own `mm` (copy-on-write) and
a copy of the parent's user `%rsp`: the same virtual address in a different
address space.

`pthread_create` is a libc wrapper around `clone` (or `clone3`, which reaches
the same `kernel_clone`). The flags share the `mm`, the file table, the
signal handlers, and the thread-group id, and the wrapper passes a new user
stack and a TLS pointer.

---

## 5. How `clone` reaches `kernel_clone`

On x86-64 this tree selects the parameter order in the `#else` branch of
`kernel/fork.c`. `arch/x86/Kconfig` selects `CLONE_BACKWARDS` only for
`X86_32`, so a 64-bit kernel compiles the signature below. The body that
fills `struct kernel_clone_args` is shared with the other orders.

```c
/* kernel/fork.c :: SYSCALL_DEFINE5(clone) :: tree c9b2385d6793
 * lines 2697–2714, the order x86-64 compiles
 */
SYSCALL_DEFINE5(clone, unsigned long, clone_flags, unsigned long, newsp,
		 int __user *, parent_tidptr,
		 int __user *, child_tidptr,
		 unsigned long, tls)
#endif
{
	struct kernel_clone_args args = {
		.flags		= (lower_32_bits(clone_flags) & ~CSIGNAL),
		.pidfd		= parent_tidptr,
		.child_tid	= child_tidptr,
		.parent_tid	= parent_tidptr,
		.exit_signal	= (lower_32_bits(clone_flags) & CSIGNAL),
		.stack		= newsp,
		.tls		= tls,
	};

	return kernel_clone(&args);
}
```

The x86-64 `syscall` instruction delivers arguments in `%rdi`, `%rsi`,
`%rdx`, `%r10`, and `%r8`: flags, new user stack, parent tid pointer, child
tid pointer, and TLS. The low 8 bits (`CSIGNAL`) become `exit_signal` and
are removed from `flags`. `fork` sets that byte to `SIGCHLD`. A
thread-creating caller leaves it clear and puts the sharing flags in the
rest of the word.

`kernel_clone` is the common path for `fork`, `vfork`, `clone`, and
`clone3`. The part that builds the task and lets it run is:

```c
/* kernel/fork.c :: kernel_clone :: tree c9b2385d6793
 * lines 2589–2605
 */
	p = copy_process(NULL, trace, NUMA_NO_NODE, args);
	add_latent_entropy();

	if (IS_ERR(p))
		return PTR_ERR(p);

	/*
	 * Do this prior waking up the new thread - the thread pointer
	 * might get invalid after that point, if the thread exits quickly.
	 */
	trace_sched_process_fork(current, p);

	pid = get_task_pid(p, PIDTYPE_PID);
	nr = pid_vnr(pid);

	if (clone_flags & CLONE_PARENT_SETTID)
		put_user(nr, args->parent_tid);
```

`copy_process` returns a task that is fully built and still off the run
queue. After the vfork and LRU bookkeeping that follows this excerpt,
line 2620 calls `wake_up_new_task`, which makes the child schedulable. The
parent syscall returns `nr` in `%rax`. The child's saved `%rax` is 0, so
both tasks resume the libc wrapper and branch on `%rax`: the parent
continues `pthread_create`, and the child enters the thread start routine.

---

## 6. `copy_process` copies or shares one resource at a time

Before any of those helpers run, `dup_task_struct` (`kernel/fork.c` line
907) allocates a new `task_struct` and a new kernel stack
(`alloc_thread_stack_node` at line 920). `arch_dup_task_struct` copies the
parent with `*dst = *src`, so the child starts life with the parent's
pointers. The copy helpers then either bump a reference count and keep the
pointer, or allocate a replacement. The kernel stack pointer is rewritten
after the copy (`tsk->stack = stack` at line 936), which is why the child
has a private kernel stack even though the struct was memcpy'd.

`include/linux/sched.h` places the process-wide pointers and the per-task
mask side by side (lines 1129–1132): `signal`, `sighand`, and
`sigset_t blocked`. The blocked mask is a member of the task, so the
struct copy gives the child its own mask, initialized to the parent's, and
a later `pthread_sigmask` changes one task.

The resource walk itself:

```c
/* kernel/fork.c :: copy_process :: tree c9b2385d6793
 * lines 2176–2197
 */
	retval = copy_files(clone_flags, p);
	if (retval)
		goto bad_fork_cleanup_semundo;
	retval = copy_fs(clone_flags, p);
	if (retval)
		goto bad_fork_cleanup_files;
	retval = copy_sighand(clone_flags, p);
	if (retval)
		goto bad_fork_cleanup_fs;
	retval = copy_signal(clone_flags, p);
	if (retval)
		goto bad_fork_cleanup_sighand;
	retval = copy_mm(clone_flags, p);
	if (retval)
		goto bad_fork_cleanup_signal;
	retval = copy_namespaces(clone_flags, p);
	if (retval)
		goto bad_fork_cleanup_mm;
	retval = copy_io(clone_flags, p);
	if (retval)
		goto bad_fork_cleanup_namespaces;
	retval = copy_thread(clone_flags, args->stack, args->stack_size, p, args->tls);
```

`args->stack` is the user `newsp` from the syscall. `args->tls` is the TLS
argument. Both reach the architecture code only here. `copy_fs` is the same
pattern for the current directory and umask (`CLONE_FS`); pthread libc
often shares it too. The four helpers the rest of this lesson reads are
`copy_mm`, `copy_files`, `copy_sighand`, and `copy_signal`.

The kernel refuses a thread group that fails to share the address space.
The check is earlier in `copy_process`:

```c
/* kernel/fork.c :: copy_process :: tree c9b2385d6793
 * lines 1950–1963
 */
	/*
	 * Thread groups must share signals as well, and detached threads
	 * can only be started up within the thread group.
	 */
	if ((clone_flags & CLONE_THREAD) && !(clone_flags & CLONE_SIGHAND))
		return ERR_PTR(-EINVAL);

	/*
	 * Shared signal handlers imply shared VM. By way of the above,
	 * thread groups also imply shared VM. Blocking this case allows
	 * for various simplifications in other code.
	 */
	if ((clone_flags & CLONE_SIGHAND) && !(clone_flags & CLONE_VM))
		return ERR_PTR(-EINVAL);
```

`CLONE_THREAD` requires `CLONE_SIGHAND`, and `CLONE_SIGHAND` requires
`CLONE_VM`. A thread group shares both the dispositions and the address
space. The kernel enforces that before the new task runs.

---

## 7. `copy_mm`: `CLONE_VM` shares the address space

`copy_mm` starts at line 1456 by clearing fault counters and setting
`tsk->mm` and `tsk->active_mm` to `NULL`. The decision is then:

```c
/* kernel/fork.c :: copy_mm :: tree c9b2385d6793
 * lines 1471–1497
 */
	/*
	 * Are we cloning a kernel thread?
	 *
	 * We need to steal a active VM for that..
	 */
	oldmm = current->mm;
	if (!oldmm)
		return 0;

	/* initialize the new vmacache entries */
	vmacache_flush(tsk);

	if (clone_flags & CLONE_VM) {
		mmget(oldmm);
		mm = oldmm;
		goto good_mm;
	}

	retval = -ENOMEM;
	mm = dup_mm(tsk, current->mm);
	if (!mm)
		goto fail_nomem;

good_mm:
	tsk->mm = mm;
	tsk->active_mm = mm;
	return 0;
```

A kernel thread has `current->mm == NULL`, and `copy_mm` returns with the
child's `mm` still `NULL`. That is the Linux shape closest to a Week 7 ToyOS
task: execution state with a shared (or absent) user address space, and the
private state living in the stack.

`fork` leaves `CLONE_VM` clear, so `dup_mm` builds a new `mm_struct`. Virtual
addresses survive and the physical pages stay copy-on-write until a write.
A pthread sets `CLONE_VM`. `mmget` takes another reference on the same
`mm_struct`, and both tasks store that pointer. A store through a pointer in
the child is a store to the parent's object. Both cores can cache the line;
[Week 2's coherence protocol](../../02-assembly-memory-and-c/sections/day-005-cache-organization.md)
moves exclusive ownership when one of them writes. Day 2 asks what C says
about that pair of accesses.

The main thread's stack mapping was created at `execve`, as
[Week 3](../../03-c-toolchain-and-startup/sections/day-007-elf-loading-and-main.md)
described. Further thread stacks are anonymous mappings libc creates in this
shared `mm` before `clone`. `copy_mm` only publishes the `mm` pointer.

---

## 8. `copy_files`: `CLONE_FILES` shares the descriptor table

```c
/* kernel/fork.c :: copy_files :: tree c9b2385d6793
 * lines 1523–1548
 */
static int copy_files(unsigned long clone_flags, struct task_struct *tsk)
{
	struct files_struct *oldf, *newf;
	int error = 0;

	/*
	 * A background process may not have any files ...
	 */
	oldf = current->files;
	if (!oldf)
		goto out;

	if (clone_flags & CLONE_FILES) {
		atomic_inc(&oldf->count);
		goto out;
	}

	newf = dup_fd(oldf, NR_OPEN_MAX, &error);
	if (!newf)
		goto out;

	tsk->files = newf;
	error = 0;
out:
	return error;
}
```

With `CLONE_FILES`, the function increments the reference count and
returns, leaving the pointer `dup_task_struct` already copied. Both tasks
index the same `files_struct`. `open`, `close`, and `dup` in one thread
change the table the other thread sees. Descriptor 3 in either thread is
the same slot.

`fork` leaves the flag clear, so `dup_fd` allocates a new table whose entries
still point at the same open-file descriptions. Week 10 separated those two
layers. A pthread shares the table itself. The `files_struct` is kernel
memory reached through the task pointer; a heap `int` is shared because both
tasks walk the same page tables.

---

## 9. Dispositions are shared; the thread-group id is shared

Signal dispositions live in `sighand->action[]` (`include/linux/sched/signal.h`,
`struct sighand_struct`). `copy_sighand`:

```c
/* kernel/fork.c :: copy_sighand :: tree c9b2385d6793
 * lines 1576–1592
 */
static int copy_sighand(unsigned long clone_flags, struct task_struct *tsk)
{
	struct sighand_struct *sig;

	if (clone_flags & CLONE_SIGHAND) {
		refcount_inc(&current->sighand->count);
		return 0;
	}
	sig = kmem_cache_alloc(sighand_cachep, GFP_KERNEL);
	RCU_INIT_POINTER(tsk->sighand, sig);
	if (!sig)
		return -ENOMEM;

	refcount_set(&sig->count, 1);
	spin_lock_irq(&current->sighand->siglock);
	memcpy(sig->action, current->sighand->action, sizeof(sig->action));
	spin_unlock_irq(&current->sighand->siglock);
```

`CLONE_SIGHAND` bumps the count and keeps the pointer `dup_task_struct`
copied. `sigaction` in one thread installs a handler every thread will
run. The other branch allocates a new table and memcpy's the current
actions; `fork` takes that branch, which is why a child's later
`sigaction` leaves the parent alone. `CLONE_CLEAR_SIGHAND`, just below
this excerpt, resets handlers to the default as a further option.

The thread group itself is `struct signal_struct`, pointed to by
`task->signal`. `copy_signal`:

```c
/* kernel/fork.c :: copy_signal :: tree c9b2385d6793
 * lines 1625–1647
 */
static int copy_signal(unsigned long clone_flags, struct task_struct *tsk)
{
	struct signal_struct *sig;

	if (clone_flags & CLONE_THREAD)
		return 0;

	sig = kmem_cache_zalloc(signal_cachep, GFP_KERNEL);
	tsk->signal = sig;
	if (!sig)
		return -ENOMEM;

	sig->nr_threads = 1;
	atomic_set(&sig->live, 1);
	refcount_set(&sig->sigcnt, 1);

	/* list_add(thread_node, thread_head) without INIT_LIST_HEAD() */
	sig->thread_head = (struct list_head)LIST_HEAD_INIT(tsk->thread_node);
	tsk->thread_node = (struct list_head)LIST_HEAD_INIT(sig->thread_head);

	init_waitqueue_head(&sig->wait_chldexit);
	sig->curr_target = tsk;
	init_sigpending(&sig->shared_pending);
```

`CLONE_THREAD` returns immediately. The child's `signal` pointer stays the
one copied from the parent, `nr_threads` is not reset to 1, and the child
joins the existing group. The other branch allocates a fresh group whose
thread list contains only the new task. That is `fork`.

The id userspace calls the pid is assigned a few hundred lines later:

```c
/* kernel/fork.c :: copy_process :: tree c9b2385d6793
 * lines 2260–2272
 */
	/* ok, now we should be set up.. */
	p->pid = pid_nr(pid);
	if (clone_flags & CLONE_THREAD) {
		p->group_leader = current->group_leader;
		p->tgid = current->tgid;
	} else {
		p->group_leader = p;
		p->tgid = p->pid;
	}

	p->nr_dirtied = 0;
	p->nr_dirtied_pause = 128 >> (PAGE_SHIFT - 10);
	p->dirty_paused_when = 0;
```

Each task has its own `pid` (the tid). `CLONE_THREAD` copies `tgid` and
`group_leader` from the caller. `getpid` reports `tgid`. `gettid` reports
`pid`. `/proc/self` names the group; `/proc/thread-self` names the task.
The `nr_dirtied` fields are accounting reset on the same path; the identity
rule is the `tgid` branch.

The signal picture for a pthread is:

```text
shared signal_struct     tgid, group leader, shared pending, rlimits
shared sighand_struct    disposition table (SIG_DFL / SIG_IGN / handler)
per task                 blocked, real_blocked, private pending
```

`sigaction` updates the shared `action[]`. `pthread_sigmask` updates one
task's `blocked`.

---

## 10. Private registers, two stacks, and a TLS base

The architecture half of creation is `arch/x86/kernel/process.c`,
`copy_thread`. The new kernel stack is framed so the first time the
scheduler switches to this task it resumes at `ret_from_fork`:

```c
/* arch/x86/kernel/process.c :: copy_thread :: tree c9b2385d6793
 * lines 146–163
 */
	childregs = task_pt_regs(p);
	fork_frame = container_of(childregs, struct fork_frame, regs);
	frame = &fork_frame->frame;

	frame->bp = encode_frame_pointer(childregs);
	frame->ret_addr = (unsigned long) ret_from_fork;
	p->thread.sp = (unsigned long) fork_frame;
	p->thread.io_bitmap = NULL;
	clear_tsk_thread_flag(p, TIF_IO_BITMAP);
	p->thread.iopl_warn = 0;
	memset(p->thread.ptrace_bps, 0, sizeof(p->thread.ptrace_bps));

#ifdef CONFIG_X86_64
	current_save_fsgs();
	p->thread.fsindex = current->thread.fsindex;
	p->thread.fsbase = current->thread.fsbase;
	p->thread.gsindex = current->thread.gsindex;
	p->thread.gsbase = current->thread.gsbase;
```

`p->thread.sp` points at a frame on the new kernel stack, and
`ret_from_fork` is the address that frame returns to. `childregs` is the
saved user register block on that stack. The parent's `thread.sp` stays
where it was. On x86-64 the same function snapshots the parent's FS and GS
bases; FS is the System V AMD64 TLS base, and `CLONE_SETTLS` replaces it
for a pthread.

The user register block is then a copy of the parent's syscall frame, with
`%rax` and `%rsp` edited:

```c
/* arch/x86/kernel/process.c :: copy_thread :: tree c9b2385d6793
 * lines 194–223
 */
	frame->bx = 0;
	*childregs = *current_pt_regs();
	childregs->ax = 0;
	if (sp)
		childregs->sp = sp;

#ifdef CONFIG_X86_32
	task_user_gs(p) = get_user_gs(current_pt_regs());
#endif

	if (unlikely(p->flags & PF_IO_WORKER)) {
		/*
		 * An IO thread is a user space thread, but it doesn't
		 * return to ret_after_fork().
		 *
		 * In order to indicate that to tools like gdb,
		 * we reset the stack and instruction pointers.
		 *
		 * It does the same kernel frame setup to return to a kernel
		 * function that a kernel thread does.
		 */
		childregs->sp = 0;
		childregs->ip = 0;
		kthread_frame_init(frame, sp, arg);
		return 0;
	}

	/* Set a new TLS for the child thread? */
	if (clone_flags & CLONE_SETTLS)
		ret = set_new_tls(p, tls);
```

`childregs->ax = 0` is the child's `%rax`. Non-zero `sp` (the syscall's
`newsp`) becomes the child's user `%rsp`, so the child resumes on the stack
libc prepared. Zero `sp`, as in `fork`, keeps the copied user `%rsp` inside
the child's new `mm`. `PF_IO_WORKER` is a kernel-internal user task that
returns through `kthread_frame_init`; a pthread falls through to
`CLONE_SETTLS`.

`set_new_tls` (lines 129–136) calls `do_set_thread_area_64(p, ARCH_SET_FS, tls)`,
which the header macro (`arch/x86/include/asm/ptrace.h` line 386) turns into
`do_arch_prctl_64`. Libc has already written a startup frame on `newsp`.
After `ret_from_fork` the child is in that stub, sees `%rax == 0`, and calls
`return_id` or `consume_owned`.

---

## 11. Architecture lens: which register holds the thread pointer

The TLS block is ordinary memory in the shared `mm`. The pointer to it has
to be a value context-switch saves with the task, because the thread must
be able to find its own block before it has loaded any other address. On
x86-64 that value is the FS base:

```c
/* arch/x86/kernel/process_64.c :: do_arch_prctl_64 :: tree c9b2385d6793
 * lines 785–813, case ARCH_SET_FS
 */
	case ARCH_SET_FS: {
		/*
		 * Not strictly needed for %fs, but do it for symmetry
		 * with %gs
		 */
		if (unlikely(arg2 >= TASK_SIZE_MAX))
			return -EPERM;

		preempt_disable();
		/*
		 * Set the selector to 0 for the same reason
		 * as %gs above.
		 */
		if (task == current) {
			loadseg(FS, 0);
			x86_fsbase_write_cpu(arg2);

			/*
			 * On non-FSGSBASE systems, save_base_legacy() expects
			 * that we also fill in thread.fsbase.
			 */
			task->thread.fsbase = arg2;
		} else {
			task->thread.fsindex = 0;
			x86_fsbase_write_task(task, arg2);
		}
		preempt_enable();
		break;
	}
```

A newborn task takes the `else` branch: FS index 0, and the base stored on
the task for the next switch to load. User code then reads `%fs:offset`.
The System V AMD64 ABI uses that base as the thread pointer and puts the
stack canary at `%fs:0x28`. Libc's `errno` is one such TLS object: the
kernel returns one code in `%rax`, and each thread's wrapper stores it in
that thread's `errno`.

AArch64's user thread pointer is the system register `TPIDR_EL0`. RISC-V's
ABI thread pointer is the integer register `tp` (`x4`). This lesson quotes
the x86 path that writes the FS base. The arm64 and riscv `copy_thread`
functions install their own registers under the same `CLONE_SETTLS` flag.

```text
x86-64    FS base, accessed as %fs:offset     quoted above
AArch64   TPIDR_EL0                           ABI register, setup not quoted
RISC-V    tp (x4), accessed as offset(tp)     ABI register, setup not quoted
```

The compiler emits those accesses for `_Thread_local`. The block lives as
long as the thread does. A pointer into another thread's TLS is reachable
because the `mm` is shared, and the object stays valid while that thread
still owns the block.

---

## 12. Execution trace: `thread_lab basic`

One successful `pthread_create` / `pthread_join` of `return_id(&token)` in
[`mode_basic`](../../projects/linux-system-programming/src/thread_lab.c).
`token` is an `int` with value 42 in the creator's frame.

```text
creator, user mode                         child, still being built
mode_basic:
  token = 42            lives at &token on the creator user stack
  pthread_create(return_id, &token)
    libc allocates a user stack and a TLS block in the shared mm
    libc writes a startup frame on that stack
    syscall clone
      %rdi flags  (CLONE_VM | CLONE_FILES | CLONE_SIGHAND
                   | CLONE_THREAD | CLONE_SETTLS, plus libc extras)
      %rsi newsp  (top of the new user stack)
      %r8  tls    (address of the new TLS block, x86-64 ABI)

kernel, creator's kernel stack
  SYSCALL_DEFINE5(clone) → kernel_clone
  dup_task_struct: new task_struct, new kernel stack
  copy_files:   CLONE_FILES, same files_struct
  copy_sighand: CLONE_SIGHAND, same dispositions
  copy_signal:  CLONE_THREAD, same signal_struct, same tgid
  copy_mm:      CLONE_VM, mmget, same mm
  copy_thread:  kernel frame → ret_from_fork
                childregs->ax = 0
                childregs->sp = newsp
                CLONE_SETTLS → ARCH_SET_FS on the child task
  wake_up_new_task
  parent returns tid in %rax

creator resumes pthread_create             child resumes on newsp
  stores pthread_t                         libc stub sees %rax == 0
  pthread_join                             calls return_id(&token)
                                           return_id returns &token
                                           thread exit
  join copies the returned pointer
  result == &token
  prints pthread_create_join=ok result=42
```

Writes to `token` are sequenced before `pthread_create`, and creation
synchronizes with the new thread's start, so the child's read happens after
that write. `pthread_join` synchronizes with the child's termination, so
the creator's comparison happens after `return_id` returns. The ownership
mode uses the same two edges around a heap object: the child writes
`value *= 2` only after create, and the parent reads and `free`s only after
join. Day 2 names the edge that is absent when two threads update one
non-atomic object in between.

---

## 13. Mastery: Explain · Draw · Observe · Build

Run [`../challenges/day-001-thread-model.sh`](../challenges/day-001-thread-model.sh).
It builds `thread_lab`, runs `basic` and `ownership`, and prints the
lifetime-relevant source. Predict `pthread_create_join=ok result=42` and
`joined_value=42` with `freed_by=parent`. Leave thread ids and scheduling
order unpredicted.

### Explain

`pthread_create` is libc around `clone` / `kernel_clone`, with flags that
share the `mm` (`CLONE_VM`), the file table (`CLONE_FILES`), the signal
handlers (`CLONE_SIGHAND`), and the thread-group id (`CLONE_THREAD`), plus
a new user stack and TLS (`CLONE_SETTLS`). Per task: registers, kernel
stack, user stack, TLS base, `blocked`. Process-wide: `mm`, descriptor
table, dispositions, `tgid`. On x86-64, `copy_thread` writes
`childregs->sp` and clears `childregs->ax`, and `ARCH_SET_FS` records the
TLS base. AArch64 uses `TPIDR_EL0`. RISC-V uses `tp`.

### Draw

1. One `mm_struct` containing two user stacks and two TLS blocks, and two
   `task_struct`s under it with private kernel stacks.
2. The `copy_process` order: files, sighand, signal, mm, then
   `copy_thread(stack, tls)`.
3. The `basic` trace from `token = 42` through `clone` to the joined
   pointer comparison. Mark which edge is `pthread_create` and which is
   `pthread_join`.

### Observe

From the challenge output, record:

- the two checked lines (`result=42`, `joined_value=42`);
- `uname` and the compiler version the script prints;
- the source range the script prints, and on each line say whether the
  object is the shared `mm`, the creator stack, the heap, or a
  `pthread_t`.

### Build

On the source range the script prints, label `token`, the heap `item`, and
the `pthread_t` with where the object lives and which intervals of the
create/join trace may touch it. Then name the `clone` flag for each shared
column: `CLONE_VM`, `CLONE_FILES`, `CLONE_SIGHAND`, `CLONE_THREAD`.

### Why? notebook

1. Why does a second thread need a new user stack when it already shares
   the address space?
2. Why does `fork` leave `newsp` zero while `pthread_create` passes a
   pointer?
3. Why does `copy_files` under `CLONE_FILES` increment a counter and
   return without assigning `tsk->files`?
4. Why does the kernel reject `CLONE_THREAD` without `CLONE_SIGHAND`, and
   `CLONE_SIGHAND` without `CLONE_VM`?
5. Why can `getpid` match in two threads whose `/proc/.../task` ids differ?
6. Why is the blocked-signal mask per task while `sigaction` is shared?
7. Why is the child's `%rax` 0 when the parent receives a tid?
8. Why does x86-64 store the TLS base on the task in the `else` branch of
   `ARCH_SET_FS`?
9. Why does Week 7's shared page tables plus private kernel stacks line up
   with `CLONE_VM`, while Week 8's private page-table root lines up with
   `fork`?

---

## References used selectively

Linux tree `c9b2385d6793`, paths named on each excerpt above.

**Next:** the shared `mm` makes both threads' loads and stores hit one object.
Day 2 asks when the C abstract machine still assigns that program a meaning.
