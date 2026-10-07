# Day 2 — Fork, Copy-on-Write, and Lineage

**Target time:** approximately 3 hours

- Why a new task shares frames until a store: 30 minutes
- `fork` into `kernel_clone`, and what `copy_mm` / `copy_files` share: 45 minutes
- The write-protect and the fault that copies a frame: 50 minutes
- One trace of two stores to the same virtual address: 30 minutes
- Mastery, then the wrapper as evidence: 25 minutes

> Parent and child have just returned from `fork`. Both will store through
> the same virtual address. Why does the first store fault, and why does
> that fault allocate a frame for only one of them?

Day 1 separated the ELF file from the live task. Fork is how a task acquires
a child task. The child needs its own registers, because Week 2's `RIP` and
the general-purpose registers are per execution stream. The child needs its
own page-table root, because Week 5's walk is per address space and Day 1
put that root in `mm->pgd`. Copying every physical frame at the `fork` call
would make the cost proportional to the whole address space, and a child
that immediately `exec`s (Day 3) would throw the copy away. The mechanism
that resolves this is copy-on-write: duplicate the page tables, clear the
writable permission on private writable mappings, and copy a frame on the
store that first needs a private version.

ToyOS has a single `struct process`. Week 7 switches kernel tasks that
share one set of page tables and copies only the saved registers. Linux
fork adds a second task and a second `mm`, then uses the page-table
permission bits Week 5 already taught so the second address space can
still name the same frames.

The excerpts are from the local tree
`5.10.134-15_rc1-10333-gc9b2385d6793` (commit `c9b2385d6793`), a 5.10-based
tree. Later mainline code moves these functions. The lines below are this
revision.

---

## 1. `fork` is a thin entry into `kernel_clone`

In `kernel/fork.c`, `SYSCALL_DEFINE0(fork)`, same tree. x86-64 and arm64 in
this tree define `__ARCH_WANT_SYS_FORK`, so the syscall exists there:

```c
#ifdef __ARCH_WANT_SYS_FORK
SYSCALL_DEFINE0(fork)
{
#ifdef CONFIG_MMU
	struct kernel_clone_args args = {
		.exit_signal = SIGCHLD,
	};

	return kernel_clone(&args);
#else
	/* can not support in nommu mode */
	return -EINVAL;
#endif
}
#endif
```

The arguments are only an exit signal. Clone flags stay zero: this path
asks for a new `mm`, a new file table, a new signal disposition table, and
a new filesystem context. `SIGCHLD` is the signal the child will deliver to
its parent when it exits. That is the lineage contract, stored later on the
child as `exit_signal`.

`kernel_clone` is the common body. In `kernel/fork.c`, `kernel_clone`, same
tree, after it has chosen whether a ptracer wants an event:

```c
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

	if (clone_flags & CLONE_VFORK) {
		p->vfork_done = &vfork;
		init_completion(&vfork);
		get_task_struct(p);
	}

	if (IS_ENABLED(CONFIG_LRU_GEN) && !(clone_flags & CLONE_VM)) {
		/* lock the task to synchronize with memcg migration */
		task_lock(p);
		lru_gen_add_mm(p->mm);
		task_unlock(p);
	}

	wake_up_new_task(p);
```

`copy_process` allocates the child `task_struct` and fills it. On success,
`kernel_clone` reads the child's pid in the caller's namespace (`pid_vnr`)
and wakes the child with `wake_up_new_task`. The function then returns `nr`
to the parent. The parent's user return value is that pid. The child's user
return value is installed earlier, inside `copy_thread`, as part of building
a resume frame that will run `ret_from_fork` and then the user instruction
after the syscall.

`copy_process` in this file calls the copiers in a fixed order. The ones
that define today's sharing are `copy_files`, `copy_fs`, `copy_sighand`,
`copy_signal`, `copy_mm`, then `copy_thread`, then `alloc_pid` in the
child's PID namespace. Each copier either takes a reference on the parent's
object or allocates a new one. Failure unwinds the ones that already
succeeded. The child task is one object. The address space, the file table,
and the pid are other objects, referenced from it, which is the Day 1 graph
being duplicated under flags.

On x86-64, `copy_thread` in `arch/x86/kernel/process.c` builds that resume
frame. For an ordinary user task it copies the parent's `pt_regs` and then
sets the child's syscall return register to zero:

```c
	/* Kernel thread ? */
	if (unlikely(p->flags & PF_KTHREAD)) {
		p->thread.pkru = pkru_get_init_value();
		memset(childregs, 0, sizeof(struct pt_regs));
		kthread_frame_init(frame, sp, arg);
		return 0;
	}

	/*
	 * Clone current's PKRU value from hardware. tsk->thread.pkru
	 * is only valid when scheduled out.
	 */
	p->thread.pkru = read_pkru();

	frame->bx = 0;
	*childregs = *current_pt_regs();
	childregs->ax = 0;
	if (sp)
		childregs->sp = sp;
```

Earlier in the same function, `frame->ret_addr` is `ret_from_fork` and
`p->thread.sp` points at that frame. When the child is scheduled, the
kernel resume path pops into `ret_from_fork`, which returns to user mode
with `RAX` already 0. The parent's `RAX` is whatever `kernel_clone`
returned, the child's pid. Both tasks continue at the user instruction
after the syscall. They have different register files because they have
different `pt_regs`. They have the same virtual address for that
instruction because the page tables were duplicated from the parent.

`vfork` in the same file passes `CLONE_VFORK | CLONE_VM` and the same
`SIGCHLD`. `CLONE_VM` is the flag that shares the `mm`. The plain `fork`
syscall leaves it clear.

---

## 2. `copy_mm` shares the address space only under `CLONE_VM`

In `kernel/fork.c`, `copy_mm`, same tree. The function first clears the
child's fault and switch counters, then decides:

```c
	tsk->mm = NULL;
	tsk->active_mm = NULL;

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

A kernel thread has `current->mm == NULL`. The child then also has a null
`mm` and will borrow an `active_mm` when it runs, which is the Day 1 kernel
thread case and the Week 7 shared-page-table case.

`CLONE_VM` takes another reference (`mmget`) and stores the parent's `mm`
into the child. One `mm` means one `pgd`, one list of VMAs, one set of
page tables. A store by either task walks the same tables. Threads that
share an address space pass this flag. `vfork` passes it so the child can
run in the parent's address space until it `exec`s or exits; the parent
waits in `kernel_clone` on `vfork_done`.

Plain `fork` fails the flag test and calls `dup_mm`. `dup_mm` allocates a
new `mm_struct`, copies the VMA layout, and duplicates the page tables
through `dup_mmap`. The child receives `tsk->mm` and `tsk->active_mm`
pointing at that new descriptor. Two roots. Week 5's walk in the child
starts at the child's `pgd` once the scheduler installs it.

---

## 3. Duplicating a page table write-protects a COW mapping

A shared mapping (`VM_SHARED`) is supposed to make one task's store visible
to the other. A private mapping keeps the store inside the storing address
space. Copy-on-write still starts from one frame: it removes write
permission from both PTEs, the hardware faults the first store, and the
fault handler allocates a private frame.

`is_cow_mapping` in `include/linux/mm.h`, same tree, is the predicate.
`page_needs_cow_for_dma` uses it when a pin means the frame must be copied
during `fork` itself:

```c
static inline bool is_cow_mapping(vm_flags_t flags)
{
	return (flags & (VM_SHARED | VM_MAYWRITE)) == VM_MAYWRITE;
}

/*
 * This should most likely only be called during fork() to see whether we
 * should break the cow immediately for a page on the src mm.
 */
static inline bool page_needs_cow_for_dma(struct vm_area_struct *vma,
					  struct page *page)
{
	if (!is_cow_mapping(vma->vm_flags))
		return false;
```

A mapping is copy-on-write when it may be written and it is private.
`VM_MAYWRITE` is the permission the VMA is allowed to have, which stays set
even after the PTE write bit is cleared. The fault handler uses it later to
decide that the faulting task is allowed to become writable again after the
copy.

In `mm/memory.c`, `copy_present_pte`, same tree, after the page's map count
has been duplicated:

```c
	/*
	 * If it's a COW mapping, write protect it both
	 * in the parent and the child
	 */
	if (is_cow_mapping(vm_flags) && pte_write(pte)) {
		ptep_set_wrprotect(src_mm, addr, src_pte);
		pte = pte_wrprotect(pte);
	}

	/*
	 * If it's a shared mapping, mark it clean in
	 * the child
	 */
	if (vm_flags & VM_SHARED)
		pte = pte_mkclean(pte);
	pte = pte_mkold(pte);

	if (!userfaultfd_wp(dst_vma))
		pte = pte_clear_uffd_wp(pte);

	set_pte_at(dst_vma->vm_mm, addr, dst_pte, pte);
	return 0;
```

`ptep_set_wrprotect` clears the writable bit in the parent's live PTE.
`pte_wrprotect` clears it in the value that `set_pte_at` installs in the
child. Both PTEs still contain the same page-frame number. Above this
block, when the leaf names an ordinary page, the function has already
called `get_page` and `page_dup_rmap`, so the frame's reference count and
map count include the child. The child's page-table pages themselves are
new. The data frame is shared.

A page that is pinned for DMA is handled just before this path, in
`copy_present_page`. If `page_needs_cow_for_dma` says the parent has pinned
this anonymous page, the kernel copies the frame during `fork` and installs
the copy in the child immediately. The pin must keep seeing the parent's
frame. The common case, an ordinary anonymous or private file page with no
such pin, takes the write-protect path.

Read-only PTEs stay read-only. There is nothing to unshare until something
makes the mapping writable. Executable text that is mapped read-only can
remain one frame for parent and child for the rest of their lives. The
instruction both tasks return to can be that shared frame, fetched through
two roots.

```text
before fork                         after fork, before any store
parent PTE                          parent PTE              child PTE
V → PFN 0x12000, writable           V → PFN 0x12000, RO     V → PFN 0x12000, RO
one mapcount                        mapcount includes both tasks
                                    two pgd roots
```

---

## 4. The first store faults and copies; the last reference may reuse

A store to a present PTE whose writable bit is clear enters the fault
handler with the address, the VMA, and the original PTE. In `mm/memory.c`,
`do_wp_page`, same tree, the anonymous decision is:

```c
	if (PageAnon(vmf->page)) {
		struct page *page = vmf->page;

		/* PageKsm() doesn't necessarily raise the page refcount */
		if (PageKsm(page) || page_count(page) != 1)
			goto copy;
		if (!trylock_page(page))
			goto copy;
		if (PageKsm(page) || page_mapcount(page) != 1 || page_count(page) != 1) {
			unlock_page(page);
			goto copy;
		}
		/*
		 * Ok, we've got the only map reference, and the only
		 * page count reference, and the page is locked,
		 * it's dark out, and we're wearing sunglasses. Hit it.
		 */
		unlock_page(page);
		wp_page_reuse(vmf);
		return VM_FAULT_WRITE;
	} else if (unlikely((vma->vm_flags & (VM_WRITE|VM_SHARED)) ==
					(VM_WRITE|VM_SHARED))) {
		return wp_page_shared(vmf);
	}
copy:
	/*
	 * Ok, we need to copy. Oh, well..
	 */
	get_page(vmf->page);

	pte_unmap_unlock(vmf->pte, vmf->ptl);
	return wp_page_copy(vmf);
```

Right after `fork`, the frame is mapped by parent and child, so
`page_count` and `page_mapcount` are above one. The faulting task falls
through to `wp_page_copy`. `wp_page_reuse` is the other exit: the faulting
task holds the only map and the only page reference, so the handler marks
the existing PTE writable and dirty. That happens for the second task once
the first task has dropped its map of this frame, and it also happens for
a page that was already private. The write bit is still clear until this
fault, because `copy_present_pte` cleared it. Reuse avoids a second
allocation. It still is a write fault.

The copy itself is `wp_page_copy`. In `mm/memory.c`, same tree, the
allocation and the byte copy:

```c
static vm_fault_t wp_page_copy(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct mm_struct *mm = vma->vm_mm;
	struct page *old_page = vmf->page;
	struct page *new_page = NULL;
	pte_t entry;
	int page_copied = 0;
	struct mmu_notifier_range range;
	int ret;

	if (unlikely(anon_vma_prepare(vma)))
		goto oom;

	if (is_zero_pfn(pte_pfn(vmf->orig_pte))) {
		new_page = alloc_zeroed_user_highpage_movable(vma,
							      vmf->address);
		if (!new_page)
			goto oom;
	} else {
		new_page = alloc_page_vma(GFP_HIGHUSER_MOVABLE, vma,
				vmf->address);
		if (!new_page)
			goto oom;

		ret = cow_user_page(new_page, old_page, vmf);
```

A zero page becomes a fresh zeroed frame. Any other page is allocated and
filled by `cow_user_page`, which copies the source frame's bytes. The
function then charges the new page, locks the PTE again, and checks that
the PTE still matches `orig_pte` (another fault might have won the race).
On a match it builds a PTE from `mk_pte(new_page, vma->vm_page_prot)`,
marks it young, and `maybe_mkwrite(pte_mkdirty(entry), vma)` sets the
writable bit because the VMA has `VM_WRITE`. It clears the old PTE, flushes
that translation, adds the new page as an anonymous rmap, and
`set_pte_at_notify` installs the new PTE in the faulting `mm` only. Then
`page_remove_rmap` drops the faulting task's map of the old page. The other
task's PTE is untouched: same virtual address, old frame, write bit still
clear.

The fault returns, the store instruction is restarted, and the writable PTE
lets the store complete into the new frame. `min_flt` on that task counts
the fault. The other task's registers and its PTE stay as they were.

---

## 5. Execution trace: one virtual address, two stores

Illustrated values, one anonymous private page, for a single-threaded
parent. The lab prints pids and the pipe payload. This trace is the path the
functions above take.

```text
V = 0x7fff00001000          illustrative user virtual address
parent, before fork:
  mm_p->pgd walks V → PFN 0x12000, present, writable
  bytes at that frame: integer 1
```

1. The parent executes the `fork` syscall. `SYSCALL_DEFINE0(fork)` calls
   `kernel_clone` with `exit_signal = SIGCHLD` and empty flags.
   `copy_process` duplicates the task. `copy_mm` calls `dup_mm`.
   `copy_present_pte` write-protects V in both page tables. Both roots now
   translate V to PFN `0x12000` with the writable bit clear. Map count is
   two.

2. `copy_thread` on x86-64 copies `pt_regs` and sets `childregs->ax = 0`.
   `alloc_pid` assigns a pid in the namespace. `wake_up_new_task` queues
   the child. `kernel_clone` returns `pid_vnr` to the parent. The parent's
   libc wrapper returns that positive pid. The child resumes at
   `ret_from_fork` and returns to user mode with `RAX == 0`. Both user
   `RIP` values name the instruction after the syscall. That instruction
   page can still be the shared read-only frame. The data page at V is
   shared and write-protected.

3. The parent stores `2` through V. The parent's walk uses `mm_p->pgd`.
   The leaf is present and the writable bit is clear, so the core raises a
   page fault. `do_wp_page` sees an anonymous page with `page_count != 1`
   and calls `wp_page_copy`. The handler allocates PFN `0x13000`,
   `cow_user_page` copies the integer `1` into it, and the parent's PTE
   for V becomes present, writable, and pointing at `0x13000`.
   `page_remove_rmap` drops the parent's map of `0x12000`. The restarted
   store writes `2` into `0x13000`.

4. The child's PTE for V still names `0x12000` and is still write-protected.
   A load by the child still returns `1`. The virtual address is the same
   number. The page-table root is `mm_c->pgd`. Week 5's walk starts at the
   root installed for `current`, so one virtual address can name two frames.

5. The child stores `3` through V. The child's walk faults. `do_wp_page`
   runs on the child's `mm`. If the child is now the only mapper and the
   only reference, the handler calls `wp_page_reuse` and marks the existing
   PTE writable; the store writes `3` into PFN `0x12000`. If a reference
   remains — the count checks in `do_wp_page` are the authority — the
   handler calls `wp_page_copy` again and the child gets a third frame.
   Either way the parent's frame `0x13000` still holds `2`. The two stores
   used one virtual address and two `mm` objects.

```text
after both stores (reuse on the second fault)
parent:  V → PFN 0x13000 writable, bytes 2
child:   V → PFN 0x12000 writable, bytes 3
```

A shared mapping (`VM_SHARED` and `VM_WRITE`) takes `wp_page_shared` in
`do_wp_page` and marks the existing page writable for both tasks. Private
anonymous memory, including the C stack and a `MAP_PRIVATE` anonymous
`mmap`, takes the copy path. That is why a global in `.data` and a local
on the stack diverge after `fork` on the first store, while a `MAP_SHARED`
region stays one frame.

---

## 6. The file table is private; the open-file description is shared

Memory and descriptors answer different sharing questions. In
`kernel/fork.c`, `copy_files`, same tree:

```c
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

`CLONE_FILES` increments `files_struct.count` and leaves `tsk->files`
pointing at the parent's table. Both tasks then share descriptor indices:
a `close` in one is a `close` in the other. Threads that share the file
table pass this flag. Plain `fork` leaves it clear, so `dup_fd` allocates
a new `files_struct` with `count` 1.

`dup_fd` in `fs/file.c` copies the open-fd and `close_on_exec` bitmaps,
then walks the parent's `struct file *` array. For each non-null pointer
it calls `get_file`, which increments `struct file.f_count`, and stores
that same pointer in the child's table. The child has a private index
space and a shared open-file description. `f_pos` lives in `struct file`,
so a `read` that advances the offset is visible through the other task's
copy of the index. `close` on one index drops one `f_count` and clears
that task's slot. The other task's slot still holds the pointer.

```text
after fork, without CLONE_FILES

parent files_struct                 child files_struct
fd 3 ──────────────┐         ┌───── fd 3
                   ▼         ▼
              struct file  (one f_pos, f_count ≥ 2)
                   │
                 pipe / inode
```

The week-10 tree program uses this directly. In
[`projects/linux-system-programming/src/process_lab.c`](../../projects/linux-system-programming/src/process_lab.c),
`mode_tree` creates a pipe, forks twice, and each child writes three
`long`s: its loop index, `getpid()`, and `getppid()`. The write end is a
`struct file` duplicated by `dup_fd`. Each child `close`s its copy of the
read end and, after the write, its copy of the write end. The parent's
read end remains valid because its `fdtable` slot is a separate reference.
The two children have different pids and the same parent pid, because each
`copy_process` set `real_parent` to the task that called `fork`, and
`getppid` reads that relationship in the caller's namespace. Record order
on the pipe follows whichever child the scheduler runs. The pid values are
the lineage. The shared pipe buffer is the file-description rule.

`copy_fs` is the same pattern for cwd and root: `CLONE_FS` bumps
`fs_struct.users`, and plain `fork` calls `copy_fs_struct`. A later `chdir`
in the child updates the child's `fs`, and the parent's cwd stays.

---

## 7. The child is a new task in the parent's lineage

`exit_signal = SIGCHLD` from the `fork` syscall is stored on the child.
When the child exits, the parent is the `SIGCHLD` recipient Day 1 named as
`parent` (and, absent a tracer, `real_parent`). The child's `task_struct`
is linked on the parent's `children` list. `alloc_pid` creates the
`struct pid` in `nsproxy->pid_ns_for_children`. `getpid` in the child
returns the new `tgid`. `getpid` in the parent returns the parent's `tgid`,
unchanged, because `fork` created a different task.

Two children of one parent, as in `mode_tree`, are two `task_struct`s, two
`mm`s, two pids, and one parent pointer. Their address spaces start as
write-protected duplicates. Their pipe ends start as extra references on
the same `struct file`. Those are independent axes. Equal descriptor
integers across the two children mean "this index was copied," and the
sharing of `f_pos` comes from `get_file` on the same pointer.

Day 4 will wait on these children and read `exit_code`. Today the creation
side is enough: the parent received the pid from `kernel_clone`, the child
received 0 from `copy_thread`, and both are runnable tasks with a lineage
edge between them.

---

## 8. Architecture lens — x86-64, AArch64, and RISC-V

Copy-on-write is a page-table permission fact plus a fault handler in
generic `mm/`. `copy_mm`, `is_cow_mapping`, `do_wp_page`, and
`wp_page_copy` are that C. The ISA supplies the PTE encoding inside
`pte_wrprotect` / `ptep_set_wrprotect`, and it supplies the store fault
that enters the handler.

```text
                legacy fork syscall in this tree     store fault into do_wp_page
x86-64          yes (__ARCH_WANT_SYS_FORK)           page fault, present+read-only
AArch64         yes (__ARCH_WANT_SYS_FORK)           data abort, permission
RISC-V          no; clone / clone3 are compiled      page fault, store vs PTE
```

`arch/riscv/include/asm/unistd.h` in this tree defines
`__ARCH_WANT_SYS_CLONE`. The legacy `fork` syscall is compiled for the
architectures that define `__ARCH_WANT_SYS_FORK`, including x86 and arm64
in this tree. User code on RISC-V reaches `kernel_clone` through
`SYSCALL_DEFINE5(clone)` or `clone3`. The C library on x86-64 often does
the same: `fork()` in libc is a `clone` system call with flags that still
mean "new mm, new files, `SIGCHLD`." `strace` on the lab can print `clone`
for a C `fork()`. Both enter `kernel_clone`. The flag word is what
`copy_mm` and `copy_files` test.

The child's zero return is written by architecture code. The x86-64
`copy_thread` quoted above stores it in `childregs->ax` and resumes through
`ret_from_fork`. AArch64 and RISC-V do the analogous store in their own
`copy_thread`, into the ABI return register of that ISA. The generic
function returns the pid only to the task that called `kernel_clone`,
which is the parent.

The fault entry differs, and the PTE bit that means "writable" differs.
After entry, the handler is `do_wp_page` on the faulting `mm`, and it
allocates a frame only for that `mm`.

---

## 9. Mastery: Explain · Draw · Observe · Build

The mechanism above is the lesson. The wrapper is the evidence you can
check on this host. From the week directory, run
[`challenges/day-002-fork-cow.sh`](../challenges/day-002-fork-cow.sh).
It builds the canonical project and runs `process_lab tree`.

### Explain

- `SYSCALL_DEFINE0(fork)` calls `kernel_clone` with `exit_signal = SIGCHLD`
  and no clone flags. `copy_process` builds the child. The parent receives
  `pid_vnr`. On x86-64 the child receives `childregs->ax = 0`.
- `copy_mm` shares `mm` when `CLONE_VM` is set and calls `dup_mm`
  otherwise. Plain `fork` duplicates the address space.
- `copy_present_pte` write-protects both PTEs of a COW mapping
  (`is_cow_mapping`: private and `VM_MAYWRITE`) and keeps the same frame.
  The first store faults into `do_wp_page` and `wp_page_copy`, which
  installs a new frame in the faulting `mm` only.
- `copy_files` shares `files_struct` under `CLONE_FILES`. Plain `fork`
  calls `dup_fd`, which `get_file`s each `struct file`. The index is
  private. `f_pos` is shared.
- Two children of `mode_tree` have one parent and a shared pipe
  description. Their stacks are separate address spaces under
  copy-on-write.

### Draw

1. Parent and child page tables for virtual address V, before either
   store, both naming one PFN with the writable bit clear.
2. The same diagram after the parent's store: parent PTE names a new PFN,
   child PTE still names the old one.
3. Two `files_struct`s and one `struct file` for the pipe write end, with
   `f_count` at least 2.

### Observe

The wrapper prints one line per child: `node`, `pid`, `ppid`, and the
root pid. Predict two distinct child pids, one shared ppid equal to the
root, and an unspecified order. If `strace` exists, the wrapper keeps
`clone`, `fork`, `wait`, and `exit` lines. A `clone` line is the libc
entry into `kernel_clone`. The wrapper's LIMIT line records the boundary
of the run: lineage and the pipe. Page-fault counts and `/proc/pid/pagemap`
stay in the source trace of section 5. The pipe lines are the runtime
evidence for lineage and for a shared open-file description.

### Build

Using the existing `process_lab` tree mode, write down for each `close` in
the child which reference drops (`f_count` on which end) and which of the
parent's descriptors must stay open for `checked_read` to finish. Then
name the `copy_files` / `dup_fd` path that made those descriptors exist.
Physical frames stay a source explanation unless you add a fault counter
you can account for. The wrapper measures lineage and the shared pipe.

### Why? notebook

1. Why is the page table duplicated at `fork` while the frame stays shared?
2. Why does `VM_MAYWRITE` stay set after `pte_wrprotect`?
3. Why can the second writer's fault call `wp_page_reuse`?
4. Why does `fork` pass `SIGCHLD` and leave `CLONE_VM` clear?
5. Why does `vfork` set `CLONE_VM`?
6. Why does `dup_fd` call `get_file` on the existing `struct file`?
7. Why can `strace` show `clone` for a C `fork()`?
8. Why do parent and child agree on the virtual address of a stack slot
   and disagree on the frame after one of them stores?
