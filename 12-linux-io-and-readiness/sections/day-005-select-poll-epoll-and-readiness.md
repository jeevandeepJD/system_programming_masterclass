# Day 5 — `select`, `poll`, `epoll`, and Readiness

**Target time:** approximately 3 hours

> Readiness is a promise that the next I/O call on that descriptor can
> make progress now. It is not a count of bytes, and it is not a
> statement that the operation has finished.

## 1. Syntax needed for this source

`select` stores interest in bits. A shift walks the set, and a mask tests one descriptor.

```c
unsigned long interest = 0;
interest |= (1UL << fd);
for (int i = 0; interest != 0; i++, interest >>= 1) {
    if (interest & 1)
        ready++;
}
```

`1UL << fd` builds one bit. `interest & 1` asks whether the current descriptor is in the set. The loop is a scan, not a copy of the bytes behind the descriptor.

## 2. Linux 0.12 first: `select` scans, then sleeps

A task blocked in one `read` cannot notice a second source. Linux 0.12's `do_select` in `fs/select.c` scans the caller's bit sets and, when nothing is ready, sleeps. The scan starts on printed page 864 and continues on 865:

```c
repeat:
	wait_table.nr = 0;
	*inp = *outp = *exp = 0;
	count = 0;
	mask = 1;
	for (i = 0 ; i < NR_OPEN ; i++, mask += mask) {
		if (mask & in)
			if (check_in(&wait_table,current->filp[i]->f_inode)) {
				*inp |= mask;
				count++;
			}
```

`check_out` and `check_ex` repeat that bit test. After the scan, printed page 865:

```c
if (!(current->signal & ~current->blocked) &&
    (wait_table.nr || current->timeout) && !count) {
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	free_wait(&wait_table);
	goto repeat;
}
free_wait(&wait_table);
return count;
```

Trace. `mask` starts at 1 and doubles each descriptor (`mask += mask`), so each pass tests one fd. `check_in` looks at that inode and, the first time, records a wait. A ready descriptor sets the same bit in `*inp` and increments `count`. If `count` is still 0, no unblocked signal is pending, and either some wait was queued or the timeout remains, the task becomes `TASK_INTERRUPTIBLE` and `schedule` runs. On wake, `free_wait` releases the wait entries and `goto repeat` scans again. A nonzero `count`, a signal, or a timeout falls through and returns `count`.

What carries forward is "scan a set, sleep once, rescan after wake." The copied `fd_set` arguments, the `NR_OPEN` width, and the pipe/character/FIFO-only check earlier in the function belong to this kernel. Linux 0.12 has `select` and does not have `poll` or `epoll`. Linux 6.12 keeps `select` and adds `poll`'s caller-built list and `epoll`'s stored interest set, including level and edge trigger.

The shared [Linux 0.12 map](../../references/EARLY_LINUX_012_MAP.md) names this `select` path and stops before `poll` and `epoll`. The sections below are `select`, `poll`, and `epoll` in Linux 6.12, tree `adc218676eef`. Read 0.12 as the first complete scan-and-sleep. Develop against the 6.12 tree.

Day 4's pipe blocks inside one `read` or one `write`. That fits a task
with one source. It does not fit a task that must take the next byte
from whichever of several files becomes readable first. A thread blocked
in `pipe_read` on source A cannot observe source B. One thread per
source spends a stack, a `task_struct`, and a wakeup on each idle
source, the context-switch cost from
[Week 7](../../07-memory-processes-and-scheduling/sections/day-005-cooperative-yield-and-switch.md)
multiplied by sources that had nothing to say.

Readiness inverts the wait. The task sleeps once, on "which of these
files would not block," and the kernel wakes it when any changes. The
excerpts are Linux 6.12 at `adc218676eef`. `select`, `poll`, and
`epoll` differ in where the interest set lives between calls, and in
what the task does after it wakes.

## 3. Evolution: why the early design changed

Linux 0.12 `select` copies the caller's fd sets, scans each index, queues a
wait the first time, and if nothing is ready sleeps once and scans again.
The interest set dies when the syscall returns. There is no `poll` and no
`epoll`.

Scanning every descriptor on every wake does not scale, and copying the set
on every call does not either. The wait has to be registered with the file.
For a long-lived set, the registration has to outlive one syscall.

`do_select` (`fs/select.c` line 476) is still that scan. It builds a poll
table, checks each fd, and `poll_schedule_timeout` (line 234) sleeps.
`do_poll` (line 884) walks a `pollfd` list the caller already built and uses
the same sleep. The file's `poll` method, such as `pipe_poll`, records the
wait on the pipe's queue.

`epoll` keeps the interest set in the kernel. `ep_insert` (`fs/eventpoll.c`
line 1618) attaches an `epitem` and polls once. `ep_poll_callback` (line
1308) runs from the file's wakeup and links the item onto the ready list.
`ep_poll` (line 1966) waits on that list instead of scanning the fd table.
`EPOLLET` (`include/uapi/linux/eventpoll.h` line 69) and `EPOLLONESHOT`
(line 66) change when the item is reported again.

The result is still a hint. A descriptor that just woke the waiter can
return a short read, or `-EAGAIN`, before the next wait. Readiness is not a
count of bytes.

## 4. Why "readable" cannot mean "done"

ToyOS waits inside one pipe read. It has no descriptor set and no `select`. The Week 8 pipe is one queue; a second source needs a second blocked call.

A blocking `read` of a pipe has three honest results, from Day 4.
A positive count means that many bytes were copied. `0` means no
writer remains. `-EAGAIN`, on a nonblocking file, means the ring is
empty and a writer still exists. None of those results is known at the
moment the task decides to wait. The kernel can know a weaker fact: a
call made right now would not go to sleep.

`pipe_poll` computes that fact from the same counters `pipe_read` uses.
A read end contributes `EPOLLIN | EPOLLRDNORM` when the ring is not
empty, and `EPOLLHUP` when `writers` is zero and the writer's counter
has moved. A write end contributes `EPOLLOUT | EPOLLWRNORM` when the
ring is not full, and `EPOLLERR` when `readers` is zero. Those bits
mean "the corresponding syscall has a chance to return without
waiting." They do not mean "N bytes are buffered," and they do not mean
"the peer has finished."

A pipe with one byte is readable, and a `read` of 4096 bytes returns 1.
A pipe whose last writer has closed is also readable, and `read`
returns 0. A socket that just became writable can still return a short
count or `EAGAIN` if the window closes between the poll and the write.
The wakeup is a hint. The consuming `read` or `write` rechecks the
predicate, the same shape as the condition-variable loop in
[Week 11](../../11-threads-and-concurrency/sections/day-004-condition-variables-and-bounded-queues.md).

```text
readiness:  "a read/write/accept issued now can progress"
the call:   copies bytes, or returns 0, or returns EAGAIN, or errors
neither:    "the transfer of the size you hoped for has completed"
```

`epoll_wait` returning 1 means one ready-list entry was copied out.
The byte count is what the following `read` returns. Treating the wait
count as a byte count, or stopping after one `read`, is a different
protocol from the one this kernel implements.

## 5. Sleeping inside one file does not scale to a set

The syscall path is still the one from [Week 8](../../08-user-mode-syscalls-and-ipc/sections/day-004-int80-syscall-abi.md).
User registers enter the kernel, the kernel may set the task to
`TASK_INTERRUPTIBLE` and call the scheduler, and a later wakeup restores
those registers so the syscall can return. [Week 11's threads](../../11-threads-and-concurrency/sections/day-001-threads-processes-clone-stacks-tls.md)
do not change the shape. A second thread blocked in `read` is a second
task on a wait queue. That is a fine design when the threads have other
work and the blocking call is the synchronization. It is a poor design
when the threads exist only to wait, because each one pays a stack and
a wakeup for a source that is usually idle.

The three multiplexers below share one structure, visible in
`do_select` and `do_poll`:

1. Build a kernel-side description of the caller's interest.
2. Ask each file's `poll` method, which both reports the current bits
   and, the first time through, registers a wait-queue entry.
3. If any file is already ready, copy the result out and return.
4. Otherwise sleep until a wakeup or a timeout, then scan again.

`epoll` keeps the interest set and the wait-queue entries across calls,
so step 1 is an `epoll_ctl` done once, and the sleep in `epoll_wait`
waits on a single ready list instead of scanning every file.

## 6. `select` copies bitmaps, and the bitmap has a historical width

The uapi header in this tree fixes the width of the kernel's
`fd_set`-shaped object:

```c
/* tree adc218676eef (Linux 6.12)
 * path: include/uapi/linux/posix_types.h
 * lines: 7–27
 * function: (macros and __kernel_fd_set)
 */
/*
 * This allows for 1024 file descriptors: if NR_OPEN is ever grown
 * beyond that you'll have to change this too. But 1024 fd's seem to be
 * enough even for such "real" unices like OSF/1, so hopefully this is
 * one limit that doesn't have to be changed [again].
 *
 * Note that POSIX wants the FD_CLEAR(fd,fdsetp) defines to be in
 * <sys/time.h> (and thus <linux/time.h>) - but this is a more logical
 * place for them. Solved by having dummy defines in <sys/time.h>.
 */

/*
 * This macro may have been defined in <gnu/types.h>. But we always
 * use the one here.
 */
#undef __FD_SETSIZE
#define __FD_SETSIZE	1024

typedef struct {
	unsigned long fds_bits[__FD_SETSIZE / (8 * sizeof(long))];
} __kernel_fd_set;
```

`__FD_SETSIZE` is 1024. That is the historical userspace `fd_set` this
header is willing to name. `core_sys_select` does not allocate "a
1024-bit set" blindly. It sizes six bitmaps from the caller's `n`: one
incoming and one outgoing set for read, write, and exception. It copies
the incoming sets from user memory with `get_fd_set`, zeroes the result
sets, and only then calls `do_select`. If `n` is larger than the
process's `max_fds`, `n` is clamped to `max_fds` before that copy. The
kernel can therefore scan more than 1024 descriptors when the caller
passes a larger `n` and a large enough buffer. The 1024-wide type is
still what a program using `fd_set` and the `FD_SET` family can name
without writing past the object. Those are two different limits, and a
report has to say which one it means. Lines 647–680 of
`core_sys_select` are the copy itself: six regions sized by
`FDS_BYTES(n)`, a stack buffer when they fit and `kvmalloc` when they
do not, `get_fd_set` from the user pointers, then `do_select`.

Every `select` pays that copy in and, after `do_select` returns, a copy
of the result bits out. The interest set is not stored in the kernel
between calls. The caller rebuilds it, the kernel recopies it, and the
kernel walks it.

`do_select` walks the bits. A set bit becomes `fdget` plus `vfs_poll`.
The first time a file reports a requested condition, the result counter
increments and `wait->_qproc` is cleared so later files are not asked
to register a waiter that this call will throw away immediately.

```c
/* tree adc218676eef (Linux 6.12)
 * path: fs/select.c
 * lines: 524–554
 * function: do_select
 */
			for (j = 0; j < BITS_PER_LONG; ++j, ++i, bit <<= 1) {
				struct fd f;
				if (i >= n)
					break;
				if (!(bit & all_bits))
					continue;
				mask = EPOLLNVAL;
				f = fdget(i);
				if (fd_file(f)) {
					wait_key_set(wait, in, out, bit,
						     busy_flag);
					mask = vfs_poll(fd_file(f), wait);

					fdput(f);
				}
				if ((mask & POLLIN_SET) && (in & bit)) {
					res_in |= bit;
					retval++;
					wait->_qproc = NULL;
				}
				if ((mask & POLLOUT_SET) && (out & bit)) {
					res_out |= bit;
					retval++;
					wait->_qproc = NULL;
				}
				if ((mask & POLLEX_SET) && (ex & bit)) {
					res_ex |= bit;
					retval++;
					wait->_qproc = NULL;
				}
				/* got something, stop busy polling */
```

A closed or never-opened index hits `mask = EPOLLNVAL` because
`fdget` finds no file. That bit does not become a readable result. If
nothing is ready, no signal is pending, and the timeout has not fired,
`do_select` sleeps in `poll_schedule_timeout` with `TASK_INTERRUPTIBLE`
(lines 604–605). The
wait-queue entries were installed by the files' `poll` methods on the
first pass, through `poll_wait`, before any of them reported ready. On
wakeup the loop scans every bit again. The return value of `select` is
the number of bits set in the result sets, not the number of bytes
behind them. A return of 2 means two conditions, on one descriptor or
on two. The caller still has to `FD_ISSET` and then `read` or `write`. Every
call pays the copy and the scan, including the calls that find nothing
ready, and the bitmap has no place to hang a pointer except a side
array indexed by the same integer.

## 7. `poll` walks a list the caller already built

`poll` takes an array of `struct pollfd`: an fd, an events mask, and a
slot for revents. There is no `fd_set` and no `__FD_SETSIZE` in this
path. `do_sys_poll` rejects `nfds` above `RLIMIT_NOFILE`, copies the
array into a `poll_list` that starts on the stack and continues onto
kmalloc'd pages, then calls `do_poll`.

```c
/* tree adc218676eef (Linux 6.12)
 * path: fs/select.c
 * lines: 907–928
 * function: do_poll
 */
		for (walk = list; walk != NULL; walk = walk->next) {
			struct pollfd * pfd, * pfd_end;

			pfd = walk->entries;
			pfd_end = pfd + walk->len;
			for (; pfd != pfd_end; pfd++) {
				/*
				 * Fish for events. If we found one, record it
				 * and kill poll_table->_qproc, so we don't
				 * needlessly register any other waiters after
				 * this. They'll get immediately deregistered
				 * when we break out and return.
				 */
				if (do_pollfd(pfd, pt, &can_busy_loop,
					      busy_flag)) {
					count++;
					pt->_qproc = NULL;
					/* found something, stop busy polling */
					busy_flag = 0;
					can_busy_loop = false;
				}
			}
```

`do_pollfd` does `fdget`, calls `vfs_poll`, masks the result with the
requested events plus `EPOLLERR | EPOLLHUP`, and stores `revents`. A
negative fd is skipped and contributes an empty `revents`, which is how
a caller punches a hole in the array without compacting it. The sleep
at the bottom of `do_poll` is `poll_schedule_timeout` with
`TASK_INTERRUPTIBLE`, the same wait `do_select` uses. After the call,
`do_sys_poll` writes each `revents` back to the user array.

The list is rebuilt on every `poll`, and the kernel asks every file
until one is ready and `_qproc` is cleared. What `poll` buys is a denser
interest set: sparse descriptors are `pollfd`s with their own masks,
not a bitmap with a historical width. The scan is still linear in the
number of files.

## 8. `epoll` stores the interest set and wakes from a ready list

`epoll_ctl` is the call that edits the interest set. `do_epoll_ctl`
rejects a bad epoll descriptor or a bad target with `-EBADF`, and a
file whose operations cannot poll with `-EPERM`. The operation switch
is the whole interface:

```c
/* tree adc218676eef (Linux 6.12)
 * path: fs/eventpoll.c
 * lines: 2262–2287
 * function: do_epoll_ctl
 */
	error = -EBADF;
	f = fdget(epfd);
	if (!fd_file(f))
		goto error_return;

	/* Get the "struct file *" for the target file */
	tf = fdget(fd);
	if (!fd_file(tf))
		goto error_fput;

	/* The target file descriptor must support poll */
	error = -EPERM;
	if (!file_can_poll(fd_file(tf)))
		goto error_tgt_fput;

	/* Check if EPOLLWAKEUP is allowed */
	if (ep_op_has_event(op))
		ep_take_care_of_epollwakeup(epds);

	/*
	 * We have to check that the file structure underneath the file descriptor
	 * the user passed to us _is_ an eventpoll file. And also we do not permit
	 * adding an epoll file descriptor inside itself.
	 */
	error = -EINVAL;
	if (fd_file(f) == fd_file(tf) || !is_file_epoll(fd_file(f)))
```

`ADD` of a descriptor already in the tree returns `-EEXIST`. `DEL` or
`MOD` of one that is absent returns `-ENOENT`. `ADD` forces
`EPOLLERR | EPOLLHUP` into the mask, so the caller cannot suppress
hangup and error by forgetting them. The lookup key is the pair
`(struct file *, fd)`, compared by `ep_cmp_ffd`. Two descriptor numbers
that refer to different files are different items. The same file under
two descriptor numbers is also two items, because the fd is part of the
key.

`ep_insert` allocates an `epitem`, links it through `file->f_ep`
(lines 1601–1609),
and inserts it into the epoll instance's red-black tree. It then polls
once, which is what installs the callback:

```c
/* tree adc218676eef (Linux 6.12)
 * path: fs/eventpoll.c
 * lines: 1691–1711
 * function: ep_insert
 */
	/* Initialize the poll table using the queue callback */
	epq.epi = epi;
	init_poll_funcptr(&epq.pt, ep_ptable_queue_proc);

	/*
	 * Attach the item to the poll hooks and get current event bits.
	 * We can safely use the file* here because its usage count has
	 * been increased by the caller of this function. Note that after
	 * this operation completes, the poll callback can start hitting
	 * the new item.
	 */
	revents = ep_item_poll(epi, &epq.pt, 1);

	/*
	 * We have to check if something went wrong during the poll wait queue
	 * install process. Namely an allocation for a wait queue failed due
	 * high memory pressure.
	 */
	if (unlikely(!epq.epi)) {
		ep_remove_safe(ep, epi);
		return -ENOMEM;
```

`ep_item_poll` calls `vfs_poll`. For a pipe that is `pipe_poll`.
`poll_wait` calls the poll table's `_qproc` when one is set. The qproc
from `ep_insert` is `ep_ptable_queue_proc`, which points an
`eppoll_entry` at `ep_poll_callback` and adds it to the file's wait
queue. A pipe read end passes `&pipe->rd_wait`, the queue Day 4's
`pipe_write` wakes. If the file is already ready, the item is linked
onto `rdllist` and any sleeper on `ep->wq` is woken. The red-black tree
is the interest set. `rdllist` is what a callback has marked and
`epoll_wait` has not finished with.

`do_epoll_wait` checks the caller's buffer, rejects a descriptor that
is not an epoll file, and then sleeps inside `ep_poll`:

```c
/* tree adc218676eef (Linux 6.12)
 * path: fs/eventpoll.c
 * lines: 2424–2449
 * function: do_epoll_wait
 */
static int do_epoll_wait(int epfd, struct epoll_event __user *events,
			 int maxevents, struct timespec64 *to)
{
	int error;
	struct fd f;
	struct eventpoll *ep;

	/* The maximum number of event must be greater than zero */
	if (maxevents <= 0 || maxevents > EP_MAX_EVENTS)
		return -EINVAL;

	/* Verify that the area passed by the user is writeable */
	if (!access_ok(events, maxevents * sizeof(struct epoll_event)))
		return -EFAULT;

	/* Get the "struct file *" for the eventpoll file */
	f = fdget(epfd);
	if (!fd_file(f))
		return -EBADF;

	/*
	 * We have to check that the file structure underneath the fd
	 * the user passed to us _is_ an eventpoll file.
	 */
	error = -EINVAL;
	if (!is_file_epoll(fd_file(f)))
```

`ep_poll` sets `TASK_INTERRUPTIBLE`, rechecks `ep_events_available`
under `ep->lock`, and only then calls `__add_wait_queue_exclusive` on
`ep->wq` (lines 2055–2057). That is the lost-wakeup close: recheck
under the waker's lock before sleeping. A signal already pending
returns `-EINTR`. The callback runs in the waker's context and must not
sleep, which is why the comment at the top of `fs/eventpoll.c` makes
`ep->lock` a rwlock. The usual path links the item if it is not already
linked, then wakes `ep->wq`:

```c
/* tree adc218676eef (Linux 6.12)
 * path: fs/eventpoll.c
 * lines: 1339–1352
 * function: ep_poll_callback
 */
	/*
	 * If we are transferring events to userspace, we can hold no locks
	 * (because we're accessing user memory, and because of linux f_op->poll()
	 * semantics). All the events that happen during that period of time are
	 * chained in ep->ovflist and requeued later on.
	 */
	if (READ_ONCE(ep->ovflist) != EP_UNACTIVE_PTR) {
		if (chain_epi_lockless(epi))
			ep_pm_stay_awake_rcu(epi);
	} else if (!ep_is_linked(epi)) {
		/* In the usual case, add event to ready list. */
		if (list_add_tail_lockless(&epi->rdllink, &ep->rdllist))
			ep_pm_stay_awake_rcu(epi);
	}
```

If `epoll_wait` is copying events out, `ovflist` is the overflow chain.
Events that arrive in that window are queued there and reattached,
so a callback during `copy_to_user` does not lose the edge.

```text
interest (RB tree of epitem)          ready (rdllist)
  built by epoll_ctl ADD                built by ep_poll_callback
  survives across epoll_wait            consumed by ep_send_events
  one node per (file, fd)               a node is linked only when ready
```

## 9. Trace: pipe write, wakeup, `epoll_wait`, read

One byte is written to a pipe whose read end is already in an epoll set
with `EPOLLIN`. Registration already happened. `epoll_ctl(ADD)` reached `ep_insert`,
which polled the pipe. At lines 657–674 `pipe_poll` stores `poll_usage`
and calls `poll_wait` on `rd_wait` before it reads `head` and `tail`,
so a wake between the test and the sleep still finds a waiter. The
waiter, if the ring was empty, is `TASK_INTERRUPTIBLE` on `ep->wq`, not
on the pipe. Its registers sit at the syscall boundary from
[Week 8](../../08-user-mode-syscalls-and-ipc/sections/day-004-int80-syscall-abi.md).
The core can run the writer.

The writer enters `pipe_write`, copies the byte into a pipe page, and
wakes the read-side queue. The wake is unconditional once `poll_usage`
is set, even if the pipe was not empty:

```c
/* tree adc218676eef (Linux 6.12)
 * path: fs/pipe.c
 * lines: 589–602
 * function: pipe_write
 */
	/*
	 * If we do do a wakeup event, we do a 'sync' wakeup, because we
	 * want the reader to start processing things asap, rather than
	 * leave the data pending.
	 *
	 * This is particularly important for small writes, because of
	 * how (for example) the GNU make jobserver uses small writes to
	 * wake up pending jobs
	 *
	 * Epoll nonsensically wants a wakeup whether the pipe
	 * was already empty or not.
	 */
	if (was_empty || pipe->poll_usage)
		wake_up_interruptible_sync_poll(&pipe->rd_wait, EPOLLIN | EPOLLRDNORM);
```

`wake_up_interruptible_sync_poll` walks `rd_wait`. The entry installed
for epoll has function `ep_poll_callback`. The callback links the
`epitem` onto `rdllist` and calls `wake_up(&ep->wq)`. That second wake
is the one that makes the `epoll_wait` task runnable. The scheduler
restores its registers. `ep_poll` reaches `ep_send_events`, which polls
the pipe once more and copies an `epoll_event` to the user buffer. The
syscall returns 1.

Only now does the user task call `read` on the pipe. That is `pipe_read`.
It copies however many bytes are in the ring, up to the buffer it was
given, and returns that count. If the writer has closed and the ring is
empty, it returns 0. If another task drained the byte between the
callback and the `read`, a nonblocking read returns `-EAGAIN`. The
ready event was still a true statement about the past. It was not a
reservation of the byte.

```text
pipe_write
  copy byte into pipe page
  wake_up rd_wait          ---> ep_poll_callback
                                 link epitem on rdllist
                                 wake_up ep->wq
                                      |
epoll_wait task                        v
  was TASK_INTERRUPTIBLE on ep->wq
  ep_send_events copies EPOLLIN to user
  syscall returns 1
user read()
  pipe_read copies the byte, returns the count
```

A trace that stops at `epoll_wait` returning 1 has not observed the
byte. A trace that stops at `read` returning 1 has.

## 10. Level trigger, edge trigger, and the `EAGAIN` drain

After an event is copied out, `ep_send_events_proc` decides whether the
item stays interesting. Lines 1750–1762 re-poll with a null qproc, so
this poll observes and does not register another waiter, then
`__put_user` the event. A failed `put_user` links the item back and can
return `-EFAULT`. The branch after a successful copy is the
level-versus-edge rule:

```c
/* tree adc218676eef (Linux 6.12)
 * path: fs/eventpoll.c
 * lines: 1885–1897
 * function: ep_send_events
 */
		if (epi->event.events & EPOLLONESHOT)
			epi->event.events &= EP_PRIVATE_BITS;
		else if (!(epi->event.events & EPOLLET)) {
			/*
			 * If this file has been added with Level
			 * Trigger mode, we need to insert back inside
			 * the ready list, so that the next call to
			 * epoll_wait() will check again the events
			 * availability. At this point, no one can insert
			 * into ep->rdllist besides us. The epoll_ctl()
			 * callers are locked out by
			 * ep_send_events() holding "mtx" and the
			 * poll callback will queue them in ep->ovflist.
```

Default mode is level trigger: `EPOLLET` is absent, so the item goes
back on `rdllist`. The next `epoll_wait` reports it again until a poll
shows the condition gone. A short `read` is fine. The following wait
notices the leftover bytes.

`EPOLLET` does not requeue. The next report requires a new callback.
`pipe_write` wakes on every write once `poll_usage` is set, so a later
write is a new edge. A short `read` that leaves bytes, with no further
write, produces none. `epoll_wait` sleeps and the bytes sit in the
ring. The repair is to read until the file would block, which means
`O_NONBLOCK` so the drain sees `-EAGAIN` instead of sleeping inside
`pipe_read` while another descriptor in the set is ready. `-EAGAIN` is
"drained." `0` is "last writer closed." The loop stops on `EAGAIN` and
returns to `epoll_wait`. It ends the connection on `0`.

`EPOLLONESHOT` clears the interest bits, leaving `EP_PRIVATE_BITS`
(`EPOLLWAKEUP`, `EPOLLONESHOT`, `EPOLLET`, `EPOLLEXCLUSIVE`).
`ep_poll_callback` treats a mask with no poll bits as disabled until
`EPOLL_CTL_MOD` puts `EPOLLIN` back. Forgetting the `MOD` looks like an
edge-triggered stall: `epoll_wait` stays quiet, and the pipe still
holds bytes.

## 11. Close drops the interest when the file's last reference drops

The epoll instance does not own the caller's descriptor number. As
`ep_insert`'s comment says, the caller already raised the file's usage
count. When that file's last reference drops, `__fput` in
`fs/file_table.c` calls `eventpoll_release` before the file's own
`release` (the call is line 422; `f_op->release` is lines 430–431).
`eventpoll_release` (`include/linux/eventpoll.h` lines 34–46) returns if
`file->f_ep` is clear. Otherwise `eventpoll_release_file` walks the links and
`ep_remove`s each item. A `dup` keeps the `struct file`, so the
`epitem` remains, still keyed by the fd passed to `EPOLL_CTL_ADD`.
Closing the epoll file itself is `ep_eventpoll_release`, a different
path that frees the interest set because the instance is going away.

`EPOLL_CTL_DEL` drops one `(file, fd)` key while the file stays open.
Closing the last descriptor that refers to the file also drops the
interest, because `__fput` runs. Closing a duplicate, while another
descriptor still refers to the same file, does not. Adding the same key
twice returns `-EEXIST`. After close-and-reuse, the new `struct file`
does not match the old item: `ep_cmp_ffd` compares the file pointer
first.

## 12. A loop that matches the kernel's rule

This is the edge-triggered pipe reader the branch in section 10
describes. Level trigger can stop after one `read`. Edge trigger
cannot.

```c
struct epoll_event ev, out;
int ep, n;
char buf[256];

if (fcntl(rfd, F_SETFL, O_NONBLOCK) < 0)
    return -1;
ep = epoll_create1(EPOLL_CLOEXEC);
if (ep < 0)
    return -1;
ev.events = EPOLLIN | EPOLLET;
ev.data.fd = rfd;
if (epoll_ctl(ep, EPOLL_CTL_ADD, rfd, &ev) < 0)
    return -1;

for (;;) {
    n = epoll_wait(ep, &out, 1, -1);
    if (n < 0) {
        if (errno == EINTR)
            continue;
        return -1;
    }
    /* n == 1 means "this fd can progress", not "1 byte is waiting". */
    for (;;) {
        ssize_t r = read(rfd, buf, sizeof buf);
        if (r > 0)
            continue;             /* r bytes; keep draining */
        if (r == 0)
            return 0;             /* last writer released the file */
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN)
            break;                /* level is gone; wait for the next edge */
        return -1;
    }
}
```

A writer puts 300 bytes into an empty pipe and closes. `epoll_wait`
returns 1. The reads return 256, then 44, then 0, and the loop ends.
If the writer stays open, the third `read` returns `-1`/`EAGAIN`, the
inner loop breaks, and `epoll_wait` sleeps until the next write. Forget
`O_NONBLOCK` and that third `read` blocks inside `pipe_read`, so a
second pipe in the same set is ignored until it returns. `select` on
the same pipe is still the copy and the scan. A positive result still
allows a short `read`. Timeout zero sets `timed_out` up front and does
not sleep.

## 13. Mastery: Explain · Draw · Observe · Build

The exercise is
[../challenges/day-005-readiness.sh](../challenges/day-005-readiness.sh).
Predict, before you run it, which call returns a count of ready
conditions and which later call returns a count of bytes.

### Explain

- Why a task blocked in `pipe_read` on one descriptor cannot observe a
  second descriptor, and what one shared sleep replaces.
- What a readiness bit promises about the next `read` or `write`, and
  which two pipe results (`0` and `-EAGAIN`) a readable pipe can still
  produce.
- Why every `select` copies bitmaps and scans them, and which object in
  this tree is 1024 descriptors wide.
- Where an epoll interest set lives between `epoll_wait` calls, and
  which function links an item onto `rdllist`.
- Why an `EPOLLET` reader that stops after one short `read` can sleep
  in `epoll_wait` with bytes still in the pipe, and what `O_NONBLOCK`
  plus `EAGAIN` changes about that loop.

### Draw

Draw three columns for one pipe read end: `select`'s bit in a copied
bitmap, `poll`'s `pollfd`, and epoll's `epitem` in the red-black tree
plus a second box for `rdllist`. Then draw the section 9 trace as five
arrows: `pipe_write`, `rd_wait`, `ep_poll_callback`, `ep->wq`,
`read`. Mark which arrow carries a byte count and which arrows carry
only a wake.

### Observe

Run the wrapper. Record `uname -r` and the tree id if you opened
sources. In the trace, point at `epoll_wait`'s return and at the
following `read`'s return, and write both numbers down. If you have a
level-triggered run and an edge-triggered run, say which one reports
the pipe again after a short read with no further write.

### Build

Write the drain loop from section 12 in your own names and annotate each
return: ready-count, byte-count, end-of-file, and would-block. State
the `EPOLLONESHOT` rule in one sentence: delivery clears the interest
bits until `EPOLL_CTL_MOD` puts them back. Keep Day 4's pipe ring in
the picture, so the ready event and the byte never get collapsed into
one number.
