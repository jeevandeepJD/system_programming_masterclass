# Day 3 — Exec Replaces the Image and Keeps the Task

**Target time:** approximately 3 hours

> A child has just called `execve("/bin/sh", ...)`. The call succeeds. Which
> `task_struct` is running, which `mm` is installed, and which instruction
> does `RIP` name?

Day 1 made a process a task plus referenced objects, and a program an ELF
file. Day 2 showed how `fork` allocates a second task and a second `mm`,
write-protecting private frames so the copy stays cheap. The usual reason
that copy stays cheap is this call: the child exists only to become a
different program. `execve` replaces the user image of the calling task.
The pid remains the pid of that task, for the single-threaded case this
week runs. The page tables, the user registers, the argument strings, and
the dispositions of caught signals are rebuilt from the new ELF file.

## 1. Syntax needed for this source

An `a.out` file begins with a `struct exec`. `ex.a_entry` reads one
member of that header. `eip` in `do_execve` is a pointer to the saved
interrupt frame, so `eip[0]` and `eip[3]` are slots in that array: the
saved instruction pointer and the saved stack pointer. Assigning them
changes where the return from the syscall will resume. The function
returns `0` after that store; the new program runs because the frame
changed.

Sketch:

```c
struct exec ex;
ex = *((struct exec *) header);
eip[0] = ex.a_entry;
eip[3] = stack_top;
return 0;
```

## 2. Linux 0.12 first: the `a.out` header and `do_execve`

`exec` answers a smaller question than `fork`: same task, new image.
Linux 0.12 supports one format, `a.out`. `do_execve` in `fs/exec.c`
reads the header from the first block, checks it, maps the text, and
writes the saved frame so the return enters the new program. The pid
stays on `current`. Printed pages are from the 0.12 commentary; the
[0.12 map](../../references/EARLY_LINUX_012_MAP.md) points at chapter
12.15.

`include/a.out.h`, as quoted with `fs/exec.c`, printed page 826:

```c
struct exec {
   unsigned long a_magic;       /* Use macros N_MAGIC, etc for access */
   unsigned a_text;             /* length of text, in bytes */
   unsigned a_data;             /* length of data, in bytes */
   unsigned a_bss;              /* length of uninitialized data area for file, in bytes */
   unsigned a_syms;             /* length of symbol table data in file, in bytes */
   unsigned a_entry;            /* start address */
   unsigned a_trsize;           /* length of relocation info for text, in bytes */
   unsigned a_drsize;           /* length of relocation info for data, in bytes */
};
```

`do_execve` copies that header with
`ex = *((struct exec *) bh->b_data)` (printed page 840). It rejects the
file unless `N_MAGIC(ex) == ZMAGIC` and the relocation sizes are zero
(printed page 842). On success, printed page 844:

```c
eip[0] = ex.a_entry;               /* eip, magic happens :-) */
eip[3] = p;                        /* stack pointer */
return 0;
```

Trace: load the header, keep this `task_struct`, replace the saved
`eip` and stack pointer. The syscall return then begins at `a_entry`.

What carries forward is image replacement on the calling task, with the
pid left in place. What changes in 5.10 is the format and the safety
around it: ELF, `linux_binprm`, `PT_INTERP`, the auxiliary vector,
credentials, and close-on-exec in `fs/exec.c` and `fs/binfmt_elf.c`.
Develop against those. `a.out`, `ZMAGIC`, and a direct store into the
80386 frame are the 0.12 loader.

ToyOS `process_init` maps one ELF and `process_start` enters it with
`iretq` ([`process.c`](../../projects/toyos/src/process.c)). 0.12 runs
that replacement against a task that already exists.

Now move to the 5.10 implementation.

The excerpts are from the local tree
`5.10.134-15_rc1-10333-gc9b2385d6793` (commit `c9b2385d6793`). It is a
5.10-based tree. Names and line placement on current mainline differ.

---

## 3. Replacement is a property of the calling task

`fork` answers "who is the new task?" `execve` answers "what user image
does this task run next?" Shells need both because a shell must remain the
shell after starting a command. The sequence is the Day 2 child — its own
`task_struct`, its own pid, a duplicated `mm` — followed by this call,
which frees that duplicated `mm` and installs a new one. The parent's task
is untouched. The child's task continues, with a new memory descriptor and
a new user `RIP`.

ToyOS `process_init` loads one ELF and `process_start` enters it with
`iretq`. Linux runs those same steps — check the header, map each
`PT_LOAD`, build a user stack, enter ring 3 — against an existing task.
`mmput` drops the old root, `activate_mm` installs the new one, and `pid`
and `tgid` stay on that `task_struct` when the thread group has one member.

Before `bprm->point_of_no_return`, a failed `execve` returns a negative
errno to the old image and libc reports `-1`. After `begin_new_exec` sets
that flag, `exec_mmap` has installed the new `mm`, and a later failure
ends the task with a fatal signal.

---

## 4. The system call builds a `linux_binprm` and hands it to the format

In `fs/exec.c`, `SYSCALL_DEFINE3(execve)`, same tree, and the `execveat`
sibling that shares `do_execveat`:

```c
SYSCALL_DEFINE3(execve,
		const char __user *, filename,
		const char __user *const __user *, argv,
		const char __user *const __user *, envp)
{
	return do_execve(getname(filename), argv, envp);
}

SYSCALL_DEFINE5(execveat,
		int, fd, const char __user *, filename,
		const char __user *const __user *, argv,
		const char __user *const __user *, envp,
		int, flags)
{
	int lookup_flags = (flags & AT_EMPTY_PATH) ? LOOKUP_EMPTY : 0;

	return do_execveat(fd,
			   getname_flags(filename, lookup_flags, NULL),
			   argv, envp, flags);
}
```

`filename`, `argv`, and `envp` arrive as user pointers. They are virtual
addresses in the calling task's current `mm`. `do_execveat_common` counts
the argument and environment vectors, then `copy_strings` copies the bytes
onto the new stack image. The comment on `copy_strings` in this file says
it copies argument and environment strings from the old process's memory
to the new process's stack. The copy itself uses `copy_from_user`. That is
Week 8's rule on a larger buffer: `copy_from_user` translates the user
virtual address through the caller's page tables and writes the bytes into
pages that will become the new user stack.

`bprm_execve` is the body that opens the file and runs the handler. In
`fs/exec.c`, `bprm_execve`, same tree:

```c
static int bprm_execve(struct linux_binprm *bprm,
		       int fd, struct filename *filename, int flags)
{
	struct file *file;
	int retval;

	/*
	 * Cancel any io_uring activity across execve
	 */
	io_uring_task_cancel();

	retval = prepare_bprm_creds(bprm);
	if (retval)
		return retval;

	check_unsafe_exec(bprm);
	current->in_execve = 1;

	file = do_open_execat(fd, filename, flags);
	retval = PTR_ERR(file);
	if (IS_ERR(file))
		goto out_unmark;

	sched_exec();

	bprm->file = file;
```

`prepare_bprm_creds` builds the credential object the new image will
commit. `do_open_execat` opens the executable and stores the `struct file`
on `bprm`. `exec_binprm`, which this function calls next, records
`old_pid = current->pid` before it searches for a binary handler, then
calls `search_binary_handler`. The comment there says the pid is fetched
before `load_binary` changes it. For a single-threaded caller,
`de_thread` finds `thread_group_empty` and leaves the task as its own
leader. When the caller is a non-leader thread, `de_thread` calls
`exchange_tids` and `transfer_pid` so the surviving task takes the previous
group's TGID. The
process id `getpid` reports — the thread-group id — is the identity that
continues. This week's labs are single-threaded, so the same `task_struct`
keeps the same `pid` and `tgid` fields while its `mm` is replaced.

On success, `bprm_execve` clears `current->in_execve` and returns. The
user instruction that resumes is in the new image. The syscall wrapper in
the old image receives a return value only on the failure path.

---

## 5. `begin_new_exec` is the point of no return

The ELF handler, once it has parsed headers and built a new `mm`, calls
`begin_new_exec`. In `fs/exec.c`, `begin_new_exec`, same tree:

```c
int begin_new_exec(struct linux_binprm * bprm)
{
	struct task_struct *me = current;
	int retval;

	/* Once we are committed compute the creds */
	retval = bprm_creds_from_file(bprm);
	if (retval)
		return retval;

	/*
	 * Ensure all future errors are fatal.
	 */
	bprm->point_of_no_return = true;

	/*
	 * Make this the only thread in the thread group.
	 */
	retval = de_thread(me);
	if (retval)
		goto out;

	/* Ensure the files table is not shared. */
	retval = unshare_files();
	if (retval)
		goto out;

	/*
	 * Must be called _before_ exec_mmap() as bprm->mm is
	 * not visibile until then. This also enables the update
	 * to be lockless.
	 */
	set_mm_exe_file(bprm->mm, bprm->file);
```

`point_of_no_return` is why a later mapping error kills the task.
`de_thread` makes this task the sole member of the thread group, as
section 4 described. `unshare_files` gives the task a private
`files_struct` if it had been sharing one under `CLONE_FILES`, so the
closes that follow affect this task's table. `set_mm_exe_file` installs
the executable `struct file` on the new `mm`. That is the object
`/proc/self/exe` will name after the call. Day 1's `proc_lab` reads it
back.

`exec_mmap` then publishes the new address space. In `fs/exec.c`,
`exec_mmap`, same tree:

```c
	task_lock(tsk);
	membarrier_exec_mmap(mm);

	local_irq_disable();
	active_mm = tsk->active_mm;
	tsk->active_mm = mm;
	tsk->mm = mm;
	/*
	 * This prevents preemption while active_mm is being loaded and
	 * it and mm are being updated, which could cause problems for
	 * lazy tlb mm refcounting when these are updated by context
	 * switches. Not all architectures can handle irqs off over
	 * activate_mm yet.
	 */
	if (!IS_ENABLED(CONFIG_ARCH_WANT_IRQS_OFF_ACTIVATE_MM))
		local_irq_enable();
	activate_mm(active_mm, mm);
```

`tsk->mm` and `tsk->active_mm` swing to the new descriptor, and
`activate_mm` loads its `pgd` into the core's page-table root. The old
`mm` is released with `mmput` on the success path just below this excerpt.
The task pointer, the pid, and the kernel stack of this syscall stay. The
user translations change underneath the still-running kernel path. Week 5's
walk, from the next user instruction on, starts at the new root. ToyOS
`address_space_activate` is the one-process form of `activate_mm`.

Still in `begin_new_exec`, after the old mappings are released, the
function makes the signal table private, drops thread state, and applies
close-on-exec:

```c
	/*
	 * Make the signal table private.
	 */
	retval = unshare_sighand(me);
	if (retval)
		goto out_unlock;

	/*
	 * Ensure that the uaccess routines can actually operate on userspace
	 * pointers:
	 */
	force_uaccess_begin();

	me->flags &= ~(PF_RANDOMIZE | PF_FORKNOEXEC | PF_KTHREAD |
					PF_NOFREEZE | PF_NO_SETAFFINITY);
	flush_thread();
	me->personality &= ~bprm->per_clear;

	/*
	 * We have to apply CLOEXEC before we change whether the process is
	 * dumpable (in setup_new_exec) to avoid a race with a process in userspace
	 * trying to access the should-be-closed file descriptors of a process
	 * undergoing exec(2).
	 */
	do_close_on_exec(me->files);
```

`flush_thread` clears architecture-specific user thread state. The
`PF_FORKNOEXEC` flag coming off is the kernel's record that this task has
now exec'd. `do_close_on_exec` runs before the image is entered, so the new
program's descriptor table already lacks the close-on-exec indices.

The disposition update and the credential commit are the next committed
steps. Same function:

```c
	__set_task_comm(me, kbasename(bprm->filename), true);

	/* An exec changes our domain. We are no longer part of the thread
	   group */
	WRITE_ONCE(me->self_exec_id, me->self_exec_id + 1);
	flush_signal_handlers(me, 0);

	/*
	 * install the new credentials for this executable
	 */
	security_bprm_committing_creds(bprm);

	commit_creds(bprm->cred);
```

`__set_task_comm` stores the new short name on the task. Day 1's `comm`
field is that array. `flush_signal_handlers(me, 0)` rewrites dispositions.
`commit_creds` installs the prepared `struct cred` into the task's
credential pointers. The task object is the same pointer `current` was at
the start of the syscall.

---

## 6. Caught handlers become `SIG_DFL`; `SIG_IGN` stays

In `kernel/signal.c`, `flush_signal_handlers`, same tree:

```c
void
flush_signal_handlers(struct task_struct *t, int force_default)
{
	int i;
	struct k_sigaction *ka = &t->sighand->action[0];
	for (i = _NSIG ; i != 0 ; i--) {
		if (force_default || ka->sa.sa_handler != SIG_IGN)
			ka->sa.sa_handler = SIG_DFL;
		ka->sa.sa_flags = 0;
#ifdef __ARCH_HAS_SA_RESTORER
		ka->sa.sa_restorer = NULL;
#endif
		sigemptyset(&ka->sa.sa_mask);
		ka++;
	}
}
```

`begin_new_exec` passes `force_default` as 0. The handler is assigned
`SIG_DFL` when the current handler is anything other than `SIG_IGN`. A slot
that already holds `SIG_IGN` keeps `SIG_IGN`. A caught handler — a user
function pointer — becomes `SIG_DFL`. A slot that was already `SIG_DFL`
stays `SIG_DFL`. Every slot, including the ignored ones, has `sa_flags`
cleared, the restorer cleared on architectures that have one, and
`sa_mask` emptied. That is the disposition rule this tree implements at
exec. The application contract for the rest of signal state across exec is
POSIX; the function that performs the replacement of the disposition table
is this one, called from `begin_new_exec` after `unshare_sighand`.

The new program therefore starts with default action for every signal the
old program had caught, and with ignore still installed for every signal
the old program had set to `SIG_IGN`. Installing a handler is the new
image's job, after it begins running.

---

## 7. `FD_CLOEXEC` closes one descriptor index

Day 2's `dup_fd` copied the `close_on_exec` bitmap into the child's private
`files_struct`. The bit lives beside the descriptor index. `struct file`
holds `f_pos` and `f_count`. The close-on-exec bit lives in the fdtable.

In `fs/file.c`, `do_close_on_exec`, same tree:

```c
void do_close_on_exec(struct files_struct *files)
{
	unsigned i;
	struct fdtable *fdt;

	/* exec unshares first */
	spin_lock(&files->file_lock);
	for (i = 0; ; i++) {
		unsigned long set;
		unsigned fd = i * BITS_PER_LONG;
		fdt = files_fdtable(files);
		if (fd >= fdt->max_fds)
			break;
		set = fdt->close_on_exec[i];
		if (!set)
			continue;
		fdt->close_on_exec[i] = 0;
		for ( ; set ; fd++, set >>= 1) {
			struct file *file;
			if (!(set & 1))
				continue;
			file = fdt->fd[fd];
			if (!file)
				continue;
			rcu_assign_pointer(fdt->fd[fd], NULL);
			__put_unused_fd(files, fd);
			spin_unlock(&files->file_lock);
			filp_close(file, files);
			cond_resched();
			spin_lock(&files->file_lock);
		}
```

The comment matches `begin_new_exec`: exec unshares the file table first.
The loop walks the bitmap, clears it, and `filp_close`s each marked
descriptor. `filp_close` drops that reference on the `struct file`. If the
parent, or another task, still holds a reference from its own table, the
open-file description remains, with its `f_pos`. The new image's index is
gone. Descriptors whose close-on-exec bit is clear stay in the table, with
the same `struct file` pointer the child inherited at `fork`. That is how
a shell passes a pipe end to a command, and how it keeps a password file
from being passed: the bit, set on the index before `exec`, makes
`do_close_on_exec` drop it.

Standard descriptors 0, 1, and 2 usually have the bit clear, so the new
image inherits the terminal or the pipe the shell arranged. The offset and
status flags are the shared description from Day 2. The index numbers match
because `dup_fd` copied them and this loop left them in place.

---

## 8. The ELF handler maps `PT_LOAD` and chooses an entry

Week 3's program headers are what `load_elf_binary` reads. In
`fs/binfmt_elf.c`, `load_elf_binary`, same tree, after the locals that name
`elf_entry`, `e_entry`, and the program-header cursor, the handler checks
the image and scans for `PT_INTERP`:

```c
	retval = -ENOEXEC;
	/* First of all, some simple consistency checks */
	if (memcmp(elf_ex->e_ident, ELFMAG, SELFMAG) != 0)
		goto out;

	if (elf_ex->e_type != ET_EXEC && elf_ex->e_type != ET_DYN)
		goto out;
	if (!elf_check_arch(elf_ex))
		goto out;
	if (elf_check_fdpic(elf_ex))
		goto out;
	if (!bprm->file->f_op->mmap)
		goto out;

	elf_phdata = load_elf_phdrs(elf_ex, bprm->file);
	if (!elf_phdata)
		goto out;

	elf_ppnt = elf_phdata;
	for (i = 0; i < elf_ex->e_phnum; i++, elf_ppnt++) {
		char *elf_interpreter;

		if (elf_ppnt->p_type == PT_GNU_PROPERTY) {
			elf_property_phdata = elf_ppnt;
			continue;
		}

		if (elf_ppnt->p_type != PT_INTERP)
			continue;
```

`ELFMAG` is the four magic bytes ToyOS checks as `ELF_MAGIC`. `ET_EXEC`
and `ET_DYN` are the types this function maps. `elf_check_arch` requires
`e_machine` to match this kernel. ToyOS hard-codes machine 62. When the
loop finds `PT_INTERP`, it reads that pathname and opens the interpreter.
It then walks `PT_LOAD` headers. `make_prot` turns `PF_R`, `PF_W`, and
`PF_X` into `PROT_READ`, `PROT_WRITE`, and `PROT_EXEC`. Each loadable
segment is mapped with `elf_map` at `p_vaddr` plus a load bias. For
`ET_DYN` the bias is the randomized placement of the image. For a fixed
`ET_EXEC` the bias is zero and the map is `MAP_FIXED`. Where `p_memsz`
exceeds `p_filesz`, `set_brk` and `padzero` cover the zero-filled tail.
That is Week 3's `.bss` rule, performed with anonymous pages.

After the executable's segments are mapped, the handler selects the user
entry. Same function:

```c
	if (interpreter) {
		elf_entry = load_elf_interp(interp_elf_ex,
					    interpreter,
					    load_bias, interp_elf_phdata,
					    &arch_state);
		if (!IS_ERR((void *)elf_entry)) {
			/*
			 * load_elf_interp() returns relocation
			 * adjustment
			 */
			interp_load_addr = elf_entry;
			elf_entry += interp_elf_ex->e_entry;
		}
		if (BAD_ADDR(elf_entry)) {
			retval = IS_ERR((void *)elf_entry) ?
					(int)elf_entry : -EINVAL;
			goto out_free_dentry;
		}
		reloc_func_desc = interp_load_addr;

		exe_file_allow_write_access(interpreter);
		fput(interpreter);

		kfree(interp_elf_ex);
		kfree(interp_elf_phdata);
	} else {
		elf_entry = e_entry;
		if (BAD_ADDR(elf_entry)) {
			retval = -EINVAL;
			goto out_free_dentry;
		}
	}
```

`e_entry` was set to `elf_ex->e_entry + load_bias`, the executable's own
entry in the placed image. If `PT_INTERP` was present, `elf_entry` becomes
the interpreter's placed entry, and the executable's entry is preserved
for the auxiliary vector. The kernel will enter user mode at `elf_entry`. The dynamic linker, running in user mode, is what eventually
transfers to the executable's entry. Week 3 named those stages. This
function is the kernel half: it maps both ELFs and chooses which entry
register to load.

With no interpreter, `elf_entry` is the executable's `e_entry` plus bias,
which is the ToyOS path: map `PT_LOAD`, jump to the header entry.

---

## 9. The initial stack carries `argc`, `argv`, `envp`, and auxv

`copy_strings` has already placed the argument and environment bytes at the
high end of the new stack, walking downward. `create_elf_tables` builds the
pointer vector the ABI requires below them. In `fs/binfmt_elf.c`,
`create_elf_tables`, same tree, the auxiliary vector is assembled in
`mm->saved_auxv` and then copied to user memory:

```c
	NEW_AUX_ENT(AT_HWCAP, ELF_HWCAP);
	NEW_AUX_ENT(AT_PAGESZ, ELF_EXEC_PAGESIZE);
	NEW_AUX_ENT(AT_CLKTCK, CLOCKS_PER_SEC);
	NEW_AUX_ENT(AT_PHDR, phdr_addr);
	NEW_AUX_ENT(AT_PHENT, sizeof(struct elf_phdr));
	NEW_AUX_ENT(AT_PHNUM, exec->e_phnum);
	NEW_AUX_ENT(AT_BASE, interp_load_addr);
	NEW_AUX_ENT(AT_FLAGS, 0);
	NEW_AUX_ENT(AT_ENTRY, e_entry);
	NEW_AUX_ENT(AT_UID, from_kuid_munged(cred->user_ns, cred->uid));
	NEW_AUX_ENT(AT_EUID, from_kuid_munged(cred->user_ns, cred->euid));
	NEW_AUX_ENT(AT_GID, from_kgid_munged(cred->user_ns, cred->gid));
	NEW_AUX_ENT(AT_EGID, from_kgid_munged(cred->user_ns, cred->egid));
	NEW_AUX_ENT(AT_SECURE, bprm->secureexec);
	NEW_AUX_ENT(AT_RANDOM, (elf_addr_t)(unsigned long)u_rand_bytes);
#ifdef ELF_HWCAP2
	NEW_AUX_ENT(AT_HWCAP2, ELF_HWCAP2);
#endif
	NEW_AUX_ENT(AT_EXECFN, bprm->exec);
```

`AT_ENTRY` is the executable's `e_entry` plus load bias, even when the
first user instruction is the interpreter. `AT_BASE` is the interpreter's
load address, zero when there is no interpreter. `AT_PHDR` points at the
program headers mapped into the new image. `AT_PAGESZ` is the page size the
Week 5 walk uses. `AT_UID` and the other ids are taken from the credential
object `commit_creds` is installing. `AT_RANDOM` is sixteen bytes the
kernel generated for the user stack. Further entries in the same function,
when the architecture or the call provides them, add `AT_PLATFORM`,
`AT_BASE_PLATFORM`, and `AT_EXECFD`. The vector ends with `AT_NULL`.

The word vector sits at the low end of this stack block. `put_user(argc, sp++)`
stores the count. A loop then `put_user`s each `argv` pointer, using
`strnlen_user` to advance to the next string, and stores a zero word.
The same pattern follows for `envp`. `copy_to_user` writes the auxiliary
vector from `mm->saved_auxv`. `mm->start_stack` becomes `bprm->p`, the
stack pointer the entry code will see.

```text
high addresses
┌──────────────────────────────────────┐
│ argument and environment strings     │  copied by copy_strings
│ AT_RANDOM bytes, platform string     │
├──────────────────────────────────────┤
│ auxv: AT_* key, value, …, AT_NULL    │
│ envp pointers, then 0                │
│ argv pointers, then 0                │
│ argc                                 │  ← user stack pointer at entry
low addresses
```

This is the stack Week 3's `_start` consumes. The kernel built it. The
dynamic linker, libc startup, and `main` are still ahead in user mode.
`RIP` at the first user instruction names `elf_entry`. The stack pointer
names `argc`.

At the end of `load_elf_binary` the handler takes `current_pt_regs()`, the
saved user frame on this task's kernel stack, optionally runs
`ELF_PLAT_INIT` for ABIs that seed extra registers, calls `finalize_exec`,
and calls `start_thread(regs, elf_entry, bprm->p)`.

On x86-64, `start_thread` in `arch/x86/kernel/process_64.c` passes
`__USER_CS` and `__USER_DS` to `start_thread_common`. Same tree:

```c
	if (static_cpu_has(X86_BUG_NULL_SEG)) {
		/* Loading zero below won't clear the base. */
		loadsegment(fs, __USER_DS);
		load_gs_index(__USER_DS);
	}

	loadsegment(fs, 0);
	loadsegment(es, _ds);
	loadsegment(ds, _ds);
	load_gs_index(0);

	regs->ip		= new_ip;
	regs->sp		= new_sp;
	regs->cs		= _cs;
	regs->ss		= _ss;
	regs->flags		= X86_EFLAGS_IF;
}

void
start_thread(struct pt_regs *regs, unsigned long new_ip, unsigned long new_sp)
{
	start_thread_common(regs, new_ip, new_sp,
			    __USER_CS, __USER_DS, 0);
}
```

The return-to-user path loads `RIP` from `regs->ip` and `RSP` from
`regs->sp`, with a user code segment and interrupts unmasked. ToyOS writes
the same architectural values into an `iretq` frame and executes `iretq`.
The task that returns is the task that entered `execve`.

---

## 10. Execution trace: the status lab's child becomes `/bin/sh`

`mode_status` in
[`projects/linux-system-programming/src/process_lab.c`](../../projects/linux-system-programming/src/process_lab.c)
forks, and the child calls `execl("/bin/sh", "sh", "-c", "exit 7", NULL)`.
On failure it `_exit(127)`. The parent `waitpid`s and requires a normal
exit of status 7. Illustrated ids below are one run. The wrapper prints the
real child pid.

1. `fork` does Day 2's work. The child task exists, `exit_signal` is
   `SIGCHLD`, and `mm_fork` is a write-protected duplicate of the lab
   program. `execl` is libc packing `argv` and calling `execve`.

2. `SYSCALL_DEFINE3(execve)` runs in task C. `copy_strings` pulls
   `"sh"`, `"-c"`, and `"exit 7"` from C's user memory with
   `copy_from_user`. `bprm_execve` opens `/bin/sh` and sets
   `current->in_execve`.

3. `load_elf_binary` checks `ELFMAG`, accepts `ET_DYN` or `ET_EXEC`, and
   checks `e_machine`. `/bin/sh` on a typical Linux host is a dynamic ELF,
   so the `PT_INTERP` loop opens the dynamic linker. `PT_LOAD` segments of
   both files are mapped into a new `mm`. `e_entry` in the auxv will name
   the shell's entry. `elf_entry` names the interpreter's entry.

4. `begin_new_exec` sets the point of no return, `de_thread` sees an empty
   thread group, `exec_mmap` assigns `C->mm = C->active_mm =` the new mm
   and `activate_mm` loads the new `pgd`. `mmput` drops `mm_fork`. The COW
   frames unique to the child become free when their last reference goes.
   The parent's `mm_p` is a different object and stays. `do_close_on_exec`
   closes any index whose bitmap bit is set. The child's standard
   descriptors, bit clear, remain. `flush_signal_handlers(C, 0)` sets
   caught handlers to `SIG_DFL` and leaves `SIG_IGN` in place.
   `commit_creds` installs the credentials prepared for this file.
   `set_mm_exe_file` makes `/proc/C/exe` name `/bin/sh`.

5. `create_elf_tables` writes `argc == 3`, the three `argv` pointers, a
   zero, the environment pointers, and the auxv including `AT_ENTRY` and
   `AT_BASE`. `start_thread` sets the child's user `RIP` to the
   interpreter and the user `RSP` to `argc`. The syscall returns to user
   mode. Task C's pid is still C. Task P is still blocked in `waitpid`.

6. The dynamic linker, in user mode, maps the shell's libraries, applies
   relocations, and transfers to the shell's entry. Libc calls the shell's
   `main`. The shell parses `-c` and runs `exit 7`, which is a user-space
   `exit` and then an exit-group syscall. The kernel records status 7 on
   task C and signals the parent with `SIGCHLD` because `exit_signal` was
   set at `fork`. `waitpid` in P decodes a normal exit of 7. The printed
   line is `exec_exit=7` with the child's pid and the parent's pid.

The second child in `mode_status` stays the lab image and `pause`s until
the parent sends `SIGTERM`. That wait status is a signal termination. Image
replacement happened in the child that reached a successful `execve`.

---

## 11. Architecture lens — x86-64, AArch64, and RISC-V

`execve`, `begin_new_exec`, `load_elf_binary`, and `create_elf_tables` are
generic C. The ISA changes three things: which `e_machine` `elf_check_arch`
accepts, which register `start_thread` writes as the resume PC, and which
page-table root register `activate_mm` loads from `mm->pgd`.

On x86-64 in this tree, `start_thread_common` writes `regs->ip`,
`regs->sp`, `__USER_CS`, `__USER_DS`, and `X86_EFLAGS_IF`. On AArch64,
`start_thread` in `arch/arm64/include/asm/processor.h` writes `regs->pc`,
`regs->sp`, and `regs->pstate = PSR_MODE_EL0t` after clearing the rest of
`pt_regs`. On RISC-V, `start_thread` in `arch/riscv/kernel/process.c`
writes `regs->epc`, `regs->sp`, and `regs->status = SR_PIE`, then adds the
FPU, vector, and XLEN bits this configuration uses. `ELF_PLAT_INIT`,
invoked just before `start_thread` when the architecture defines it, seeds
any extra entry registers. The initial stack keeps the SysV shape —
`argc`, pointer vectors, auxiliary vector — with pointer width following
the ELF class. `AT_ENTRY` remains the executable entry. The first user
instruction remains `elf_entry`.

ToyOS requires `header->machine` to equal 62 (`ELF_MACHINE_X86_64`) before
it maps a segment. `readelf -h` on the lab binary prints that machine for
`process_lab`. The child that execs `/bin/sh` loads a second file, with
its own machine, `PT_INTERP`, and `e_entry`, into the same task.

---

## 12. Mastery: Explain · Draw · Observe · Build

The wrapper is evidence after the trace. From the week directory, run
[`challenges/day-003-exec-elf-inheritance.sh`](../challenges/day-003-exec-elf-inheritance.sh).
It runs `process_lab status` and prints ELF headers of that lab binary.

### Explain

- `execve` replaces the user image of the calling task. `exec_mmap` sets
  `tsk->mm` and `tsk->active_mm` to the new descriptor and activates its
  `pgd`. For a single-threaded caller, `pid` and `tgid` stay on that
  `task_struct`. A non-leader thread execs through `de_thread`, which
  `transfer_pid`s the group's TGID onto the surviving task.
- `load_elf_binary` requires ELF magic, `ET_EXEC` or `ET_DYN`, and
  `elf_check_arch`. It maps `PT_LOAD`. With `PT_INTERP`, the first user
  `RIP` is the interpreter's entry, and `AT_ENTRY` still records the
  executable's entry.
- `create_elf_tables` places `argc`, `argv`, `envp`, and the auxiliary
  vector on the new stack. `copy_strings` filled the strings with
  `copy_from_user`. `start_thread` writes the resume `RIP` and stack
  pointer into `pt_regs`.
- `do_close_on_exec` closes indices whose `close_on_exec` bit is set and
  drops those `struct file` references. Other indices keep the shared
  open-file description inherited at `fork`.
- `flush_signal_handlers(current, 0)` assigns `SIG_DFL` when the current
  handler differs from `SIG_IGN`, leaves `SIG_IGN` in place, and clears
  `sa_flags` and `sa_mask` on every slot.

### Draw

1. Task C before and after `exec_mmap`: same `task_struct`, old `mm`
   released, new `mm->pgd` installed.
2. The initial stack from section 9, with `AT_ENTRY` and `AT_BASE` labeled
   for a dynamic `/bin/sh`.
3. The child's fd table across `do_close_on_exec`: one index with the bit
   set, removed; descriptors 0, 1, and 2 still pointing at the same
   `struct file` objects as before the call.

### Observe

The program prints `exec_exit=7` with child and parent pids, then
`signal_exit=SIGTERM`. The first line is a successful exec of the shell.
The second line is the paused child terminated by `SIGTERM`. If `strace`
exists, the wrapper keeps `execve`, `wait`, `SIGTERM`, and `exit` lines.
`readelf` on `process_lab` shows class, type, machine, entry, and any
`PT_INTERP` of the program that calls `fork`. The wrapper's LIMIT line
records what this run leaves to the source trace: the initial stack, a
runtime `FD_CLOEXEC` probe, and loader relocations.

### Build

From `mode_status`, name the task that still exists after `execl` returns
successfully inside the child, and name the `mm` that `waitpid` is running
on in the parent. Then list three fields `begin_new_exec` rewrites on the
child (`mm` via `exec_mmap`, `comm`, dispositions via
`flush_signal_handlers`) and three it leaves for a single-threaded caller
(the task pointer, `pid` / `tgid`, and descriptors whose close-on-exec bit
is clear). Check the prediction against the printed pids: the parent pid
in `exec_exit=7` is the task that called `fork`, and the child pid is the
task that became the shell.

### Why? notebook

1. Why does a shell `fork` before `execve`?
2. Why is `begin_new_exec` a point of no return once `exec_mmap` has
   published the new `mm`?
3. Why is the first user instruction often the interpreter when `AT_ENTRY`
   names the executable?
4. Why does `copy_strings` use `copy_from_user`?
5. Why is the close-on-exec bit in the `fdtable` bitmap?
6. Why does `flush_signal_handlers` with `force_default == 0` leave
   `SIG_IGN` unchanged?
7. Why can `/proc/<child>/exe` name `/bin/sh` while the parent's exe still
   names `process_lab`?
8. Why does ToyOS need no `exec` syscall for its single embedded image?
