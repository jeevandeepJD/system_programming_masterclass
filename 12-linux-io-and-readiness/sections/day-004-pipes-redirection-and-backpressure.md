# Day 4 — Pipes, Redirection, and Backpressure

**Target time:** approximately 3 hours

> A pipe is a bounded queue of bytes in the kernel. Each end is a file
> the process already knows how to read and write. The bound is what
> forces a fast writer to sleep.

## 1. Syntax needed for this source

A pipe is one array and two indexes. A loop consumes bytes until the count or the stored data runs out, and the tail wraps.

```c
while (count > 0 && used > 0) {
    buf[i++] = data[tail];
    tail = (tail + 1) % PAGE;
    count--;
    used--;
}
```

`tail` names the next byte to read. `head` names the next byte to write. Both are indexes into the same page. The sleep happens in the inner loop when `used` is 0 and a writer still exists.

## 2. Linux 0.12 first: one page, head and tail, sleep, EOF, SIGPIPE

The bytes cannot live in either process. Linux 0.12 gives the pipe one page, stores head and tail on the inode, and sleeps the reader or the writer on that inode's wait queues. `read_pipe` in `fs/pipe.c`, printed pages 803–804:

```c
while (count>0) {
	while (!(size=PIPE_SIZE(*inode))) {
		wake_up(& PIPE_WRITE_WAIT(*inode));
		if (inode->i_count != 2)
			return read;
		if (current->signal & ~current->blocked)
			return read?read:-ERESTARTSYS;
		interruptible_sleep_on(& PIPE_READ_WAIT(*inode));
	}
	chars = PAGE_SIZE-PIPE_TAIL(*inode);
	if (chars > count)
		chars = count;
	if (chars > size)
		chars = size;
```

Trace. `PIPE_SIZE` is the bytes sitting between head and tail. An empty pipe wakes writers, then returns what was already read when `i_count != 2` (the writer is gone: EOF). A pending signal returns `-ERESTARTSYS` if nothing was read yet. Otherwise the reader sleeps on `PIPE_READ_WAIT`. With data present, `chars` is clamped to the request, to the bytes before the end of the page, and to `size`. The function then advances `PIPE_TAIL`, masks it with `PAGE_SIZE-1`, copies with `put_fs_byte`, and `wake_up`s writers. `write_pipe` on printed page 804 is the mirror: a full page sleeps on `PIPE_WRITE_WAIT`, and `i_count != 2` does `current->signal |= (1<<(SIGPIPE-1))` and returns `written ? written : -1`.

What carries forward is the bounded buffer, sleep when empty or full, EOF when the writer is gone, and a signal when a write finds no reader. The single page, the inode fields used as head/tail, `i_count != 2` as the lifetime test, and the direct signal bit belong to this kernel. Linux 5.10 keeps the causal picture and uses a ring of pipe pages, `readers`/`writers` counts, `pipe_read`/`pipe_write`, poll wait queues, and `-EPIPE` together with `SIGPIPE`.

ToyOS has the queue in miniature. [Week 8's pipe](../../08-user-mode-syscalls-and-ipc/sections/day-006-process-fd-ipc.md) is one 64-byte array and a read position, copied in each direction, without this sleep, EOF, and `SIGPIPE` protocol.

The shared [Linux 0.12 → Linux 5.10 map](../../references/EARLY_LINUX_012_MAP.md) shows the same empty-reader loop. The sections below are the pipe in Linux 5.10.134, tree `c9b2385d6793`. Read 0.12 as the first complete implementation. Develop against the 5.10 tree.

The excerpts in this lesson are Linux 5.10.134 at tree `c9b2385d6793`.
Record `uname -r` before you treat a line number as true of the machine
you are tracing. Names and widths move between releases. The POSIX
contract for `pipe`, `read`, `write`, and `dup2` is the application
interface. This file is one kernel's implementation of that contract.

ToyOS already has the smallest version of the idea. [Week 8's pipe](../../08-user-mode-syscalls-and-ipc/sections/day-006-process-fd-ipc.md)
is one 64-byte array, two fixed descriptors, and a copy in each
direction. Today's work is the part that array leaves out: two real
file objects, a ring that reuses slots, sleep when the ring is full or
empty, end-of-file when the last writer releases its file, and
`SIGPIPE` when a write finds no reader.

## 3. Why the bytes cannot live in either process

A producer and a consumer are two address spaces. [Week 10](../../10-linux-processes-and-signals/sections/day-001-process-mental-model.md)
gives each task its own page tables. A store the producer executes
updates a physical page that the consumer's page tables do not name.
Handing the consumer a raw pointer into the producer would be handing
it an address that translates, in the consumer, to something else or to
nothing.

The kernel is the place both tasks can enter. A `write` follows the
syscall path from [Week 8](../../08-user-mode-syscalls-and-ipc/sections/day-004-int80-syscall-abi.md):
registers cross in, the kernel copies out of the caller's buffer, and
the call returns a count. A later `read` copies into a different user
buffer. The queue between those copies is kernel memory.

The queue is bounded. An unbounded queue lets a fast producer allocate
until the machine is out of pages. A bound turns "the consumer is
behind" into a sleep, the same shape as the condition-variable wait in
[Week 11](../../11-threads-and-concurrency/sections/day-004-condition-variables-and-bounded-queues.md).
The predicates are "there is a free slot" and "there is a byte, or no
writer left." The lock is the pipe mutex. The queues are `rd_wait` and
`wr_wait`.

```text
producer address space          kernel                     consumer address space
  user buffer --write()-->   pipe ring of pages   --read()-->  user buffer
                             head produces
                             tail consumes
                             full  -> writer sleeps on wr_wait
                             empty -> reader sleeps on rd_wait
```

The user-visible object is still a file. [Week 9](../../09-filesystems-and-virtual-devices/sections/day-003-user-fd-syscalls.md)
already made a descriptor an index into a per-task table of open files.
A pipe does not add a new syscall family for data movement. It adds a
pair of files whose `read_iter` and `write_iter` implement this queue.

## 4. One ring, two counters, pages for slots

`include/linux/pipe_fs_i.h` in this tree describes the object. The
comment is the data model. The fields under it are what `pipe_read` and
`pipe_write` actually update.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: include/linux/pipe_fs_i.h
 * lines: 34–64
 * function: struct pipe_inode_info (definition)
 */
/**
 *	struct pipe_inode_info - a linux kernel pipe
 *	@mutex: mutex protecting the whole thing
 *	@rd_wait: reader wait point in case of empty pipe
 *	@wr_wait: writer wait point in case of full pipe
 *	@head: The point of buffer production
 *	@tail: The point of buffer consumption
 *	@note_loss: The next read() should insert a data-lost message
 *	@max_usage: The maximum number of slots that may be used in the ring
 *	@ring_size: total number of buffers (should be a power of 2)
 *	@nr_accounted: The amount this pipe accounts for in user->pipe_bufs
 *	@tmp_page: cached released page
 *	@readers: number of current readers of this pipe
 *	@writers: number of current writers of this pipe
 *	@files: number of struct file referring this pipe (protected by ->i_lock)
 *	@r_counter: reader counter
 *	@w_counter: writer counter
 *	@poll_usage: is this pipe used for epoll, which has crazy wakeups?
 *	@fasync_readers: reader side fasync
 *	@fasync_writers: writer side fasync
 *	@bufs: the circular array of pipe buffers
 *	@user: the user who created this pipe
 *	@watch_queue: If this pipe is a watch_queue, this is the stuff for that
 **/
struct pipe_inode_info {
	struct mutex mutex;
	wait_queue_head_t rd_wait, wr_wait;
	unsigned int head;
	unsigned int tail;
	unsigned int max_usage;
	unsigned int ring_size;
```

`head` and `tail` are stored unmasked. The comment at lines 62–66 of
the same header says they wrap naturally, and that the ring is a power
of two, at most `2^31`. A slot is `bufs[index & (ring_size - 1)]`, a
`struct pipe_buffer` with a `struct page *`, an offset, and a length.
`PIPE_SIZE` is `PAGE_SIZE`. One slot is one page of pipe bytes, not one
userspace `write`.

`alloc_pipe_info` starts from `PIPE_DEF_BUFFERS` (16). Over the soft
per-user limit it shrinks to `PIPE_MIN_DEF_BUFFERS` (2, at the top of
`fs/pipe.c`), because a one-slot pipe blocks a writer that still owes
the pipe a page. `pipe_max_size` is 1048576, the comment's cap for a
non-root `F_SETPIPE_SZ`. A 4096-byte page makes the default ring 16
pages. That product is not a second named constant.

`pipe_empty` is `head == tail`. `pipe_full` is `head - tail >= max_usage`
at the `pipe_read` and `pipe_write` call sites. Unsigned subtraction is
what lets the ring be full with no spare slot reserved to tell full
from empty. `__pipe_lock` takes `pipe->mutex` directly. While it is
held, `head`, `tail`, and the buffer lengths move together. The waiter
is whichever task next enters `read` or `write`, the kernel analogue of
the pthread mutex in [Week 11's queue](../../11-threads-and-concurrency/sections/day-004-condition-variables-and-bounded-queues.md).

## 5. `pipe()` builds two files and publishes two descriptors

`get_pipe_inode` allocates the inode on the pipe pseudo-filesystem,
calls `alloc_pipe_info`, and at lines 882–885 sets `inode->i_pipe`,
`pipe->files = 2`, `pipe->readers = pipe->writers = 1`, and
`inode->i_fop = &pipefifo_fops`.

`files` starts at 2 because `create_pipe_files` is about to build two
`struct file` objects. `readers` and `writers` start at 1: one read
end, one write end. Those counters are not the number of tasks and not
the number of descriptor slots. `fork` and `dup` add references to an
existing `struct file`. `pipe_release` runs later, from `__fput` in
`fs/file_table.c`, when the last reference to that file is dropped.
Until then `writers` stays 1 even if three processes hold the write
end.

`create_pipe_files` is the split into directions. The first `struct
file` is write-only. The read end is a clone of it. Both store the same
`pipe_inode_info` in `private_data`.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: fs/pipe.c
 * lines: 926–948
 * function: create_pipe_files
 */
	f = alloc_file_pseudo(inode, pipe_mnt, "",
				O_WRONLY | (flags & (O_NONBLOCK | O_DIRECT)),
				&pipefifo_fops);
	if (IS_ERR(f)) {
		free_pipe_info(inode->i_pipe);
		iput(inode);
		return PTR_ERR(f);
	}

	f->private_data = inode->i_pipe;

	res[0] = alloc_file_clone(f, O_RDONLY | (flags & O_NONBLOCK),
				  &pipefifo_fops);
	if (IS_ERR(res[0])) {
		put_pipe_info(inode, inode->i_pipe);
		fput(f);
		return PTR_ERR(res[0]);
	}
	res[0]->private_data = inode->i_pipe;
	res[1] = f;
	stream_open(inode, res[0]);
	stream_open(inode, res[1]);
	return 0;
```

`res[0]` is the read end. `res[1]` is the write end. `O_NONBLOCK` is
copied onto both. `O_DIRECT` is applied to the write end only; `pipe_write`
later treats that flag as packet mode, which section 11 keeps separate
from the ordinary byte stream. `stream_open` marks both as streams, so
they have no shared file position that a seek could name. `pipefifo_fops`
wires `.read_iter` to `pipe_read`, `.write_iter` to `pipe_write`,
`.poll` to `pipe_poll`, and `.release` to `pipe_release`.

`pipe2` and `pipe` both land in `do_pipe2`. `pipe` passes flags 0.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: fs/pipe.c
 * lines: 1001–1021
 * function: do_pipe2
 */
static int do_pipe2(int __user *fildes, int flags)
{
	struct file *files[2];
	int fd[2];
	int error;

	error = __do_pipe_flags(fd, files, flags);
	if (!error) {
		if (unlikely(copy_to_user(fildes, fd, sizeof(fd)))) {
			fput(files[0]);
			fput(files[1]);
			put_unused_fd(fd[0]);
			put_unused_fd(fd[1]);
			error = -EFAULT;
		} else {
			fd_install(fd[0], files[0]);
			fd_install(fd[1], files[1]);
		}
	}
	return error;
}
```

`SYSCALL_DEFINE2(pipe2)` returns `do_pipe2`. `__do_pipe_flags` calls
`create_pipe_files`, then `get_unused_fd_flags` twice. `copy_to_user`
publishes the indexes, and only then does `fd_install` make them live.
A faulting copy `fput`s both files, drops the reserved indexes, and
returns `-EFAULT` with nothing installed. That is the
[Week 8 user-copy](../../08-user-mode-syscalls-and-ipc/sections/day-005-safe-user-copying.md)
rule: do not publish a kernel object if the user pointer did not accept
the name.

After a successful `pipe`, the process has two descriptors and one
queue. A `write` on the read index fails the direction check before
`pipe_write` runs, because that `struct file` was opened `O_RDONLY`.
A `read` on the write index fails the same way. The direction lives in
the file, not in a bit the user has to pass on every call.

## 6. `fork` plus `dup2` is how a shell aims the queue

A pipe inside one process is only a queue the process can use to talk
to itself. A pipeline needs a second task whose standard input or
standard output is one of the two ends. [Week 10's fork](../../10-linux-processes-and-signals/sections/day-002-fork-cow-and-lineage.md)
gives the child its own fd table, copied by `dup_fd`, with each live
descriptor holding another reference on the same `struct file`.
[Exec](../../10-linux-processes-and-signals/sections/day-003-exec-elf-and-inheritance.md)
keeps that table, except indexes marked close-on-exec. The program
named by `exec` therefore inherits whatever the child left at
descriptors 0, 1, and 2.

`dup2(pipefd, 1)` makes descriptor 1 refer to the same open file as
`pipefd`, then the child closes the original pipe indexes so that it
does not keep an extra reference. The parent's job is the matching
close of the end it will not use. Those closes are the lifetime
protocol. They are not optional cleanup.

```text
parent                         child A (producer)              child B (consumer)
pipe() -> r, w
fork A, fork B
close(r)                       dup2(w, STDOUT_FILENO)          dup2(r, STDIN_FILENO)
close(w) after                 close(r); close(w)              close(r); close(w)
  the last byte                exec producer                   exec consumer
waitpid both
```

After `exec`, the producer's `write(1, ...)` is `pipe_write` into a
pipe page, and the consumer's `read(0, ...)` is the other end of the
same `pipe_inode_info`. The programs name the ring only as the integers
0 and 1.

If the parent forgets `close(w)`, `writers` never drops. The consumer
sleeps in `read` after the producer has exited, because the parent
still holds the write file. A child that `dup2`s the read end onto
stdin and leaves the write end open has made itself a writer, so its
own later read will not see end-of-file. Section 8 is that rule in
`pipe_release`.

## 7. Three bytes, from the user buffer to the other user buffer

Take a blocking pipe, one writer, one reader, and a three-byte write of
`'a'`, `'b'`, `'c'`. `pipe_write` locks the pipe. `readers` is 1, so it
does not take the `SIGPIPE` path. The ring is empty, so the merge into
a previous buffer is skipped. The ring is not full. The function
allocates a page if `tmp_page` is empty, advances `head` under
`rd_wait.lock`, and installs a `pipe_buffer` whose `ops` are
`anon_pipe_buf_ops` and whose flags are `PIPE_BUF_FLAG_CAN_MERGE`.
`copy_page_from_iter` copies the three bytes at offset 0. `buf->len`
becomes 3. The iterator is empty, so the loop breaks, the lock drops,
and because the pipe was empty the function wakes `rd_wait` with
`EPOLLIN | EPOLLRDNORM`.

That wake makes a reader blocked on `rd_wait` runnable.
[Week 7](../../07-memory-processes-and-scheduling/sections/day-005-cooperative-yield-and-switch.md)
is the register save and restore under that sentence. The pipe mutex
is not held across the sleep.

`pipe_read` then sees `head != tail`. It confirms the buffer, and
`copy_page_to_iter` copies `min(buf->len, requested)` bytes into the
reader's user buffer. A reader that asked for 1 byte leaves `len == 2`
and `offset == 1` and does not advance `tail`. A reader that asked for
8 bytes copies 3, sees `len == 0`, releases the buffer, and increments
`tail`. The return value is the number of bytes copied, 1 or 3, not the
number the writer passed and not a record header.

Both copies are kernel copies through a pipe page. The user's `write`
never stores into the consumer's address space. Day 6 is what changes
when a load names the page through the consumer's own page tables.

## 8. End-of-file is "no writer remains"

An empty pipe and a closed pipe look the same if you only test
`head == tail`. The reader needs a second fact: will any future `write`
still be able to append? `pipe_readable` (lines 220–227) answers the
wait. It loads `head`, `tail`, and `writers` with `READ_ONCE`, because
it runs without the mutex, and it returns true when the ring is
non-empty or when `writers` is zero. The locked path in `pipe_read`
makes the decision that becomes the syscall result:

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: fs/pipe.c
 * lines: 322–348
 * function: pipe_read
 */
			if (!buf->len) {
				pipe_buf_release(pipe, buf);
				spin_lock_irq(&pipe->rd_wait.lock);
#ifdef CONFIG_WATCH_QUEUE
				if (buf->flags & PIPE_BUF_FLAG_LOSS)
					pipe->note_loss = true;
#endif
				tail++;
				pipe->tail = tail;
				spin_unlock_irq(&pipe->rd_wait.lock);
			}
			total_len -= chars;
			if (!total_len)
				break;	/* common path: read succeeded */
			if (!pipe_empty(head, tail))	/* More to do? */
				continue;
		}

		if (!pipe->writers)
			break;
		if (ret)
			break;
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}
		__pipe_unlock(pipe);
```

The sleep itself is the next call, still inside `pipe_read`, after the
comment that explains why an interrupt here can return `-ERESTARTSYS`
directly: the lock is already dropped and no byte was copied.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: fs/pipe.c
 * lines: 367–378
 * function: pipe_read
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

If the ring is empty and `writers` is already 0, the function breaks
with `ret` still 0. The syscall returns 0. That 0 is end-of-file. If
the ring is empty, a writer still exists, and the file is blocking, the
reader drops the mutex and sleeps on `rd_wait` until `pipe_readable`
becomes true. If the file is `O_NONBLOCK`, it returns `-EAGAIN` instead
of sleeping. `-EAGAIN` means "empty, and a writer still exists." `0`
means "empty, and no writer remains." A program that treats both as
"try again" will spin after the writer has gone. A program that treats
both as end-of-file will stop while the writer is merely slow.

`pipe_release` is what drives `writers` to 0. It runs when the write
end's `struct file` loses its last reference.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: fs/pipe.c
 * lines: 711–729
 * function: pipe_release
 */
static int
pipe_release(struct inode *inode, struct file *file)
{
	struct pipe_inode_info *pipe = file->private_data;

	__pipe_lock(pipe);
	if (file->f_mode & FMODE_READ)
		pipe->readers--;
	if (file->f_mode & FMODE_WRITE)
		pipe->writers--;

	/* Was that the last reader or writer, but not the other side? */
	if (!pipe->readers != !pipe->writers) {
		wake_up_interruptible_all(&pipe->rd_wait);
		wake_up_interruptible_all(&pipe->wr_wait);
		kill_fasync(&pipe->fasync_readers, SIGIO, POLL_IN);
		kill_fasync(&pipe->fasync_writers, SIGIO, POLL_OUT);
	}
	__pipe_unlock(pipe);
```

The condition `!pipe->readers != !pipe->writers` is true when one side
has hit zero and the other has not. Closing the last write reference
wakes every reader. A reader blocked in the wait above sees
`pipe_readable` true because `writers` is now 0, reacquires the mutex,
finds the ring empty, and returns 0. Closing the last read reference
wakes every writer, which is the setup for the next section.

Data already in the ring is still readable after the writers have gone.
`pipe_read` copies those bytes first. Only when the ring is empty does
the `!pipe->writers` test return 0. End-of-file is ordered after the
last byte, not instead of it.

## 9. No readers: the signal and the error are both generated

The matching failure on the write side is a pipe that will never be
read again. `pipe_write` checks before copying anything:

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: fs/pipe.c
 * lines: 414–436
 * function: pipe_write
 */
static ssize_t
pipe_write(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *filp = iocb->ki_filp;
	struct pipe_inode_info *pipe = filp->private_data;
	unsigned int head;
	ssize_t ret = 0;
	size_t total_len = iov_iter_count(from);
	ssize_t chars;
	bool was_empty = false;
	bool wake_next_writer = false;

	/* Null write succeeds. */
	if (unlikely(total_len == 0))
		return 0;

	__pipe_lock(pipe);

	if (!pipe->readers) {
		send_sig(SIGPIPE, current, 0);
		ret = -EPIPE;
		goto out;
	}
```

`send_sig(SIGPIPE, current, 0)` posts the signal, and the function sets
`-EPIPE`. Both happen.
[Week 10's delivery](../../10-linux-processes-and-signals/sections/day-005-signals-masks-and-safe-handlers.md)
decides what the task sees. The default disposition terminates the
process, so the C statement after `write` never runs. `SIG_IGN` or a
blocked signal lets `write` return `-1` with `errno == EPIPE`.

The same check repeats inside the copy loop (lines 477–482), because
the writer drops the mutex while it waits for space. `send_sig` still
runs. If some bytes are already copied, `ret` stays the positive short
count and is not replaced with `-EPIPE`. A writer that ignored
`SIGPIPE` must honor that count and expect the next `write` to return
`EPIPE`. A writer that left the default disposition can die on this
path after a partial copy, because the signal is posted either way.
`EPIPE` is the syscall errno. `SIGPIPE` is the signal beside it. Day 7
puts both in one report.

## 10. Backpressure is a sleep on `wr_wait`

The bound is enforced in the same loop. When `pipe_full` is true and
the file is blocking, the writer wakes any reader that might be waiting
on an empty pipe, then sleeps:

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: fs/pipe.c
 * lines: 546–571
 * function: pipe_write
 */
		/* Wait for buffer space to become available. */
		if (filp->f_flags & O_NONBLOCK) {
			if (!ret)
				ret = -EAGAIN;
			break;
		}
		if (signal_pending(current)) {
			if (!ret)
				ret = -ERESTARTSYS;
			break;
		}

		/*
		 * We're going to release the pipe lock and wait for more
		 * space. We wake up any readers if necessary, and then
		 * after waiting we need to re-check whether the pipe
		 * become empty while we dropped the lock.
		 */
		__pipe_unlock(pipe);
		if (was_empty)
			wake_up_interruptible_sync_poll(&pipe->rd_wait, EPOLLIN | EPOLLRDNORM);
		kill_fasync(&pipe->fasync_readers, SIGIO, POLL_IN);
		wait_event_interruptible_exclusive(pipe->wr_wait, pipe_writable(pipe));
		__pipe_lock(pipe);
		was_empty = pipe_empty(pipe->head, pipe->tail);
		wake_next_writer = true;
```

`pipe_writable` is the mirror of `pipe_readable`: the ring is not full,
or `readers` is zero. The exclusive wait wakes one sleeper to take the
space a reader just freed. `pipe_read` remembers `was_full` and, if it
frees a slot, wakes `wr_wait` with `EPOLLOUT | EPOLLWRNORM`.

The waiting task leaves the runqueue the same way a futex waiter does
in [Week 11](../../11-threads-and-concurrency/sections/day-003-mutexes-ownership-lock-order-futex.md).
Bytes already copied stay in the ring. A nonblocking write that has
copied nothing returns `-EAGAIN`; one that has copied some pages returns
that short count. `wait_event_interruptible_exclusive` rechecks the
predicate after publishing the waiter, which is the lost-wakeup close
from the condition-variable lesson. A full pipe is a consumer that is
behind. The `PIPE_MIN_DEF_BUFFERS` comment at the top of `fs/pipe.c`
names the other failure: a one-slot pipe blocks a writer that still
owes the pipe a token, which is how GNU make's jobserver uses one.

## 11. `PIPE_BUF` is an atomicity bound

The uapi header names the constant applications are told to rely on.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: include/uapi/linux/limits.h
 * lines: 5–17
 * function: (macros)
 */
#define NR_OPEN	        1024

#define NGROUPS_MAX    65536	/* supplemental group IDs are available */
#define ARG_MAX       131072	/* # bytes of args + environ for exec() */
#define LINK_MAX         127	/* # links a file may have */
#define MAX_CANON        255	/* size of the canonical input queue */
#define MAX_INPUT        255	/* size of the type-ahead buffer */
#define NAME_MAX         255	/* # chars in a file name */
#define PATH_MAX        4096	/* # chars in a path name including nul */
#define PIPE_BUF        4096	/* # bytes in atomic write to a pipe */
#define XATTR_NAME_MAX   255	/* # chars in an extended attribute name */
#define XATTR_SIZE_MAX 65536	/* size of an extended attribute value (64k) */
#define XATTR_LIST_MAX 65536	/* size of extended attribute namelist (64k) */
```

The comment says atomic write. It does not say message. `fs/pipe.c`
never compares a transfer against the identifier `PIPE_BUF`. The write
path holds `__pipe_lock` across the copy and drops it only to sleep in
section 10. Two writers cannot interleave while one holds the mutex. A
write that does not fit publishes what it copied, releases the mutex,
and continues after the wake, so the other writer can append between
the two halves.

Readers see bytes in order, with no marker where one `write` ended.
`AAAA` from one writer and `BBBB` from another can return as one `read`
of eight or as a `read` of three plus a `read` of five. A length
prefix, a newline, or some other frame has to be in the bytes if the
reader needs records.

`O_DIRECT` on the write end is a different mechanism. `is_packetized`
tests that flag, and a new buffer then gets `PIPE_BUF_FLAG_PACKET`
instead of `PIPE_BUF_FLAG_CAN_MERGE`. `pipe_read` returns a packet
buffer as one result and clears `buf->len`. Ordinary pipes merge small
writes onto the tail of the previous page. Merging is the opposite of
framing. Use `PIPE_BUF` when two racing writers must not cut a record
in half, keep each record at most that size, and still find the record
boundary in the bytes. Do not read the constant as "every `write`
becomes one `read`."

## 12. ToyOS has the queue and not the protocol

[Week 8](../../08-user-mode-syscalls-and-ipc/sections/day-006-process-fd-ipc.md)
implements `pipe_data[64]`. A write succeeds only when the whole
request fits. An empty read returns zero immediately. There is one
global pipe, fixed descriptors 3 and 4, no `fork`, no `dup2`, no wait
queue, no `EPIPE`, and no `SIGPIPE`. This lesson splits that zero into
`-EAGAIN` or a sleep while a writer exists, and `0` only after the last
writer releases the file.
[Week 9's VFS](../../09-filesystems-and-virtual-devices/sections/day-001-vfs-objects-and-ownership.md)
is why the queue hangs on an inode. `pipefs_dname` prints
`pipe:[inode]`, which is what `/proc/PID/fd/N` shows for either end.

The comparison worth keeping:

```text
ToyOS pipe                         this tree's pipe
64-byte linear buffer              ring of pages, default 16 slots
empty read returns 0 at once       empty read sleeps, or -EAGAIN, or 0 at EOF
no writer-gone state               writers hits 0 in pipe_release
no broken-pipe signal              send_sig(SIGPIPE) and -EPIPE
one global pipe, fds 3 and 4       two struct file objects, any free indexes
no second process                  fork copies the files, dup2 aims them
```

## 13. A pipeline small enough to trace by hand

The following is the protocol from sections 4, 6, and 7 in one process
tree. It is ordinary POSIX C. It does not open the kernel objects
except through the syscalls those sections implement.

```c
int r, w, fds[2], status;
char buf[8];
ssize_t n;
pid_t child;

if (pipe(fds) != 0)
    return 1;
r = fds[0];
w = fds[1];
child = fork();
if (child == 0) {
    close(w);
    if (dup2(r, STDIN_FILENO) < 0)
        _exit(127);
    close(r);
    n = read(STDIN_FILENO, buf, sizeof buf);
    /* n == 3 for the write below; a later read returns 0 */
    _exit(n < 0 ? 127 : 0);
}
close(r);
if (write(w, "abc", 3) != 3)
    return 1;
close(w);                 /* last write reference: child observes EOF */
if (waitpid(child, &status, 0) < 0)
    return 1;
```

`pipe` enters `do_pipe2`. `fork` duplicates the files. The child points
descriptor 0 at the read end and drops the write end. The parent drops
the read end, writes `"abc"`, and closes the write end. The child's
`read` returns 3, and a second `read` returns 0 because `pipe_release`
drove `writers` to 0. `waitpid` collects the child.

Leave the write end open in the child and also skip the parent's
`close(r)`, and `writers` stays nonzero after the parent closes `w`.
The child's second `read` sleeps. `strace` shows `read` not returning,
and `/proc/PID/fd` still lists the write-end `pipe:[inode]`. Close the
read end in the child before the parent writes, and the parent dies on
the default `SIGPIPE`. Ignore the signal and the same `write` returns
`-1` with `errno == EPIPE`.

## 14. Mastery: Explain · Draw · Observe · Build

The exercise is
[../challenges/day-004-pipes.sh](../challenges/day-004-pipes.sh).
Run it from the week directory after you can predict, for a blocking
pipe, what a `read` returns on empty-with-writer, empty-with-no-writer,
and a short user buffer.

### Explain

- Why the bytes of a pipe live in kernel pages rather than in either
  task's address space, and which two syscalls move them across the
  boundary.
- What `create_pipe_files` puts in `res[0]` and `res[1]`, and why
  `readers` and `writers` start at 1 even though `files` starts at 2.
- Why a parent that forks a consumer and forgets to close its own copy
  of the write end prevents the child from ever seeing end-of-file.
- What `pipe_write` does when `readers` is 0, and which disposition of
  `SIGPIPE` lets the caller observe `EPIPE`.
- Why two successful four-byte writes can come back as one eight-byte
  read, and what `PIPE_BUF` guarantees instead of a record boundary.

### Draw

Draw one `pipe_inode_info` with a ring of four slots, `head`, `tail`,
and the page each occupied slot points at. Mark `rd_wait` and `wr_wait`.
Under it, draw two tasks after `fork` and `dup2`: which descriptor
indexes share which `struct file`, and which `close` calls drop the
last reference. Then draw the full-pipe case: writer off the runqueue,
reader's `pipe_read` advancing `tail`, wakeup on `wr_wait`.

### Observe

Run the wrapper. Record `uname -r`, the compiler, and the tree id if
you opened the sources. In the output, point at the read that returns
0 and write down which close made `writers` hit 0. If you trace a
blocking writer against a slow reader, name the syscall that does not
return until the reader has consumed a slot.

### Build

Write a one-page account of a single byte in a two-process pipeline:
the `write` buffer, the pipe page and slot index, the `read` buffer,
the `close` that produces end-of-file, and the `waitpid` that reaps the
child. Include the sentence you will stand on in a report: `PIPE_BUF`
bounds interleaving of concurrent writers, and the byte stream still
has no frames unless the bytes themselves contain them.
