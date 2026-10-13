# Day 1 — A Process Is a Live Task

**Target time:** approximately 3 hours

- Why a task object exists, beside the ELF file: 35 minutes
- The `task_struct` graph and the objects it references: 50 minutes
- PID namespaces, registers, and page tables: 40 minutes
- One execution trace of `proc_lab`: 30 minutes
- Mastery, then the wrapper as evidence: 25 minutes

> The same ELF file has been started twice. Why does each run have its own
> instruction pointer, its own page-table root, and its own PID?

Week 2 taught the architectural register file: `RIP` names the next
instruction, and a general-purpose register holds a value only while some
saved context says it belongs to this stream of execution. Week 5 taught the
page-table walk: a virtual address becomes a physical frame only when a root
pointer and a chain of table entries say so. Week 3 taught that a C program
on disk is an ELF image — `PT_LOAD` segments, `e_entry`, and, for a dynamic
executable, `PT_INTERP` — plus an initial stack the loader builds before
`main`. Weeks 6–9 built the small operating system that uses those pieces:
one bootstrap address space, then kernel tasks with saved registers and
private stacks, then a ring-3 process with its own page-table root, entered
by `iretq`, calling the kernel through `int 0x80`, with `copy_from_user`
checking every user pointer.

Linux is that shape with sharing, credentials, and lifetime added. A program
is still an ELF file. A process is a live task plus the kernel objects the
task references.

## 1. Syntax needed for this source

The early task code is a structure, a pointer, an array index, a bit
clear, and a function that returns an `int`. `p->pid` means `(*p).pid`:
follow the pointer, then read the member. `task[i]` selects one slot.
`current->filp[fd]` is that pattern twice: the running task, then one
file pointer in its array. `~(1 << fd)` is a mask with bit `fd` clear.
A call such as `sys_open` passes arguments and returns the index, or a
negative error.

Sketch:

```c
struct task_struct * p = task[i];
p->filp[fd] = f;
p->close_on_exec &= ~(1 << fd);
return fd;
```

## 2. Linux 0.12 first: the fixed task array and `current->filp`

Read the small task table before the 6.12 graph. Linux 0.12 keeps at
most 64 tasks. Each is a `task_struct` in `include/linux/sched.h`
(printed pages 203–204): `state`, `pid`, `pgrp`, `session`, `signal`,
`exit_code`, `filp[NR_OPEN]` (32 `struct file *`), `ldt[3]`, and a
`tss`. The running task is `current`. A descriptor is an index into
that task's `filp`, and the open-file object holds the inode and the
offset. Printed pages below are from Zhao Jiong, *A Heavily Commented
Linux Kernel Source Code* (2019). The
[0.12 map](../../references/EARLY_LINUX_012_MAP.md) collects them.

`kernel/sched.c`, printed page 356:

```c
struct task_struct *current = &(init_task.task);
struct task_struct * task[NR_TASKS] = {&(init_task.task), };
```

`fs/open.c`, `sys_open`, printed page 824:

```c
for(fd=0 ; fd<NR_OPEN ; fd++)
        if (!current->filp[fd])
                break;
if (fd>=NR_OPEN)
        return -EINVAL;
current->close_on_exec &= ~(1<<fd);
f=0+file_table;
for (i=0 ; i<NR_FILE ; i++,f++)
        if (!f->f_count) break;
if (i>=NR_FILE)
        return -EINVAL;
(current->filp[fd]=f)->f_count++;
```

Trace: scan `current->filp` for a free index, then scan the global
`file_table` for `f_count == 0`. Store that `struct file *` in the
task and bump the count. On printed page 825 the same function sets
`f_inode` and `f_pos` and `return (fd)`.

What carries forward is the task record, the pid, and a small integer
that names an open-file object. The fixed `task[64]` and embedded
`filp[]` are the 0.12 picture of that idea.

ToyOS already has one `struct process` with a page-table root, an
entry, a kernel stack, and a pid, entered by `iretq`
([`process.c`](../../projects/toyos/src/process.c)). 0.12 is that
record repeated in `task[]`.

## 3. Evolution: why the early design changed

The fixed `task[64]` and the embedded `filp[]` fit a uniprocessor with a
small process limit. SMP and larger machines made a global array the
bottleneck: every lookup walked the same slots, and a task could not
outlive a slot that the next `fork` needed to reuse.

The pressure split the record. A task became a dynamically allocated,
reference-counted object. The open-file array moved into its own
`files_struct`, so threads can share a table or copy one. Credentials,
the memory descriptor, and signal state became objects the task points
at, with their own reference counts, instead of fields baked into one
slot.

Pid namespaces then made the integer a view. One task can show a
different number in a container than on the host, so the kernel keeps a
`struct pid` and looks the number up in the caller's namespace. The
tables that find a task by that number grew hash links and RCU-protected
walks, because a global lock around `task[]` does not scale.

What survived is the idea from section 2: a task record, a pid, and a
small integer that names an open-file object. ToyOS still has the small
form, one `struct process`. The 6.12/current path below is the same
graph, allocated and shared instead of slotted.

The source excerpts below are from Linux 6.12, tag `v6.12`, commit
`adc218676eef`. Field layout in a later mainline release can move. The
names quoted here are what this revision actually contains. Develop
against this 6.12/current shape.

---

## 4. The hardware already has registers and a page-table root

A core can execute one instruction stream at a time. `RIP` (or the program
counter on another ISA) selects the instruction. The page-table root selects
which translations that instruction's loads and stores use. When the core
stops one stream and resumes another, both of those values have to be stored
somewhere that outlives the core's architectural registers.

Week 6 installed one kernel address space at bootstrap and left the core in
it. Week 7 needed a second fact: two kernel tasks share that address space
and still need different resume points. Each ToyOS kernel task therefore
keeps a saved context — instruction pointer, stack pointer, callee-saved
registers — and a private kernel stack. Switching tasks writes the core's
registers into the outgoing context and loads the incoming one. The page
tables stay the bootstrap tables, because those tasks are both ring 0 and
both trusted with the whole kernel map.

Week 8 added a user image that must be untrusted. `struct process` in
[`projects/toyos/src/process.c`](../../projects/toyos/src/process.c) holds a
page-table root, an ELF entry, a user stack, a kernel stack, and a PID.
`process_init` builds a private PML4, maps the embedded ELF's `PT_LOAD`
segments, and maps a user stack. `process_start` activates that root and
enters CPL 3 through `iretq`, which loads user `RIP`, `CS`, `RFLAGS`, `RSP`,
and `SS` from a frame the kernel prepared. A later `int 0x80` is the
privilege boundary: the CPU switches to the kernel stack, the stub saves the
user registers, C handles the request, and `iretq` restores the user
continuation. `copy_from_user` walks the user root and copies one checked
byte at a time, because a value in a user register is an address in that
user map. The kernel copies through the checked path before it uses the
bytes.

That is already a process in miniature: identity, a register resume point, a
page-table root, and a kernel stack for crossings. It is one process, with
one address space, and with authority implied by "this is the user program we
loaded." Linux keeps every one of those roles and then allows many tasks to
share some of the heavy objects, to carry credentials as data, and to outlive
their user mappings long enough for a parent to observe them.

---

## 5. A program is an ELF file

An ELF file is a sequence of bytes with a header. Week 3 separated two views
of those bytes. Section headers describe how the linker grouped `.text`,
`.rodata`, `.data`, and `.bss`. Program headers describe the memory image a
loader builds. A `PT_LOAD` header supplies a file offset, a virtual address,
a file size, a memory size, and permission flags. Bytes past `p_filesz` and
inside `p_memsz` are the zero-filled part of the image. `e_entry` is a
virtual address in that image. `PT_INTERP`, when present, names another ELF
file, the interpreter, whose entry runs before the executable's entry.

None of those fields is an architectural register. The file has no current
`RIP`, no open descriptors, no credentials, and no pending signals. Copying
the file copies the recipe. Starting it twice requires two live contexts that
can each hold a `RIP` and a page-table root.

ToyOS makes the same cut inside one function. `load_elf` checks the magic,
ELF64, little-endian, machine 62 (`EM_X86_64`), and each program header of
type `ELF_LOAD`. For each such header it allocates frames and calls
`map_page_in` on `process->root`. The entry virtual address is stored in
`process->entry`. The CPU fetches from that address only after `iretq` has
loaded it into `RIP` and the PML4 has been activated. Until then the ELF
image is input data.

Linux's loader, visited on Day 3, follows the same headers — `PT_LOAD`,
`PT_INTERP`, `e_entry` — and writes them into a new memory descriptor. The
descriptor is then hung off an existing task. The file remains the file.

```text
ELF file on disk                         live task
─────────────────────                    ──────────────────────────────
e_entry, PT_LOAD, PT_INTERP              task_struct
bytes, no registers                      saved user registers / kernel stack
no page tables                           mm->pgd  (page-table root)
no PID                                   pid, tgid, looked up in a namespace
no credentials                           real_cred, cred
no descriptors                           files, fs
```

---

## 6. `task_struct` is the schedulable object

In `include/linux/sched.h`, `struct task_struct`, Linux 6.12 commit
`adc218676eef`, lines 778–801:

```c
struct task_struct {
#ifdef CONFIG_THREAD_INFO_IN_TASK
	/*
	 * For reasons of header soup (see current_thread_info()), this
	 * must be the first element of task_struct.
	 */
	struct thread_info		thread_info;
#endif
	unsigned int			__state;

	/* saved state for "spinlock sleepers" */
	unsigned int			saved_state;

	/*
	 * This begins the randomizable portion of task_struct. Only
	 * scheduling-critical items should be added above here.
	 */
	randomized_struct_fields_start

	void				*stack;
	refcount_t			usage;
	/* Per task flags (PF_*), defined further below: */
	unsigned int			flags;
	unsigned int			ptrace;
```

`__state` is the scheduler's classification of this task. The comment
above the `TASK_*` macros says `task->__state` is about runnability,
separate from `exit_state`. `TASK_RUNNING` is zero. `saved_state` holds
a state to restore for some sleepers. `stack` is the kernel stack. That is the stack Week 8's syscall
frame lived on. Entry from user mode saves the user registers there; the C
syscall handler sees a frame; the return path restores them. `usage` is a
reference count on the task object itself, so the object can remain reachable
after the task has stopped running. That is the lifetime hook Day 4 will use
when a parent still needs an exit status. `current` on a CPU names the
`task_struct` that owns the core, the way ToyOS's `current` names the running
kernel task.

The rest of the process is a graph of pointers out of this object. The
pointers are how sharing works: two tasks can hold the same address, and a
reference count on the target decides when the target is freed.

```text
task_struct
├── stack, thread          registers and the kernel stack (per task)
├── pid, tgid              numbers cached from struct pid
├── real_parent, parent    lineage
├── mm, active_mm          user address space; page-table root is mm->pgd
├── files                  descriptor table
├── fs                     cwd, root, umask
├── real_cred, cred        credential objects
├── signal, sighand        process-wide signal state and dispositions
├── blocked, pending       this task's mask and pending set
├── nsproxy                namespaces, including the PID namespace
└── exit_state             life after the task has stopped executing
```

---

## 7. Memory, exit, identity, credentials, files, signals

Still in `include/linux/sched.h`, `struct task_struct`, same tree, lines
921–929. The memory and exit fields:

```c
	struct mm_struct		*mm;
	struct mm_struct		*active_mm;
	struct address_space		*faults_disabled_mapping;

	int				exit_state;
	int				exit_code;
	int				exit_signal;
	/* The signal sent when the parent dies: */
	int				pdeath_signal;
```

`mm` is the user address space. `active_mm` is the address space whose page
tables are installed while this task runs. A user task points both fields at
its own `mm`. A kernel thread has no user image: `copy_mm` in `kernel/fork.c`
returns early when `current->mm` is absent, and the thread runs with a
borrowed `active_mm` so the core still has a page-table root for kernel
mappings. Week 7's kernel tasks were that case in a smaller kernel. They
shared the bootstrap page tables and never owned a private user `mm`.

`exit_state`, `exit_code`, and `exit_signal` sit on the task because the user
mappings and the "someone must observe how this ended" fact have different
lifetimes. The address space can be torn down while the task record still
holds a status for the parent. Day 4 follows that handoff. The field is
already visible here so the process model includes death as data on the
task object.

Identity and lineage, same struct, lines 1018–1042:

```c
	pid_t				pid;
	pid_t				tgid;

#ifdef CONFIG_STACKPROTECTOR
	/* Canary value for the -fstack-protector GCC feature: */
	unsigned long			stack_canary;
#endif
	/*
	 * Pointers to the (original) parent process, youngest child, younger sibling,
	 * older sibling, respectively.  (p->father can be replaced with
	 * p->real_parent->pid)
	 */

	/* Real parent process: */
	struct task_struct __rcu	*real_parent;

	/* Recipient of SIGCHLD, wait4() reports: */
	struct task_struct __rcu	*parent;

	/*
	 * Children/sibling form the list of natural children:
	 */
	struct list_head		children;
	struct list_head		sibling;
	struct task_struct		*group_leader;
```

`pid` and `tgid` are `pid_t` values cached on the task. `real_parent` is the
creating parent. `parent` is the task that receives `SIGCHLD` and that
`wait4` reports. For an ordinary child they name the same task. The structure
keeps both because a tracer can become `parent` while `real_parent` remains
the creator. `children` and `sibling` are the lists that make "lineage" a
walk of kernel pointers. `group_leader` points at the task that represents
the thread group. A single-threaded program is its own leader.

Credentials, same struct, lines 1094–1114. The comments are the kernel's own names for the two
pointers:

```c
	/* MM fault and swap info: this can arguably be seen as either mm-specific or thread-specific: */
	unsigned long			min_flt;
	unsigned long			maj_flt;

	/* Empty if CONFIG_POSIX_CPUTIMERS=n */
	struct posix_cputimers		posix_cputimers;

#ifdef CONFIG_POSIX_CPU_TIMERS_TASK_WORK
	struct posix_cputimers_work	posix_cputimers_work;
#endif

	/* Process credentials: */

	/* Tracer's credentials at attach: */
	const struct cred __rcu		*ptracer_cred;

	/* Objective and real subjective task credentials (COW): */
	const struct cred __rcu		*real_cred;

	/* Effective (overridable) subjective task credentials (COW): */
	const struct cred __rcu		*cred;
```

`min_flt` and `maj_flt` count faults on the task. Day 2's copy-on-write fault
is one of the events that increments them. The credential pointers are
separate objects. The comment marks them copy-on-write: a task that changes
credentials installs a new `struct cred` and drops its reference on the old
one, which matters when another task still holds the previous object. A
syscall authorizes against this object. ToyOS had no such object; the single
user process was allowed to issue the teaching syscalls by being the process
that `iretq` had entered. Linux records authority as data so that `exec` can
change it (Day 3 calls `commit_creds`) and so that two tasks can carry
different authority in the same kernel.

Files, namespaces, and signals, same struct, lines 1140–1160:

```c
	/* Filesystem information: */
	struct fs_struct		*fs;

	/* Open file information: */
	struct files_struct		*files;

#ifdef CONFIG_IO_URING
	struct io_uring_task		*io_uring;
#endif

	/* Namespaces: */
	struct nsproxy			*nsproxy;

	/* Signal handlers: */
	struct signal_struct		*signal;
	struct sighand_struct __rcu		*sighand;
	sigset_t			blocked;
	sigset_t			real_blocked;
	/* Restored if set_restore_sigmask() was used: */
	sigset_t			saved_sigmask;
	struct sigpending		pending;
```

`fs` is filesystem context: root, current directory, umask. `files` is the
open-file table, indexed by descriptor number. `nsproxy` holds the namespaces
this task lives in, including the PID namespace that interprets `pid` and
`tgid`. `signal` is the process-wide signal state a thread group shares.
`sighand` is the disposition table. `blocked` and `pending` belong to this
task. The split is the reason a later lesson can say "the mask is per task"
and "the disposition is per thread group" by pointing at two different
fields.

The architecture-specific tail must stay last. Same struct, same tree,
lines 1595–1610:

```c
	/*
	 * New fields for task_struct should be added above here, so that
	 * they are included in the randomized portion of task_struct.
	 */
	randomized_struct_fields_end

	/* CPU-specific state of this task: */
	struct thread_struct		thread;

	/*
	 * WARNING: on x86, 'thread_struct' contains a variable-sized
	 * structure.  It *MUST* be at the end of 'task_struct'.
	 *
	 * Do not put anything below here!
	 */
};
```

`thread` holds CPU-specific task state. On x86 the comment requires it at the
end because `thread_struct` varies in size. User general-purpose registers at
a syscall or interrupt boundary live in a `pt_regs` frame on the kernel
stack. While the task is running in user mode, those values are in the core.
When the scheduler switches away, `RIP` from Week 2 is stored with the task.
The ELF file stores `e_entry`. The live instruction pointer is a property of
the task.

---

## 8. Page tables live in `mm`, registers live on the task

In `include/linux/mm_types.h`, `struct mm_struct`, same tree, lines
790–817, the root sits with the virtual-memory areas:

```c
struct mm_struct {
	struct {
		/*
		 * Fields which are often written to are placed in a separate
		 * cache line.
		 */
		struct {
			/**
			 * @mm_count: The number of references to &struct
			 * mm_struct (@mm_users count as 1).
			 *
			 * Use mmgrab()/mmdrop() to modify. When this drops to
			 * 0, the &struct mm_struct is freed.
			 */
			atomic_t mm_count;
		} ____cacheline_aligned_in_smp;

		struct maple_tree mm_mt;

		unsigned long mmap_base;	/* base of mmap area */
		unsigned long mmap_legacy_base;	/* base of mmap area in bottom-up allocations */
#ifdef CONFIG_HAVE_ARCH_COMPAT_MMAP_BASES
		/* Base addresses for compatible mmap() */
		unsigned long mmap_compat_base;
		unsigned long mmap_compat_legacy_base;
#endif
		unsigned long task_size;	/* size of task vm space */
		pgd_t * pgd;
```

`mm_mt` is the maple tree of virtual-memory areas. `pgd` is the page-directory
root, a `pgd_t *`. Week 5's walk starts there: each index selects the next
table, and the leaf names a frame and permission bits. ToyOS stores the same
idea in `process->root` and loads it when `address_space_activate` runs.
Linux loads `mm->pgd` when it switches to a task whose `active_mm` requires
a different root.

A C pointer, from Week 2 and Week 3, is a virtual address interpreted by
this root. The abstract machine's objects — the stack slot, the heap block,
the string literal in `.rodata` — occupy mappings in this `mm`. The
`task_struct`, the `struct cred`, and the descriptor table occupy kernel
memory. User mode fetches and stores are translated through `mm->pgd` and
fault if the leaf is absent or the permission bit rejects the access.

That is why a syscall argument that "is a pointer" still has to be copied.
Week 8's `copy_from_user` refuses a kernel-range address and walks the user
root one byte at a time. Linux draws the same boundary with a larger copy
routine: the register contains a virtual address in the caller's `mm`, and
the kernel copies bytes out under that translation before it trusts them.
The page tables enforce the boundary on every user load. The copy enforces
it at the moment the kernel chooses to read.

```text
while this task is current
──────────────────────────
core registers  ←→  pt_regs on this task's kernel stack, and thread
page-table root ←→  active_mm->pgd
user virtual address 0x7fff…  walks that root
kernel object (files, cred, task)  lives outside that user map
```

Two ordinary processes therefore have two roots. Day 2 will show that the
roots can still point at the same physical frame until one of them writes.

---

## 9. `getpid` returns the thread-group id in a namespace

In `kernel/sys.c`, `SYSCALL_DEFINE0(getpid)` and `SYSCALL_DEFINE0(gettid)`,
same tree, lines 958–976:

```c
/**
 * sys_getpid - return the thread group id of the current process
 *
 * Note, despite the name, this returns the tgid not the pid.  The tgid and
 * the pid are identical unless CLONE_THREAD was specified on clone() in
 * which case the tgid is the same in all threads of the same group.
 *
 * This is SMP safe as current->tgid does not change.
 */
SYSCALL_DEFINE0(getpid)
{
	return task_tgid_vnr(current);
}

/* Thread ID - the internal kernel "pid" */
SYSCALL_DEFINE0(gettid)
{
	return task_pid_vnr(current);
}
```

`task_tgid_vnr` in `include/linux/pid.h` (lines 292–294) calls
`__task_pid_nr_ns(tsk, PIDTYPE_TGID, NULL)`. The `NULL` namespace argument
means the caller's current PID namespace. The number is a view of a
`struct pid` as seen from the caller's namespace. `gettid` uses `PIDTYPE_PID` and returns
the per-task id. The comment states the rule this tree implements: `tgid`
and `pid` match unless the task was created with `CLONE_THREAD`, in which
case every task in the group shares one `tgid` and each has its own `pid`.

ToyOS `SYS_GETPID` writes `init_process.pid` into the saved `RAX`. There is
one namespace, the number 1, and one task. Linux inserts a namespace lookup
between the task and the integer a program sees. A container and its host
can report different integers for one `struct pid`. After `usage` drops and
the pid is freed, the namespace may assign that integer to a later task. A
lone integer in a log is evidence together with the namespace and the time
of the observation.

`/proc/self` is that lookup from the reading task. The lab opens it, and
opens `/proc/<getpid()>/status`, and both paths name the same task for a
single-threaded process because `getpid` returned `tgid` and `/proc/self`
resolves the reader.

---

## 10. Descriptors, credentials, and lifetime are referenced objects

A descriptor number is an index in the `fdtable` hung off `files_struct`.
In `include/linux/fdtable.h` the table has a reference count, a pointer to
the current `fdtable`, a `close_on_exec` bitmap, and an array of `struct
file *`. In `include/linux/fs.h`, `struct file` carries `f_count`, `f_pos`,
`f_flags`, `f_mode`, and `f_path`. Three different facts travel under the
word "file":

```text
int fd                         index in this task's fdtable
fdtable.fd[fd]                 struct file *   (the open-file description)
struct file.f_pos              shared offset, with f_count references
struct file.f_path             which inode, pipe, or socket
fdtable.close_on_exec bit      belongs to this index, in this table
```

Day 2's ordinary fork copies the table and takes a new reference on each
`struct file`, so the offset is shared and the index is private. Day 3's
`FD_CLOEXEC` closes one index at image replacement because the bit lives in
the table, beside the index. Closing an index drops one `f_count`. The
description lives as long as some index, some kernel path, or some other
task still holds it. Week 9's ToyOS file table is one array inside one
kernel; Linux makes the array an object tasks can share or duplicate.

Credentials work the same way. `real_cred` and `cred` are pointers. Changing
uids replaces the pointer. The previous object stays until its last user
releases it. `/proc/PID/status` prints a projection of the ids. The
authorization check reads the object.

`exit_state` and `usage` are the lifetime pair on the task itself. The task
can be finished as an execution stream and still occupy a pid while `usage`
keeps the object alive for the parent. That is the structural reason a
process is a kernel object with a life cycle, and an ELF file is an input
that many such objects can name.

---

## 11. Execution trace: `proc_lab` reads its own task

The program in
[`projects/linux-system-programming/src/proc_lab.c`](../../projects/linux-system-programming/src/proc_lab.c)
calls `getpid`, opens `/proc/<that>/status`, prints the `Pid:` and `PPid:`
lines, prints the first line of `/proc/self/stat`, and `readlink`s
`/proc/self/exe`. The following trace is one run of that program after its
image is already installed. Numeric addresses and ids below are a single
illustrated run. The wrapper prints the real ones.

```text
shell task                         proc_lab task
RIP in the shell's text            RIP in proc_lab / libc text
mm_shell->pgd installed            mm_lab->pgd installed when it runs
files: shell's descriptors         files: lab's descriptors
pid/tgid: shell's ids              pid == tgid: this process id
real_parent: shell's parent        real_parent: the shell (or its child
                                   if the shell forked first)
```

1. The scheduler makes the `proc_lab` task `current`. The core's
   page-table root register receives `active_mm->pgd`, which is `mm->pgd`
   for this user task. User `RIP` is restored from the task's saved frame
   to an instruction in the mapped image. That image was built from the
   ELF `PT_LOAD` segments. The file's `e_entry` was an input to that
   mapping. The live `RIP` is a register.

2. `main` calls `getpid`. Libc places the syscall number in the ABI
   register and crosses the privilege boundary. On this x86-64 host the
   usual instruction is `syscall`. ToyOS used `int $0x80` and a DPL-3
   interrupt gate; both paths arrive in the kernel with the user registers
   saved on the task's kernel stack and with `current` equal to this
   `task_struct`. The handler is `sys_getpid`. It returns
   `task_tgid_vnr(current)`. For this single-threaded task, `pid` and
   `tgid` hold the same number, so the integer matches the per-task id as
   well. The return path writes that integer into the user return register
   and resumes user `RIP` at the instruction after the syscall. ToyOS did
   the same write to `frame->rax` for `SYS_GETPID`.

3. `snprintf` builds `/proc/<tgid>/status`. `fopen` allocates a descriptor
   index in this task's `files`, pointing at a new `struct file` whose
   `f_path` is that proc inode. The reads that follow run in kernel mode,
   generating text from this task: the `Pid:` line from the thread-group
   id, the `PPid:` line from the parent. Those strings are copied back to
   the user buffer. The user buffer's address is a virtual address in
   `mm_lab`. The copy is the Linux form of Week 8's checked user copy.

4. `fopen("/proc/self/stat")` resolves `self` as the reading task, the same
   `current`. The first line includes the `comm` field stored on the task
   and the task state. `readlink("/proc/self/exe")` returns the path of the
   file installed as this `mm`'s executable. That file is the ELF program.
   The task holding `mm` is the process. The link can name the same inode
   for every run. The task, the `mm`, and the `pgd` are new each run.

5. A second start of the same path repeats the construction: another
   `task_struct`, another `mm`, another `pgd`, another `files`, another
   pair of credential references, another `pid` lookup in the namespace.
   Both tasks can be runnable. Each has its own saved `RIP`. A store by
   one walks only the root installed while it is current. The ELF bytes
   on disk stay one file. If the mappings are private, Day 2's
   copy-on-write decides whether a write allocates a new frame. The two
   processes already exist before that question comes up, because they
   already have two tasks.

The wrapper
[`challenges/day-001-process-model.sh`](../challenges/day-001-process-model.sh)
builds this program, prints ELF class, type, machine, and entry, and runs
it. The stable line is `proc_inspection=ok`. The PID, PPID, and executable
path are observations of one task on one host.

---

## 12. Architecture lens — x86-64, AArch64, and RISC-V

The graph in `task_struct` is C. `mm`, `files`, `fs`, `real_cred`,
`signal`, `pid`, and `tgid` are the same members on every architecture this
tree builds. What the ISA changes is where a resume register and a
page-table root live in the core, and which instruction enters the kernel.

```text
                user resume          page-table root        syscall entry
x86-64          RIP, in pt_regs.ip   CR3 ← mm->pgd          syscall
AArch64         PC                   TTBR0_EL1 ← mm->pgd    svc
RISC-V          PC                   satp ← mm->pgd         ecall
```

On x86-64, `thread_struct` is the variable-sized tail the comment in
`sched.h` warns about, and the user general-purpose registers at a
privilege crossing sit in `pt_regs` on the kernel stack. `iretq` in ToyOS
is the teaching form of "restore RIP, CS, RFLAGS, RSP, SS from a frame."
Linux's `syscall` return path restores the same user-visible contract
through a different entry instruction. AArch64 and RISC-V store their
register frames in their own `pt_regs` layouts and install their own
page-table root register from the same `mm->pgd` pointer. The fault and
syscall entry stubs differ. Once `current` is set, `sys_getpid` runs the
same C: it reads `tgid` in the caller's PID namespace.

The ELF `e_machine` field selects which of those machines the file is for.
`readelf -h` on the lab binary prints that machine. It prints an entry
address in the file's image. The live `RIP` is a register of the running
task.

---

## 13. Mastery: Explain · Draw · Observe · Build

The lab is evidence after this model. From the week directory, run
[`challenges/day-001-process-model.sh`](../challenges/day-001-process-model.sh).
It builds
[`projects/linux-system-programming`](../../projects/linux-system-programming/)
and runs `proc_lab` on itself.

### Explain

- A program is an ELF file: program headers, `e_entry`, and bytes. A
  process is a `task_struct` plus `mm`, `files`, `fs`, `real_cred` /
  `cred`, `signal`, and a pid in a namespace.
- `getpid` returns `task_tgid_vnr(current)`. For a single-threaded task,
  `pid` and `tgid` hold the same number. `gettid` returns the per-task id.
- Registers live with the task (`thread`, `pt_regs` on `stack`). Page
  tables live in `mm->pgd`. Week 2's `RIP` and Week 5's walk meet in those
  two places.
- ToyOS `struct process` is the small version: one root, one entry, one
  kernel stack, one PID, entered by `iretq`. Linux adds sharing,
  credential objects, namespaces, and a task lifetime separate from the
  ELF file.
- Starting one ELF twice allocates two tasks. The inode of the executable
  can be shared. Each run has its own page-table root and its own register
  save area.

### Draw

1. The `task_struct` graph from section 6, with an arrow from `mm` to
   `pgd` and an arrow from `files` to `struct file`.
2. One core, with `RIP` and the page-table root register, and the two
   kernel objects those registers are loaded from.
3. Two runs of `proc_lab`: two tasks, two `mm`s, one ELF inode.

### Observe

The wrapper prints `uname`, the ELF header (class, type, machine, entry),
`Pid:` and `PPid:` from status, the `stat` line, `exe=`, and
`proc_inspection=ok`. Record which of those come from the file (ELF
header, hash) and which come from the live task (pid, ppid, exe link,
stat). Run it twice and mark the fields that changed. The script's evidence
is the calling process: status, stat, exe, and the ELF header.

### Build

In the week-10 project, extend the observation only through its existing
interfaces: predict `Pid` versus `PPid` before the run, then point at the
`task_struct` fields that supply each printed line. Keep the experiment on
the calling process. The wrapper's LIMIT line is the boundary of the evidence:
the calling process, its own pid, and the fields `proc_lab` prints.

### Why? notebook

1. Why can the ELF file stay unmodified while two processes run it?
2. Why does `mm` exist separately from `task_struct`?
3. Why does `getpid` read `tgid`?
4. Why is a PID an integer only after a namespace lookup?
5. Why does the kernel stack belong to the task, next to the user
   register frame?
6. Why is `close_on_exec` a bit in the descriptor table, beside `f_pos`
   living in `struct file`?
7. Why can `exit_state` be meaningful after user mappings are gone?
8. Why did ToyOS need `copy_from_user` once the page-table root became
   per process?
