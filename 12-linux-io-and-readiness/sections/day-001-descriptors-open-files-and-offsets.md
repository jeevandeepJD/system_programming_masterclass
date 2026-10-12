# Day 1 — A Descriptor Is a Table Index

**Target time:** approximately 3 hours

- Why the kernel returns an integer: 30 minutes
- The chain from `files_struct` to `f_pos`: 45 minutes
- Sharing, close, and the two kinds of flags: 50 minutes
- One trace of open, dup, read, lseek, close: 30 minutes
- Mastery, then the wrapper: 25 minutes

> `open` returned `3`. What object does that integer name, and why does
> `lseek` on a duplicate move the next `read` on the original?

Week 8 crossed into the kernel with a saved user register frame and a
checked copy. Week 9's ToyOS table already returns an integer for a slot
that holds an offset and points at an inode. Week 10 hung `files` off
`task_struct`. Week 11 showed `fork` duplicating that table and a pthread
sharing it. Linux gives each of those layers its own lifetime. The
integer is an index. The open file is a kernel object. The offset lives
on the open file, so two indexes can share one position and two opens of
one path can have two positions.

The excerpts below are from the local tree
`5.10.134-15_rc1-10333-gc9b2385d6793` (commit `c9b2385d6793`). That tree
is 5.10-based. Field layout in a later mainline release can move. The
names quoted here are what this revision actually contains.

## 1. Why an integer is the handle

A user program needs to name an open file across many calls. The name it
is allowed to hold is a small integer in its own memory. On the next
syscall that integer arrives in an argument register, and the kernel uses
it as a subscript into a table only the kernel allocates. Week 8's
`copy_from_user` is the same rule for addresses: a value in a user
register is an address in that process's page tables.

`files_lookup_fd_raw` in `include/linux/fdtable.h` refuses an index at or
past `max_fds`, then clamps it with `array_index_nospec` before the load.
`pick_file` in `fs/file.c` does the same clamp before it clears a slot.
A page-table walk has the same shape. The tables decide the translation.
The task's file table decides which open file the integer names.

ToyOS already uses the integer this way. In
[`projects/toyos/src/vfs.c`](../../projects/toyos/src/vfs.c), `vfs_open`
returns `VFS_FILE_BASE + i` for a free slot in `files[4]`. `file_for`
rejects a number outside that range and a slot whose `open` flag is
clear. The user program never holds `&files[i]`.

## 2. The table is an object the task points at

`task_struct` has a `files` pointer. Week 10's picture stopped there.
The object on the other end of that pointer is `files_struct`, and the
array inside it is an `fdtable`. Both live in `include/linux/fdtable.h`.

```c
/* include/linux/fdtable.h :: struct fdtable :: tree c9b2385d6793
 * lines 27–44
 */
struct fdtable {
	unsigned int max_fds;
	struct file __rcu **fd;      /* current fd array */
	unsigned long *close_on_exec;
	unsigned long *open_fds;
	unsigned long *full_fds_bits;
	struct rcu_head rcu;
};

static inline bool close_on_exec(unsigned int fd, const struct fdtable *fdt)
{
	return test_bit(fd, fdt->close_on_exec);
}

static inline bool fd_is_open(unsigned int fd, const struct fdtable *fdt)
{
	return test_bit(fd, fdt->open_fds);
}
```

```c
/* include/linux/fdtable.h :: struct files_struct :: tree c9b2385d6793
 * lines 49–68
 */
struct files_struct {
  /*
   * read mostly part
   */
	atomic_t count;
	bool resize_in_progress;
	wait_queue_head_t resize_wait;

	struct fdtable __rcu *fdt;
	struct fdtable fdtab;
  /*
   * written part on a separate cache line in SMP
   */
	spinlock_t file_lock ____cacheline_aligned_in_smp;
	unsigned int next_fd;
	unsigned long close_on_exec_init[1];
	unsigned long open_fds_init[1];
	unsigned long full_fds_bits_init[1];
	struct file __rcu * fd_array[NR_OPEN_DEFAULT];
};
```

`count` is how many tasks point at this table. Week 11's `copy_files`
increments it under `CLONE_FILES`, so those threads share one array.
`fdt` is the live `fdtable`. `fdtab` and `fd_array` are the embedded
first table, `NR_OPEN_DEFAULT` slots. `file_lock` covers the written
half. `fd[i]` is the open-file pointer. `open_fds` and `close_on_exec`
are bits of the index.

```text
task A                         task B (CLONE_FILES)
  files ──┐                     files ──┘
          v
     files_struct.count >= 2
       fdt -> fd[3] ──> struct file (one f_pos, one f_count)
              close_on_exec bit 3

task C after fork (CLONE_FILES clear)
  files -> a new files_struct
             fd[3] ──> the same struct file
             its own close_on_exec bit 3
```

`dup_fd` is the fork copy. The next section is the object it points at.

## 3. `struct file` is the open instance

The pointer in `fd[i]` aims at `struct file` in `include/linux/fs.h`.
This is the object Week 9 called an open-file slot, with a kernel
lifetime of its own.

```c
/* include/linux/fs.h :: struct file :: tree c9b2385d6793
 * lines 959–978
 */
struct file {
	union {
		struct llist_node	fu_llist;
		struct rcu_head 	fu_rcuhead;
	} f_u;
	struct path		f_path;
	struct inode		*f_inode;	/* cached value */
	const struct file_operations	*f_op;

	/*
	 * Protects f_ep_links, f_flags.
	 * Must not be taken from IRQ context.
	 */
	spinlock_t		f_lock;
	enum rw_hint		f_write_hint;
	atomic_long_t		f_count;
	unsigned int 		f_flags;
	fmode_t			f_mode;
	struct mutex		f_pos_lock;
	loff_t			f_pos;
```

`f_count` is the reference count. `get_file` (`include/linux/fs.h` lines
1008–1012) increments it. Each index that holds the pointer is one
reference, and so is a kernel lookup that has not put the file yet.
`f_pos` is the byte offset. `f_pos_lock` guards it when more than one
reference exists. `f_flags` holds status such as `O_NONBLOCK`. `f_lock`
guards that word. `f_mode` holds kernel bits such as `FMODE_READ` and
`FMODE_LSEEK`. `f_op` is the method table. `f_inode` is the cached inode.
`f_path` is the `include/linux/path.h` pair of mount and dentry; the
dentry's `d_inode` is "where the name belongs."

`__alloc_file` in `fs/file_table.c` allocates with `kmem_cache_zalloc` and
sets `f_count` to 1, so a new open file's `f_pos` starts at 0. Line 995
of `struct file`, `f_mapping`, points at the inode's `address_space`.
Day 3 follows it. `do_dentry_open` stores `f_inode`, copies
`inode->i_mapping` into `f_mapping`, installs `f_op`, sets
`FMODE_ATOMIC_POS` for a regular file or a directory, and clears
`O_CREAT`, `O_EXCL`, `O_NOCTTY`, and `O_TRUNC` from `f_flags`. Those four
bits were instructions for open.

## 4. Open reserves an index, then publishes the pointer

The user-visible `open` on this tree is a thin wrapper. `sys_open` may
set `O_LARGEFILE`, then calls `do_sys_open(AT_FDCWD, ...)`. `do_sys_open`
packs the flags into an `open_how` and calls `do_sys_openat2`.

```c
/* fs/open.c :: do_sys_openat2, do_sys_open :: tree c9b2385d6793
 * lines 1218–1251
 */
static long do_sys_openat2(int dfd, const char __user *filename,
			   struct open_how *how)
{
	struct open_flags op;
	int fd = build_open_flags(how, &op);
	struct filename *tmp;

	if (fd)
		return fd;

	tmp = getname(filename);
	if (IS_ERR(tmp))
		return PTR_ERR(tmp);

	fd = get_unused_fd_flags(how->flags);
	if (fd >= 0) {
		struct file *f = do_filp_open(dfd, tmp, &op);
		if (IS_ERR(f)) {
			put_unused_fd(fd);
			fd = PTR_ERR(f);
		} else {
			fsnotify_open(f);
			fd_install(fd, f);
		}
	}
	putname(tmp);
	return fd;
}

long do_sys_open(int dfd, const char __user *filename, int flags, umode_t mode)
{
	struct open_how how = build_open_how(flags, mode);
	return do_sys_openat2(dfd, filename, &how);
}
```

`getname` copies the user path before any slot exists. The pointer in
the user register is a virtual address in the caller's page tables.
`get_unused_fd_flags` then reserves an integer. `do_filp_open` builds the
`struct file` only after that reservation. Failure calls `put_unused_fd`.
Success calls `fd_install`. The `open_fds` bit stays set while
`do_filp_open` runs, so two opens are not handed one index.
`__put_unused_fd` (lines 586–591) clears the bit and may rewind `next_fd`,
which is how a closed integer gets reused.

```c
/* fs/file.c :: alloc_fd (tail), get_unused_fd_flags :: tree c9b2385d6793
 * lines 560–583
 */
	if (start <= files->next_fd)
		files->next_fd = fd + 1;

	__set_open_fd(fd, fdt);
	if (flags & O_CLOEXEC)
		__set_close_on_exec(fd, fdt);
	else
		__clear_close_on_exec(fd, fdt);
	error = fd;

out:
	spin_unlock(&files->file_lock);
	return error;
}

int __get_unused_fd_flags(unsigned flags, unsigned long nofile)
{
	return alloc_fd(0, nofile, flags);
}

int get_unused_fd_flags(unsigned flags)
{
	return __get_unused_fd_flags(flags, rlimit(RLIMIT_NOFILE));
}
```

`alloc_fd` holds `files->file_lock` until the bitmaps are updated. The
ceiling is `RLIMIT_NOFILE`. `O_CLOEXEC` is still in this flags word;
`build_open_flags` strips it from the open itself, and `alloc_fd` is
what records the bit in `close_on_exec`.

```c
/* fs/file.c :: fd_install :: tree c9b2385d6793
 * lines 620–642
 */
void fd_install(unsigned int fd, struct file *file)
{
	struct files_struct *files = current->files;
	struct fdtable *fdt;

	rcu_read_lock_sched();

	if (unlikely(files->resize_in_progress)) {
		rcu_read_unlock_sched();
		spin_lock(&files->file_lock);
		fdt = files_fdtable(files);
		WARN_ON(fdt->fd[fd] != NULL);
		rcu_assign_pointer(fdt->fd[fd], file);
		spin_unlock(&files->file_lock);
		return;
	}
	/* coupled with smp_wmb() in expand_fdtable() */
	smp_rmb();
	fdt = rcu_dereference_sched(files->fdt);
	BUG_ON(fdt->fd[fd] != NULL);
	rcu_assign_pointer(fdt->fd[fd], file);
	rcu_read_unlock_sched();
}
```

`rcu_assign_pointer` publishes the pointer. The comment at lines 616–617
says this consumes the file reference, as if the caller had called
`fput`. The `BUG_ON` is the reservation's promise: the slot is empty.
The comment above the function names a `dup2` that would install a
different file between the bitmap update and this store.

## 5. `dup` and `fork` share `f_pos`; a second `open` does not

`dup` looks up the existing pointer, takes another reference, and
installs that same pointer at a new index. `fork` does the same per
slot inside `dup_fd` (`fs/file.c` lines 383–397): `get_file` on each
live pointer, then `rcu_assign_pointer` into the child's new table.

```c
/* fs/file.c :: sys_dup :: tree c9b2385d6793
 * lines 1228–1241
 */
SYSCALL_DEFINE1(dup, unsigned int, fildes)
{
	int ret = -EBADF;
	struct file *file = fget_raw(fildes);

	if (file) {
		ret = get_unused_fd_flags(0);
		if (ret >= 0)
			fd_install(ret, file);
		else
			fput(file);
	}
	return ret;
}
```

`fget_raw` is `__fget(fd, 0, 1)`, so it takes one reference and
`fd_install` consumes it. Both indexes hold the same address and the
same `f_pos`. `f_dupfd` (lines 1243–1254) is the `F_DUPFD` form: it
calls `get_file` and then `fd_install`. `sys_dup` passes flags `0`, so
`alloc_fd` clears close-on-exec on the new index. `F_DUPFD_CLOEXEC`
passes `O_CLOEXEC` and sets the bit on the new index only.

The child after `fork` has private indexes and shares each `struct file`.
Week 11's `copy_files` is the other switch: `CLONE_FILES` increments
`files_struct.count` and shares the table. A second `open` calls
`do_filp_open` again and gets a new `struct file` whose `f_pos` starts
at 0.

`file_needs_f_pos_lock` (`fs/file.c` lines 997–1000) is true when
`FMODE_ATOMIC_POS` is set and `file_count` is greater than one, or the
inode is a directory. `__fdget_pos` then takes `f_pos_lock` around the
update of that one `loff_t`.

## 6. Close clears one index and drops one reference

`sys_close` calls `close_fd`, which calls `__close_fd`. The slot is
cleared before the file's release work runs.

```c
/* fs/file.c :: pick_file, __close_fd :: tree c9b2385d6793
 * lines 646–679
 */
static struct file *pick_file(struct files_struct *files, unsigned fd)
{
	struct file *file = NULL;
	struct fdtable *fdt;

	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);
	if (fd >= fdt->max_fds)
		goto out_unlock;
	fd = array_index_nospec(fd, fdt->max_fds);
	file = fdt->fd[fd];
	if (!file)
		goto out_unlock;
	rcu_assign_pointer(fdt->fd[fd], NULL);
	__put_unused_fd(files, fd);

out_unlock:
	spin_unlock(&files->file_lock);
	return file;
}

/*
 * The same warnings as for __alloc_fd()/__fd_install() apply here...
 */
int __close_fd(struct files_struct *files, unsigned fd)
{
	struct file *file;

	file = pick_file(files, fd);
	if (!file)
		return -EBADF;

	return filp_close(file, files);
}
```

`pick_file` clamps the index, unpublishes `fd[fd]`, and returns `NULL`
for an empty slot, which `__close_fd` turns into `-EBADF`.
`__put_unused_fd` clears `open_fds` and may rewind `next_fd`, so the next
`alloc_fd` can reuse the integer. After that the old `int` is a stale
name: a later `open` may install a different `struct file` at the same
index. The bits in the user's variable survived; the object did not.

`filp_close` (`fs/open.c` lines 1332–1349) calls `f_op->flush` when
present, drops POSIX locks for this owner, then `fput`. `fput_many`
(`fs/file_table.c` lines 335–354) subtracts from `f_count` with
`atomic_long_sub_and_test` and queues `__fput` only when the count hits
zero. A `dup`'d file stays alive when one index closes. `__fput` calls
`f_op->release` and drops the path. The inode has its own `i_count`.

`sys_close` has a second rule, written on the syscall itself: if
`close_fd` returns a restart code, `sys_close` rewrites it to `-EINTR`.
The comment says the close cannot be restarted because the table entry
was already cleared. A signal arriving during flush must not replay an
operation that has already unpublished the slot.

## 7. `O_CLOEXEC` is a slot bit; status flags live on the file

`build_open_flags` (`fs/open.c` lines 1042–1057) sets
`strip = FMODE_NONOTIFY | O_CLOEXEC` and clears those bits from the local
flags that describe the open. The comment says `O_CLOEXEC` is not
relevant to `struct open_flags`. `how->flags` is unchanged, so
`get_unused_fd_flags(how->flags)` still sees the bit and `alloc_fd`
records it in `close_on_exec`. Two stores, two objects.

`fcntl` keeps the split visible to user space. `do_fcntl` in `fs/fcntl.c`:

```c
/* fs/fcntl.c :: do_fcntl :: tree c9b2385d6793
 * lines 337–356
 */
	switch (cmd) {
	case F_DUPFD:
		err = f_dupfd(arg, filp, 0);
		break;
	case F_DUPFD_CLOEXEC:
		err = f_dupfd(arg, filp, O_CLOEXEC);
		break;
	case F_GETFD:
		err = get_close_on_exec(fd) ? FD_CLOEXEC : 0;
		break;
	case F_SETFD:
		err = 0;
		set_close_on_exec(fd, arg & FD_CLOEXEC);
		break;
	case F_GETFL:
		err = filp->f_flags;
		break;
	case F_SETFL:
		err = setfl(fd, filp, arg);
		break;
```

`F_GETFD` / `F_SETFD` read and write the bitmap for this index.
`F_GETFL` returns `filp->f_flags`. `F_SETFL` calls `setfl`, which holds
`filp->f_lock` and writes

```text
filp->f_flags = (arg & SETFL_MASK) | (filp->f_flags & ~SETFL_MASK);
```

with `SETFL_MASK` equal to `O_APPEND | O_NONBLOCK | O_NDELAY | O_DIRECT |
O_NOATIME` (`fs/fcntl.c` line 33, assignment at line 80). `dup` shares those status bits, because `dup` shares the `struct file`.
`F_SETFD` changes one index's bitmap bit. `do_close_on_exec` walks that
bitmap at `exec` and closes those indexes. Another index, or another
task's table, can still hold the `struct file`.

## 8. `lseek` writes the shared `f_pos`

`read` and `write` also move `f_pos`. Day 2 follows that path. The seek
path is the one that writes the field on purpose.

```c
/* fs/read_write.c :: vfs_llseek, ksys_lseek :: tree c9b2385d6793
 * lines 291–320
 */
loff_t vfs_llseek(struct file *file, loff_t offset, int whence)
{
	loff_t (*fn)(struct file *, loff_t, int);

	fn = no_llseek;
	if (file->f_mode & FMODE_LSEEK) {
		if (file->f_op->llseek)
			fn = file->f_op->llseek;
	}
	return fn(file, offset, whence);
}
EXPORT_SYMBOL(vfs_llseek);

static off_t ksys_lseek(unsigned int fd, off_t offset, unsigned int whence)
{
	off_t retval;
	struct fd f = fdget_pos(fd);
	if (!f.file)
		return -EBADF;

	retval = -EINVAL;
	if (whence <= SEEK_MAX) {
		loff_t res = vfs_llseek(f.file, offset, whence);
		retval = res;
		if (res != (loff_t)retval)
			retval = -EOVERFLOW;	/* LFS: should only happen on 32 bit platforms */
	}
	fdput_pos(f);
	return retval;
}
```

`fdget_pos` is `__fdget_pos`: the index becomes a `struct file *`, and
the position mutex is taken when `file_needs_f_pos_lock` says so. A
missing file is `-EBADF`. `vfs_llseek` calls `no_llseek`, which returns
`-ESPIPE`, unless `FMODE_LSEEK` is set and `f_op->llseek` is present.
`default_llseek` writes `file->f_pos` under `inode_lock`. The method
writes the shared field, so `lseek` on one duplicate moves the position
`read` will use on the other. `fdput_pos` drops the lookup reference.

## 9. Execution trace: open, dup, read, lseek, close

One task, already running, with its own `files_struct`. The path names a
regular file. Numbers here are a walk through the calls, not a trace of
one PID.

```text
user                         kernel, this task
open(path, O_RDONLY)         sys_open
  path pointer in rdi          getname: copy the user path
  flags in rsi                 get_unused_fd_flags → index 3
  mode in rdx                  do_filp_open → new struct file
                               f_pos = 0, f_count = 1
                               fd_install(3, file)
                               return 3 in rax

dup(3)                       sys_dup
  3 in rdi                     fget_raw(3): f_count = 2
                               get_unused_fd_flags(0) → index 4
                               close_on_exec bit 4 cleared
                               fd_install(4, same file)
                               return 4

read(3, buf, 1)              ksys_read
                               fdget_pos(3): same struct file
                               copy f_pos (0) to a local
                               one byte copied, local pos = 1
                               f_pos = 1

read(4, buf, 1)              ksys_read
                               fd[4] is the same struct file
                               starts at f_pos 1

lseek(4, 0, SEEK_SET)        ksys_lseek → vfs_llseek
                               f_op->llseek writes f_pos = 0
                               a later read on fd 3 starts at 0

close(4)                     pick_file clears fd[4]
                               filp_close → fput
                               f_count = 1, file still alive

close(3)                     pick_file clears fd[3]
                               fput → count reaches 0
                               __fput → f_op->release
```

A second `open` of the same path allocates another `struct file` at
`f_pos == 0`. Day 2 expands the `read` steps. fd 3 and fd 4 are two
slots and one `f_pos`.

## 10. A small C picture of one shared offset

This is the user-space side of the trace. `open` and `dup` are libc
wrappers around the syscalls above. The checks make the shared position
visible: the second `read` must observe the byte after the first.

```c
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    char byte;
    int fd = open("offset-demo", O_RDWR | O_CREAT | O_TRUNC, 0600);
    int alias;
    if (fd < 0)
        return 1;
    if (write(fd, "abcd", 4) != 4)
        return 1;
    if (lseek(fd, 0, SEEK_SET) != 0)
        return 1;
    alias = dup(fd);
    if (alias < 0)
        return 1;
    if (read(fd, &byte, 1) != 1 || byte != 'a')
        return 1;
    if (read(alias, &byte, 1) != 1 || byte != 'b')
        return 1;
    if (lseek(alias, 0, SEEK_SET) != 0)
        return 1;
    if (read(fd, &byte, 1) != 1 || byte != 'a')
        return 1;
    close(alias);
    close(fd);
    return 0;
}
```

`alias` and `fd` are two `int`s on this stack. After `dup` they select
one `struct file`: the second `read` returns `'b'`, and `lseek` on
`alias` rewinds `read` on `fd`. A second `open` would be a new `f_pos`.
After `close(fd)` the variable still holds the integer, and `next_fd`
may already have reused the slot.

## 11. Registers, locks, and the ToyOS table

On x86-64, `arch/x86/entry/entry_64.S` records the `syscall` registers:
`rax` is the call number, `rdi`/`rsi`/`rdx` are the first three
arguments, and the return comes back in `rax`. `arch/x86/entry/syscalls/syscall_64.tbl`
numbers `read` 0, `write` 1, `open` 2, `close` 3, `lseek` 8, `dup` 32,
`fcntl` 72. A negative `rax` is an errno. AArch64 takes the number from
`regs->regs[8]` in `do_el0_svc` and the arguments from `orig_x0` plus
`regs[1]` through `regs[5]` (`arch/arm64/include/asm/syscall.h`). The
generic table uses different numbers: `openat` 56, `close` 57, `read` 63,
`write` 64 (`include/uapi/asm-generic/unistd.h`). RISC-V holds the number
in `a7` and stores the return to `PT_A0` (`arch/riscv/kernel/entry.S`).
The C behind the stub is `do_sys_open` and `ksys_lseek` on each of them.

`getname` translates the path with the caller's `mm->pgd`. The new
table and `struct file` are kernel objects. `files->file_lock` covers
allocation and `pick_file`. `f_pos_lock` covers `f_pos` once
`file_count` exceeds one. `f_lock` covers `f_flags`, so `F_SETFL` of
`O_NONBLOCK` is visible on every index that shares the file. Threads
with `CLONE_FILES` share the table.

ToyOS
([`projects/toyos/src/vfs.c`](../../projects/toyos/src/vfs.c)) keeps one
`files[4]`. Each slot has its own `offset`, set to 0 by `vfs_open`, and
`file_for` rejects a bad index. The bytes live in the inode array. Linux
adds `f_count`, the shared `f_pos`, the per-index close-on-exec bit, and
a table tasks can share or duplicate. Day 2 follows `f_op`. Day 3 follows
`f_mapping`.

## 12. Mastery: Explain · Draw · Observe · Build

Run
[`../challenges/day-001-descriptors.sh`](../challenges/day-001-descriptors.sh)
after this explanation. The wrapper is the evidence step.

### Explain

A descriptor is an index in the calling task's `fdtable`. The table hangs
off `files_struct`, which hangs off `task_struct.files`. `fd[i]` is a
`struct file *`. That object holds `f_count`, `f_pos`, `f_flags`,
`f_mode`, `f_op`, `f_inode`, and `f_path`. `open` reserves the index in
`get_unused_fd_flags` before `do_filp_open`, then `fd_install` publishes
the pointer and consumes one reference. `dup` and `fork`'s `dup_fd`
install another reference to the same `struct file`, so `f_pos` is
shared. A second `open` builds another `struct file`. `close` clears one
slot and `fput`s one reference. `O_CLOEXEC` is a `close_on_exec` bit on
the index. `O_NONBLOCK` and the other `SETFL_MASK` bits live in
`f_flags`.

### Draw

1. One `files_struct`, its `fdtable`, two indexes pointing at one
   `struct file`, and that file's `f_pos` and `f_inode`.
2. The open sequence: `getname`, reserve index, `do_filp_open`,
   `fd_install`, with the `put_unused_fd` arrow on the error path.
3. After `fork`: two `files_struct`s, two indexes, one `struct file`.
   Beside it, two threads with `CLONE_FILES`: one `files_struct`.

### Observe

On the lab run, record each integer the program prints and say which
kernel object it indexes. Mark which indexes share one `f_pos`, which
close drops a reference without freeing the file, and which close drops
the last reference. Record an `EBADF` from a stale or empty index as the
`pick_file` miss.

### Build

Extend the C picture in section 10 only by a second `open` of the same
path. Predict, before you run it, that the new index has its own `f_pos`
at 0 while `alias` still sees the shared position. Then name the call in
`do_sys_openat2` that allocates that second `struct file`.

### Why? notebook

1. Why is a descriptor an integer in a user register, rather than a
   kernel pointer the user could pass back?
2. Why does `get_unused_fd_flags` run before `do_filp_open`?
3. Why does `fd_install` consume a reference?
4. Why does `sys_dup` pass `0` to `get_unused_fd_flags`?
5. Why can `lseek` on one duplicate move `read` on the other?
6. Why is `O_CLOEXEC` stripped in `build_open_flags` and still honored
   by `alloc_fd`?
7. Why can `close` return `-EINTR` without restarting?
8. Why does `__fdget_pos` lock `f_pos` when `file_count` is greater than
   one?
9. Why does ToyOS store `offset` in `struct file` and the bytes in
   `struct inode`?

**Next:** Day 2 follows `read` and `write` from that index through a
short transfer, a zero end, and a negative errno.
