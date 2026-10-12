# Day 2 — A Count Is a Request, a Return Is Progress

**Target time:** approximately 3 hours

- Why the kernel may move fewer bytes than you asked: 30 minutes
- One `read` and one `write` through the VFS: 50 minutes
- Sleep, `EAGAIN`, and `EINTR`: 40 minutes
- The copy across the user boundary, then a loop: 35 minutes
- Mastery, then the wrapper: 25 minutes

> You asked `read` for 4096 bytes and it returned 200. Has the call
> failed, finished the file, or merely paused?

Day 1 made a descriptor an index into a `struct file`, and `f_pos` the
position that index shares with its duplicates. This lesson is what a
later `read` or `write` of that index is allowed to do with a user
buffer. The number you pass is a ceiling. The number that comes back is
how far this attempt got.

The excerpts are from the local tree
`5.10.134-15_rc1-10333-gc9b2385d6793` (commit `c9b2385d6793`), the same
5.10-based tree as Day 1.

## 1. Why the call is allowed to stop early

A disk file, a pipe, a socket, and a device do not have the same notion
of "the next bytes." A regular file has an `i_size` and a page cache.
A pipe has a ring of buffers and a count of writers. A socket has a
receive queue that the peer fills when it chooses. A device method
decides how many bytes its hardware has ready. One kernel function that
promised to fill the user buffer would have to sleep, retry, and hide
those differences.

`vfs_read` and `vfs_write` therefore perform one attempt. They check the
open file, check the user range, and call `f_op->read` or `f_op->write`
(or the `*_iter` method) once. Whatever `ssize_t` that method returns is
what the syscall returns. The VFS does not loop until the caller's count
is satisfied. That loop is the caller's job, because only the caller
knows whether a short result should be retried, reported, or treated as
"try again when the descriptor is ready."

ToyOS is the small version of a short read.
[`vfs_read`](../../projects/toyos/src/vfs.c) sets
`count = length < available ? length : available`, copies that many
bytes from the inode array, advances `offset`, and returns `count`. At
the end of the inode that count is 0. `vfs_write` refuses a write that
does not fit, returning `-28`, so the toy does not yet have a short
write. Linux uses one signed return for all three outcomes, on every
kind of object that supplies `file_operations`.

## 2. Three results, and the libc translation

Inside the kernel the return is an `ssize_t`:

- a positive value is the number of bytes moved;
- `0` is a completed attempt that moved nothing, which for a nonzero
  request at end-of-file, or on a pipe whose last writer is gone, is the
  end;
- a negative value is `-errno`.

`SYSCALL_DEFINE3(read)` returns `ksys_read` directly, so the register
that comes back to user mode holds that `ssize_t`. Libc's `read` and
`write` are a second translation: a negative kernel return becomes `-1`
and `errno` is set to the positive code. A program that treats any
return below the requested count as failure will retry a finished file
forever, or abandon a transfer that had already moved bytes.

Two zeros must stay apart. The header comment in `fs/pipe.c` lines 68–69
says a read with `count = 0` should return 0. `generic_file_buffered_read`
in `mm/filemap.c` does the same when `iov_iter_count(iter)` is 0 (lines
2611–2612). That zero is "you asked for nothing." End-of-file is a zero
returned for a nonzero request once `iocb->ki_pos >= isize` (lines
2651–2652) and no bytes were copied. The caller's loop distinguishes
them by the count it passed in.

`include/uapi/asm-generic/errno.h` line 22 defines `EWOULDBLOCK` as
`EAGAIN`. On this tree they are one code. A test for either name matches
the same return.

## 3. `ksys_read` looks up the index and moves `f_pos` only on progress

```c
/* fs/read_write.c :: file_ppos, ksys_read :: tree c9b2385d6793
 * lines 617–640
 */
/* file_ppos returns &file->f_pos or NULL if file is stream */
static inline loff_t *file_ppos(struct file *file)
{
	return file->f_mode & FMODE_STREAM ? NULL : &file->f_pos;
}

ssize_t ksys_read(unsigned int fd, char __user *buf, size_t count)
{
	struct fd f = fdget_pos(fd);
	ssize_t ret = -EBADF;

	if (f.file) {
		loff_t pos, *ppos = file_ppos(f.file);
		if (ppos) {
			pos = *ppos;
			ppos = &pos;
		}
		ret = vfs_read(f.file, buf, count, ppos);
		if (ret >= 0 && ppos)
			f.file->f_pos = pos;
		fdput_pos(f);
	}
	return ret;
}
```

`fdget_pos` is Day 1's lookup: a bad index stays `-EBADF`, and
`f_pos_lock` is taken when the file has `FMODE_ATOMIC_POS` and more than
one reference. `file_ppos` returns `NULL` for `FMODE_STREAM`, so a stream
has no shared offset to update. Otherwise the current `f_pos` is copied
to a local `pos`, and `vfs_read` receives a pointer to that local. The
store back to `f.file->f_pos` happens only when `ret >= 0`. An error
leaves the shared offset where it was. A short success moves it by
however many bytes the method claimed, because the method updates `*ppos`
and this function publishes that local back.

`fdput_pos` drops the lookup reference and unlocks the position mutex if
it was taken. The user's `buf` is still a `__user` pointer. Nothing in
`ksys_read` dereferences it.

## 4. `vfs_read` checks, caps, and calls the method once

```c
/* fs/read_write.c :: vfs_read :: tree c9b2385d6793
 * lines 476–505
 */
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	ssize_t ret;

	if (!(file->f_mode & FMODE_READ))
		return -EBADF;
	if (!(file->f_mode & FMODE_CAN_READ))
		return -EINVAL;
	if (unlikely(!access_ok(buf, count)))
		return -EFAULT;

	ret = rw_verify_area(READ, file, pos, count);
	if (ret)
		return ret;
	if (count > MAX_RW_COUNT)
		count =  MAX_RW_COUNT;

	if (file->f_op->read)
		ret = file->f_op->read(file, buf, count, pos);
	else if (file->f_op->read_iter)
		ret = new_sync_read(file, buf, count, pos);
	else
		ret = -EINVAL;
	if (ret > 0) {
		fsnotify_access(file);
		add_rchar(current, ret);
	}
	inc_syscr(current);
	return ret;
}
```

`FMODE_READ` and `FMODE_CAN_READ` are the open file's own capability,
set when the file was opened. A write-only description fails here, before
any byte moves. `access_ok(buf, count)` asks whether the whole range is a
user address this kernel is willing to copy. Failure is `-EFAULT`, and
`f_pos` will not move because `ksys_read` stores it only for `ret >= 0`.

`MAX_RW_COUNT` is `(INT_MAX & PAGE_MASK)` in `include/linux/fs.h` line
2542. A request larger than that is shortened before the method runs, so
a regular file can return a short count while `i_size` still has more
bytes. That short count is progress. The next call continues at the
updated `f_pos`.

The method pointer is the object Day 1's `f_op` selected. A pipe, a
socket, and a device node each install their own `read` or `read_iter`.
`vfs_read` calls one of them once and returns. `new_sync_read` builds a
one-element `iov_iter` and calls `read_iter`; it too is one attempt.

For a regular file that attempt often lands in `generic_file_read_iter`.
Its comment in `mm/filemap.c` lines 2719–2737 says the routine is for
filesystems that use the page cache directly, that `IOCB_NOWAIT` means
`-EAGAIN` when the read would wait, and that the return is the number of
bytes copied, including partial reads. `generic_file_buffered_read`
returns `written ? written : error` (line 2714). Bytes already copied
win over a later fault: the positive count is what the caller must
account for, and the offset has already moved.

## 5. `rw_verify_area` rejects a nonsense range

```c
/* fs/read_write.c :: rw_verify_area :: tree c9b2385d6793
 * lines 371–401
 */
	inode = file_inode(file);
	if (unlikely((ssize_t) count < 0))
		return retval;

	/*
	 * ranged mandatory locking does not apply to streams - it makes sense
	 * only for files where position has a meaning.
	 */
	if (ppos) {
		loff_t pos = *ppos;

		if (unlikely(pos < 0)) {
			if (!unsigned_offsets(file))
				return retval;
			if (count >= -pos) /* both values are in 0..LLONG_MAX */
				return -EOVERFLOW;
		} else if (unlikely((loff_t) (pos + count) < 0)) {
			if (!unsigned_offsets(file))
				return retval;
		}

		if (unlikely(inode->i_flctx && mandatory_lock(inode))) {
			retval = locks_mandatory_area(inode, file, pos, pos + count - 1,
					read_write == READ ? F_RDLCK : F_WRLCK);
			if (retval < 0)
				return retval;
		}
	}

	return security_file_permission(file,
				read_write == READ ? MAY_READ : MAY_WRITE);
```

`retval` was set to `-EINVAL` at the top of the function (line 369). A
`count` that does not fit in `ssize_t` is rejected, which is why a huge
request cannot look like a negative length later. A negative position
is `-EINVAL` unless the file uses unsigned offsets, and a position plus
count that wraps is the same class of rejection, or `-EOVERFLOW` when
the unsigned case still cannot represent the range. Streams pass a
`NULL` position from `file_ppos`, so the position checks are skipped, and
the comment says mandatory locking applies only where a position means
something. The last call is the LSM hook. None of this copies bytes.
It decides whether the one attempt may start.

## 6. `vfs_write` and `ksys_write` are the same contract in the other direction

```c
/* fs/read_write.c :: vfs_write :: tree c9b2385d6793
 * lines 585–615
 */
ssize_t vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	ssize_t ret;

	if (!(file->f_mode & FMODE_WRITE))
		return -EBADF;
	if (!(file->f_mode & FMODE_CAN_WRITE))
		return -EINVAL;
	if (unlikely(!access_ok(buf, count)))
		return -EFAULT;

	ret = rw_verify_area(WRITE, file, pos, count);
	if (ret)
		return ret;
	if (count > MAX_RW_COUNT)
		count =  MAX_RW_COUNT;
	file_start_write(file);
	if (file->f_op->write)
		ret = file->f_op->write(file, buf, count, pos);
	else if (file->f_op->write_iter)
		ret = new_sync_write(file, buf, count, pos);
	else
		ret = -EINVAL;
	if (ret > 0) {
		fsnotify_modify(file);
		add_wchar(current, ret);
	}
	inc_syscw(current);
	file_end_write(file);
	return ret;
}
```

```c
/* fs/read_write.c :: ksys_write :: tree c9b2385d6793
 * lines 647–665
 */
ssize_t ksys_write(unsigned int fd, const char __user *buf, size_t count)
{
	struct fd f = fdget_pos(fd);
	ssize_t ret = -EBADF;

	if (f.file) {
		loff_t pos, *ppos = file_ppos(f.file);
		if (ppos) {
			pos = *ppos;
			ppos = &pos;
		}
		ret = vfs_write(f.file, buf, count, ppos);
		if (ret >= 0 && ppos)
			f.file->f_pos = pos;
		fdput_pos(f);
	}

	return ret;
}
```

The mode bits, `access_ok`, `rw_verify_area`, and the `MAX_RW_COUNT` cap
match `vfs_read`. `file_start_write` / `file_end_write` bracket the
method. The method is again one call. A positive return is bytes taken
from the user buffer. `ksys_write` publishes `f_pos` only when `ret >= 0`.
A short write is progress: the next call must pass the tail, not the
original buffer. Day 3 follows a buffered write after this return and
shows why a positive count is still not a durable store.

`O_NONBLOCK` and `O_APPEND` live in `f_flags`, the status word Day 1 put
on the `struct file`. `dup` shares them. A nonblocking duplicate and the
original index wait, or refuse to wait, together.

## 7. A pipe sleeps, returns `EAGAIN`, or returns what it has

`pipe_read` in `fs/pipe.c` is one method `vfs_read` can land on. After
it has copied whatever is already buffered, lines 340–347 decide among
end, short progress, and waiting. No writers breaks the loop: `ret` stays
0 if nothing was copied, which is the end, or stays at the short count if
something was. A nonzero `ret` breaks immediately, so a blocking pipe
read is still short when some data was already there. Only the empty,
still-open case continues. `O_NONBLOCK` in `f_flags` sets `-EAGAIN` and
does not sleep.

The sleep is the rest of that path:

```c
/* fs/pipe.c :: pipe_read :: tree c9b2385d6793
 * lines 344–378
 */
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}
		__pipe_unlock(pipe);

		/*
		 * We only get here if we didn't actually read anything.
		 *
		 * However, we could have seen (and removed) a zero-sized
		 * pipe buffer, and might have made space in the buffers
		 * that way.
		 *
		 * You can't make zero-sized pipe buffers by doing an empty
		 * write (not even in packet mode), but they can happen if
		 * the writer gets an EFAULT when trying to fill a buffer
		 * that already got allocated and inserted in the buffer
		 * array.
		 *
		 * So we still need to wake up any pending writers in the
		 * _very_ unlikely case that the pipe was full, but we got
		 * no data.
		 */
		if (unlikely(was_full))
			wake_up_interruptible_sync_poll(&pipe->wr_wait, EPOLLOUT | EPOLLWRNORM);
		kill_fasync(&pipe->fasync_writers, SIGIO, POLL_OUT);

		/*
		 * But because we didn't read anything, at this point we can
		 * just return directly with -ERESTARTSYS if we're interrupted,
		 * since we've done any required wakeups and there's no need
		 * to mark anything accessed. And we've dropped the lock.
		 */
		if (wait_event_interruptible_exclusive(pipe->rd_wait, pipe_readable(pipe)) < 0)
			return -ERESTARTSYS;
```

`wait_event_interruptible_exclusive` is the task leaving the run queue
until `pipe_readable` is true or a signal is pending. That is the
scheduler from Week 7 applied to an I/O wait queue: the core is free to
run someone else, and this task's registers sit in its saved frame. The
comment says the wait returns `-ERESTARTSYS` when interrupted, and that
this path has not copied bytes, so there is no partial progress to
preserve.

`pipe_write` (lines 547–556) is the mirror. A full pipe with
`O_NONBLOCK` sets `-EAGAIN` when `ret` is still 0. `signal_pending`
sets `-ERESTARTSYS` on the same "nothing committed yet" rule.

The user-visible `EINTR` is what the signal path does with
`-ERESTARTSYS`. On x86, `handle_signal` in `arch/x86/kernel/signal.c`:

```c
/* arch/x86/kernel/signal.c :: handle_signal :: tree c9b2385d6793
 * lines 793–813
 */
	/* Are we from a system call? */
	if (syscall_get_nr(current, regs) >= 0) {
		/* If so, check system call restarting.. */
		switch (syscall_get_error(current, regs)) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
			regs->ax = -EINTR;
			break;

		case -ERESTARTSYS:
			if (!(ksig->ka.sa.sa_flags & SA_RESTART)) {
				regs->ax = -EINTR;
				break;
			}
			fallthrough;
		case -ERESTARTNOINTR:
			regs->ax = regs->orig_ax;
			regs->ip -= 2;
			break;
		}
	}
```

If the handler lacks `SA_RESTART`, `rax` becomes `-EINTR` and libc's
`read` returns `-1` with `errno == EINTR`. If `SA_RESTART` is set, the
original syscall number is restored and `ip` moves back over the
two-byte `syscall` instruction, so the instruction runs again. A loop
that retries `EINTR` is correct for the first case. The second case
never surfaces in user space. Either way the program must not treat
`EINTR` as a permanent failure of the descriptor.

`generic_perform_write` has a direct `-EINTR` as well, when
`fatal_signal_pending(current)` is true (`mm/filemap.c` lines 3572–3574).
That one is not `-ERESTARTSYS`. It is already the errno a write attempt
returns if it has not copied anything yet.

## 8. The bytes cross a checked boundary

`access_ok` in `vfs_read` is the first gate. The copy itself is the
second. Week 8's `copy_from_user` walked the user page tables one byte
at a time because a user register holds an address in that process's
map. This tree's helper is the same rule with a wider copy.

```c
/* include/linux/uaccess.h :: _copy_from_user :: tree c9b2385d6793
 * lines 152–164
 */
static inline __must_check unsigned long
_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	unsigned long res = n;
	might_fault();
	if (!should_fail_usercopy() && likely(access_ok(from, n))) {
		instrument_copy_from_user(to, from, n);
		res = raw_copy_from_user(to, from, n);
	}
	if (unlikely(res))
		memset(to + (n - res), 0, res);
	return res;
}
```

```c
/* include/linux/uaccess.h :: copy_from_user, copy_to_user :: tree c9b2385d6793
 * lines 188–202
 */
static __always_inline unsigned long __must_check
copy_from_user(void *to, const void __user *from, unsigned long n)
{
	if (likely(check_copy_size(to, n, false)))
		n = _copy_from_user(to, from, n);
	return n;
}

static __always_inline unsigned long __must_check
copy_to_user(void __user *to, const void *from, unsigned long n)
{
	if (likely(check_copy_size(from, n, true)))
		n = _copy_to_user(to, from, n);
	return n;
}
```

The return is the number of bytes not copied. Zero means the whole range
moved. `might_fault` is the permission for this path to take a page
fault: the user virtual address is translated through the calling task's
page tables, the same walk Week 5 and Week 7 built. A hole in that map,
or a permission the user is not allowed, stops the copy. The comment at
line 89 says only `copy_from_user` zero-fills the uncopied tail of the
kernel destination. The kernel does not leave leftover stack bytes from
a short copy visible as if they had been read from the user.

`__user` on the pointer is the type-level reminder. A C pointer in kernel
code and a user virtual address are different objects. Dereferencing the
user pointer with an ordinary load skips `access_ok`, the fault-in, and
the zero-fill. That is the bug Week 8's checked copy exists to prevent.

A read's copy is usually `copy_to_user` or `copy_page_to_iter`: kernel
bytes into the user map. A write's copy is the other way. In
`generic_perform_write`, `iov_iter_fault_in_readable` brings the user
page in before `write_begin`, and a short `copied` count is progress or
a fault, not a silent success. The VFS return is still one `ssize_t`.

```text
user virtual address in rsi / x1 / a1
        |
        |  page-table walk of current->mm
        v
access_ok, then raw_copy_* 
        |
        v
kernel buffer or page-cache page
        |
        v
ssize_t in the return register
        positive  -> caller advances its cursor by that many
        0         -> end, for a nonzero request
        negative  -> errno, f_pos unchanged when ksys_* saw ret < 0
```

## 9. A loop that means what it says

This is the caller side of the contract. `read` here is libc: `-1` and
`errno`, not the kernel's negative `ssize_t`. The cursor `got` is the
only record of progress. `EINTR` retries the same tail. `EAGAIN` returns
the bytes already taken so the caller can wait for readiness, or `-1`
when nothing has been taken yet.

```c
#include <errno.h>
#include <unistd.h>

ssize_t read_full(int fd, void *buf, size_t count)
{
    unsigned char *p = buf;
    size_t got = 0;

    while (got < count) {
        ssize_t n = read(fd, p + got, count - got);
        if (n > 0) {
            got += (size_t)n;
            continue;
        }
        if (n == 0)
            return (ssize_t)got;
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN)
            return got ? (ssize_t)got : -1;
        return -1;
    }
    return (ssize_t)got;
}
```

A `write` loop has the same shape, with one extra care: a return of 0
from `write` is not end-of-file. It is a stall the caller has to define
a policy for. The kernel side of a pipe write returns `-EAGAIN` or
sleeps; it does not use 0 as "the reader went away" in the snippet
above. A read's 0 is the end. Mixing those two zeros is how a copy loop
spins.

The `fd` in this function is Day 1's index. If another thread shares the
`files_struct` and closes it, the next `read` can return `EBADF`. If a
`dup` or a `fork` shares the `struct file`, this loop and the other task
advance one `f_pos` under `f_pos_lock`. The cursor `got` is private to
this call. `f_pos` is not.

## 10. Execution trace: a short read, a sleep, a signal

One task, descriptor 3 a blocking pipe, descriptor 4 a regular file
positioned at the start of a 100-byte file. The user asks for 4096.

```text
read(4, buf, 4096)                 ksys_read
  rdi = 4, rsi = buf, rdx = 4096     fdget_pos, local pos = f_pos
                                     vfs_read: access_ok ok
                                     read_iter -> page cache
                                     100 bytes copied, pos = 100
                                     f_pos = 100, return 100
  libc returns 100                   loop adds 100 and calls again

read(4, buf+100, 3996)               ki_pos >= i_size, written == 0
                                     return 0
  libc returns 0                     loop treats 0 as end
                                     total 100, not an error

read(3, buf, 4096)  pipe empty,      pipe_read: ret == 0, writers exist,
O_NONBLOCK clear                     O_NONBLOCK clear
                                     wait_event_interruptible_exclusive
                                     task sleeps on pipe->rd_wait

writer deposits 50 bytes             wake, copy 50, ret = 50
                                     if (ret) break
  libc returns 50                    short, and the pipe still has a writer

signal during the wait,              pipe_read returns -ERESTARTSYS
no bytes copied, SA_RESTART clear    handle_signal sets regs->ax = -EINTR
  libc returns -1, errno = EINTR     loop continues, same buffer
```

The regular-file pair is a full transfer made of a short count and a
zero. The pipe pair is a sleep, then a short count, because data was
available and the method returned it. The signal case copies nothing, so
retrying does not skip a byte and does not double-count one.

## 11. Architecture lens

Day 1 named the registers. For `read` and `write` they hold the
descriptor, the user address, and the count. On x86-64, `arch/x86/entry/entry_64.S`
lines 76–85: `rax` is the syscall number (0 for `read`, 1 for `write` in
`syscall_64.tbl`), `rdi` is arg0, `rsi` arg1, `rdx` arg2. The return
`ssize_t` comes back in `rax`. AArch64 uses `regs->regs[8]` as the number
and `orig_x0` plus `regs[1]`..`regs[5]` as arguments, so a read's
descriptor, buffer, and count are `x0`, `x1`, `x2`. RISC-V keeps the
number in `a7` and returns in `a0`. The generic numbers differ (`read` is
63, `write` is 64) and the C is `ksys_read` / `ksys_write` either way.

The page-table root stays the caller's `mm->pgd` for the whole copy.
`might_fault` and `raw_copy_from_user` are loads and stores whose
addresses are translated by that root. A fault in the kernel's copy path
is the same class of event as a user load that misses: the tables either
name a frame the access may use, or they do not. The kernel then turns
the failure into the uncopied count, and `vfs_read` turns a failed
`access_ok` into `-EFAULT` before the method runs.

`f_pos_lock` is the mutex from Day 1. Two threads, or a parent and child
sharing the open file, do not each run this update unsynchronized.
`files->file_lock` is not held across the wait: `pipe_read` drops the
pipe lock before `wait_event_interruptible_exclusive`, which is why a
closer in another thread can proceed. The sleeper rechecks the pipe when
it wakes.

## 12. Mastery: Explain · Draw · Observe · Build

Run [`../challenges/day-002-stream-io.sh`](../challenges/day-002-stream-io.sh)
after this explanation.

### Explain

`vfs_read` and `vfs_write` check mode, `access_ok`, and `rw_verify_area`,
cap the count at `MAX_RW_COUNT`, and call one method. A positive
`ssize_t` is progress and is the amount `ksys_read` / `ksys_write` add
to `f_pos`. Zero for a nonzero request is the end. A negative value is
`-errno`, and `f_pos` stays put. `EAGAIN` is the nonblocking pipe's
refusal to sleep. `-ERESTARTSYS` from `pipe_read` becomes `-EINTR` or a
restarted `syscall`, depending on `SA_RESTART`. The user pointer is
copied through `access_ok` and `copy_from_user` / `copy_to_user`, which
walk the caller's page tables.

### Draw

1. The path from the user count through `ksys_read`, `vfs_read`, and one
   `f_op` call, with three arrows out: positive, zero, negative.
2. A pipe's decision: no writers, already copied something, `O_NONBLOCK`,
   or `wait_event_interruptible_exclusive`.
3. The user buffer as a virtual address, the page-table walk, and the
   kernel buffer on the other side of `copy_to_user`.

### Observe

On the lab run, record a short positive return, a zero, an `EINTR` retry,
and an `EAGAIN` from a nonblocking descriptor. For each, say whether
`f_pos` moved and which source condition produced it: `MAX_RW_COUNT` or
end-of-file, `pipe_read`'s `if (ret) break`, `O_NONBLOCK`, or
`handle_signal`.

### Build

Extend `read_full` with a `write_full` that advances a cursor by each
positive return, retries `EINTR`, and on `EAGAIN` returns the bytes
already written. Predict the `f_pos` value after a 100-byte file is read
with a 4096-byte request that returns 100 and then 0.

### Why? notebook

1. Why does `vfs_read` call the method once?
2. Why does `ksys_read` assign `f_pos` only when `ret >= 0`?
3. Why can a regular file return a short count while bytes remain?
4. Why is a zero from `read` of a nonzero count the end, while a zero
   request is also a zero return?
5. Why does `pipe_read` return `-EAGAIN` only when `ret` is still 0?
6. Why does `-ERESTARTSYS` sometimes appear in user space as `EINTR`?
7. Why does `_copy_from_user` zero the uncopied tail?
8. Why is the user buffer address translated by `mm->pgd` during the copy?
9. Why does ToyOS `vfs_read` return a short count, while its `vfs_write`
   returns `-28` when the bytes do not fit?

**Next:** Day 3 follows a positive `write` into a dirty page-cache page,
and shows what `fsync` waits for.
