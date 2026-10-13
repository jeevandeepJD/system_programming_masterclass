# Day 7 — I/O Debugging and the Comparison Report

**Target time:** approximately 3 hours

> A stuck `read`, a vanished writer, and a `SIGBUS` on a load are three
> different layers failing. The report names the layer that produced
> each fact, then walks one failure from the symptom to a regression
> check.

## 1. Syntax needed for this source

A kernel print fills a buffer the kernel owns, then displays that buffer. The return is how many characters were formatted.

```c
char buf[64];
int n = format_into(buf, "fd %d", fd);
if (n < 0)
    return n;
show(buf);
```

`buf` is a pointer to the first character. `format_into` writes through it. `show` walks the characters until it has displayed `n` of them. The user program still cannot load the kernel object by indexing its own memory.

## 2. Linux 0.12 first: the whole source, `printk`, Bochs, and GDB

The 0.12 tree is small enough to read the path you are debugging and to print from inside it. `printk` in `kernel/printk.c` formats into a static buffer and hands that buffer to the console. The listing starts on printed page 438; the function body is on printed page 439:

```c
int printk(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i=vsprintf(buf,fmt,args);
	va_end(args);
	console_print(buf);
	return i;
}
```

Trace. `va_start` walks the arguments after `fmt`. `vsprintf` writes the formatted text into the file-scope `buf[1024]` and returns the length. `console_print` displays that kernel buffer. A `printk` you add beside `sys_read` or `read_pipe` shows the fd, the count, or `f_pos` on the console because those objects are in the same address space as the call. Chapter 17.5 (printed page 1065) runs that kernel under Bochs: `bochsdbg -q -f bochsrc-0.12-hd.bxrc` stops with the CPU at the reset vector, and `vbreak` / `lbreak` / `pbreak` plant instruction breakpoints. Chapter 17.11 (printed page 1094) rebuilds Bochs with `gdbstub` and the kernel with `-g`, then attaches GDB to port 1234 so the same stop is a C stack rather than a segment:offset.

What carries forward is "name the layer, then stop the machine on that layer." The static `buf`, `console_print`, the 80386 debugger commands, and a kernel you recompile for one `printk` belong to this tree. Linux 6.12 is too large, and the kernel you boot is often one you cannot rebuild for a question. The same facts come out through `/proc/self/fd` and `/proc/self/fdinfo`, and through `strace` recording the syscall and its return. GDB still stops the task; it attaches to the process, not to a Bochs stub.

The shared [Linux 0.12 map](../../references/EARLY_LINUX_012_MAP.md) is the early control flow those prints would confirm. The sections below are `/proc`, `strace`, and GDB on Linux 6.12, tree `adc218676eef`. Read 0.12 as the first complete kernel you can print and stop. Develop against the 6.12 tree.

Days 4–6 put a byte queue behind two descriptors, a readiness wait in
front of those descriptors, and a page-cache page behind a virtual
address. None of those objects is a C variable the program can print.
`/proc`, strace, and GDB ask the kernel, or the stopped task, to
describe them. Each answer stops at a different layer. A transcript
with no kernel identity and no binary identity is a story about a
different machine.

The line-level claims below are from tree `adc218676eef`, a 6.12
kernel. Before you repeat a line number, record `uname -r` and the tree
id you actually opened. [Week 10's report](../../10-linux-processes-and-signals/sections/day-007-proc-strace-gdb-comparison-report.md)
used the same rule for `task_struct`. It applies here to `struct file`,
the pipe ring, and the VMA.

## 3. Evolution: why the early design changed

Linux 0.12 answers "what is this descriptor doing" by a `printk` you compile
in, or by stopping the 80386 image in Bochs or GDB. The kernel, the console,
and the debugger share one machine. A `printk` beside `sys_read` can name
the fd because the object is in the same address space as the call.

That does not survive a kernel you do not rebuild, or a question about one
task on a running system. The evidence has to be exported in a form a
process can read, and a failure has to come back as a structured errno
rather than a line on the console.

`/proc/<pid>/fdinfo` is generated when it is read. `seq_show` in
`fs/proc/fd.c` (lines 23–72) locks the task, looks the index up with
`files_lookup_fd_locked`, and prints `f_pos`, the flags, the mount id, and
the inode number. `close_on_exec` (now taking `files_struct`, lines 99–102
of `include/linux/fdtable.h`) folds `O_CLOEXEC` into that flags word. The
symlink `/proc/<pid>/fd/N` is `proc_fd_link` (lines 175–196), which copies
`f_path`. A pipe renders through `pipefs_dname` (`fs/pipe.c` lines 870–873)
as `pipe:[inode]`.

`strace` records the syscall and the return because ptrace stops on entry
and exit. The kernel side of that class of hook is `trace_sys_enter`
(`kernel/entry/common.c` line 61), which ftrace, perf, and eBPF can attach
to without a private `printk`. GDB still stops the task. It attaches to the
process.

The return value stayed an errno-shaped `ssize_t`. `EBADF`, `EPIPE`, and
`EAGAIN` are evidence about which layer refused the call. A console line is
not a substitute for that code.

## 4. Why the descriptor table has to be asked for

ToyOS is the small source you can still read outright. [`vfs.c`](../../projects/toyos/src/vfs.c) and the Week 8 serial line show a descriptor and a pipe. They do not show `/proc`, `strace`, or a page cache.

The integer a program stores in `int fd` is an index. The `struct file`,
the pipe's `head` and `tail`, and the VMA's file offset live in kernel
memory. [Week 9](../../09-filesystems-and-virtual-devices/sections/day-003-user-fd-syscalls.md)
is why a user load of that integer cannot observe them. The kernel
builds a directory of symlinks at `/proc/PID/fd` and a text file per
descriptor at `/proc/PID/fdinfo` when something reads them. Two reads
are two generations. A task can `close` and `dup` between them.

`fs/proc/fd.c` generates the fdinfo text. `seq_show` locks the task,
looks the descriptor up, and copies out the position and the flags.

```c
/* tree adc218676eef (Linux 6.12)
 * path: fs/proc/fd.c
 * lines: 34–60
 * function: seq_show
 */
	task_lock(task);
	files = task->files;
	if (files) {
		unsigned int fd = proc_fd(m->private);

		spin_lock(&files->file_lock);
		file = files_lookup_fd_locked(files, fd);
		if (file) {
			f_flags = file->f_flags;
			if (close_on_exec(fd, files))
				f_flags |= O_CLOEXEC;

			get_file(file);
			ret = 0;
		}
		spin_unlock(&files->file_lock);
	}
	task_unlock(task);
	put_task_struct(task);

	if (ret)
		return ret;

	seq_printf(m, "pos:\t%lli\nflags:\t0%o\nmnt_id:\t%i\nino:\t%lu\n",
		   (long long)file->f_pos, f_flags,
		   real_mount(file->f_path.mnt)->mnt_id,
		   file_inode(file)->i_ino);
```

`O_NONBLOCK` and `O_CLOEXEC` are visible here because they live in
`f_flags` and the close-on-exec bitmap. A pipe that was supposed to be
nonblocking, and whose fdinfo flags do not include the nonblocking bit,
will block inside `pipe_read` on an edge-triggered drain. That is a
Day 5 bug you can see without guessing.

After the position line, the same function prints the inode number and
optionally calls the file's own `show_fdinfo`. Pipes in this tree do not
set that operation. `pipefifo_fops` (`fs/pipe.c` lines 1232–1241) has
`read_iter`, `write_iter`, `poll`, `release`, and `splice_write`, and no
`show_fdinfo`. The `if` is false, and fdinfo for a pipe stops at
position, flags, mount id, inode number, and whatever `show_fd_locks`
printed.

```c
/* tree adc218676eef (Linux 6.12)
 * path: fs/proc/fd.c
 * lines: 62–72
 * function: seq_show
 */
	/* show_fd_locks() never dereferences files, so a stale value is safe */
	show_fd_locks(m, file, files);
	if (seq_has_overflowed(m))
		goto out;

	if (file->f_op->show_fdinfo)
		file->f_op->show_fdinfo(m, file);

out:
	fput(file);
	return 0;
```

The symlink at `/proc/PID/fd/N` is not a stored path. `proc_fd_link`
takes a reference on the task's file and copies `f_path`. `d_path` then
renders it. For a pipe, the dentry operation `pipefs_dname` prints
`pipe:[inode]`. Both ends of one pipe share that inode, so both
symlinks print the same `pipe:[…]` text. That shared text is how you
prove two descriptors are the two ends, or that a child inherited the
end you thought you had closed.

```c
/* tree adc218676eef (Linux 6.12)
 * path: fs/proc/fd.c
 * lines: 175–196
 * function: proc_fd_link
 */
static int proc_fd_link(struct dentry *dentry, struct path *path)
{
	struct task_struct *task;
	int ret = -ENOENT;

	task = get_proc_task(d_inode(dentry));
	if (task) {
		unsigned int fd = proc_fd(d_inode(dentry));
		struct file *fd_file;

		fd_file = fget_task(task, fd);
		if (fd_file) {
			*path = fd_file->f_path;
			path_get(&fd_file->f_path);
			ret = 0;
			fput(fd_file);
		}
		put_task_struct(task);
	}

	return ret;
}
```

```c
/* tree adc218676eef (Linux 6.12)
 * path: fs/pipe.c
 * lines: 867–874
 * function: pipefs_dname
 */
/*
 * pipefs_dname() is called from d_path().
 */
static char *pipefs_dname(struct dentry *dentry, char *buffer, int buflen)
{
	return dynamic_dname(buffer, buflen, "pipe:[%lu]",
				d_inode(dentry)->i_ino);
}
```

`readlink` of the symlink is a syscall, like the status file in the
Week 10 lesson. It shows the file that was installed at that index at
the moment `fget_task` ran. It does not show `pipe->head`, the unread
byte count, or whether a task is sleeping on `rd_wait`. Those facts
need a trace, or `FIONREAD` on the pipe, which `pipe_ioctl` in
`fs/pipe.c` computes by summing `buf->len` under the pipe mutex
(lines 610–623). fdinfo's `pos` for a pipe is not that sum.
`stream_open` gave the pipe no shared file position that a seek could
use.

## 5. strace sees the syscall, not the ring

strace stops the task at syscall entry and exit through ptrace. The
line it prints is the architecture's registers at that boundary: the
number, the arguments, and the return value. [Week 8](../../08-user-mode-syscalls-and-ipc/sections/day-004-int80-syscall-abi.md)
is which register holds the number on the educational path, and the
same idea holds for the vDSO `syscall` instruction on a modern x86-64
process: the number and the arguments are in the ABI registers, and
the return value comes back in the register the ABI uses for a
function result. strace decodes those registers. It does not decode
`pipe->writers`.

A `write` line that returns a positive count is Day 4's short-or-full
copy. A `write` line that never returns is a task sleeping in
`pipe_write`'s `wait_event_interruptible_exclusive` on `wr_wait`, which
is backpressure, not a lost byte. An `epoll_wait` line that returns 1
is one ready event. The byte count is the next `read` line. Collapsing
those two numbers is the Day 5 mistake, and strace is the tool that
keeps them on separate lines.

What strace will not show is a load or a store. `mmap` appears.
`msync` appears. The store that faults does not appear as a syscall.
The next thing in the trace, if the fault is fatal, is the signal.
That absence is evidence. A report that says "the read failed" when
the trace shows no `read` after `mmap` has named the wrong layer.

GDB stops on the signal or on a breakpoint and can print the fault
address and the instruction pointer. The instruction pointer is in the
user program, at the load or store, which is the retry point from
Day 6. GDB does not print `i_size` unless you have kernel symbols and
a kernel target. For this week's userspace labs, GDB answers "which
instruction faulted and at which address." The source answers "why
that address is past the file."

## 6. Five results that get swapped

These are the failures the labs actually produce. Each one has a
generator in the tree from Days 4–6.

**`EBADF`.** The descriptor index does not name a `struct file` in
this task. `do_pipe2` never installed it, or `close` already released
it, or the index was never returned. `ksys_mmap_pgoff` returns
`-EBADF` when `fget` fails (`mm/mmap.c` line 515). `do_epoll_wait`
returns `-EBADF` when `fdget` on the epoll descriptor fails
(`fs/eventpoll.c` lines 2440–2442). `do_epoll_ctl` does the same for
the epoll fd and the target fd (lines 2262–2270), and returns `-EPERM`
when the target file has no poll operation (lines 2273–2275). A trace
that shows `epoll_ctl` failing with `EPERM` is a file that cannot be
waited on, not a readiness bug. A trace that shows `EBADF` is a
lifetime bug in the index: use after close, or the wrong integer.

**`EPIPE` and `SIGPIPE`.** Day 4's `pipe_write` posts both when
`readers` is zero: `send_sig(SIGPIPE, current, 0)` and `-EPIPE`
(`fs/pipe.c` lines 457–460). The default disposition kills the writer
on the way back to user mode, so the C statement after `write` does not
run and strace shows the signal rather than a handled errno. Ignoring
or blocking `SIGPIPE` lets `write` return `-1` with `EPIPE`. A partial
copy that then discovers the last reader has gone still posts the
signal, and it keeps the positive count when some bytes were already
written (lines 498–502). The report has to say which of those two
returns it observed. "Broken pipe" as a shell phrase is not yet either
of them.

**`EAGAIN`.** A nonblocking `pipe_read` or `pipe_write` returns
`-EAGAIN` when the ring is empty or full and the other side still
exists (Day 4, `pipe_read` lines 356–360 and `pipe_write` lines
557–561). `epoll_wait` timing out returns 0, not `EAGAIN`. `EAGAIN`
from a nonblocking `read` during an `EPOLLET` drain means "the level
is gone; go back to `epoll_wait`." Treating it as a fatal error drops
the connection. Treating `epoll_wait`'s 0-timeout 0 as `EAGAIN` is the
same confusion in the other direction.

**epoll misuse.** `EPOLL_CTL_ADD` of a key that is already present
returns `-EEXIST`. `DEL` or `MOD` of a missing key returns `-ENOENT`
(`do_epoll_ctl` lines 2358–2383). `EPOLLET` does not put the item back
on `rdllist` (`ep_send_events` lines 1885–1897), so a short read
with no later wakeup leaves bytes in the pipe and the waiter asleep.
`EPOLLONESHOT` clears the interest bits until `EPOLL_CTL_MOD`. Closing
the last reference to a watched file runs `eventpoll_release` from
`__fput` (Day 5). Closing a `dup` while another descriptor keeps the
file does not. A stall whose strace shows one `read` and then
`epoll_wait`, on a pipe whose `FIONREAD` is still nonzero, matches the
edge-triggered arm. A stall whose fdinfo lacks `O_NONBLOCK` matches the
drain that blocked inside `read` instead of returning `EAGAIN`.

**`SIGBUS` on a mapped file.** This is not an errno. Day 6's
`filemap_fault` returns `VM_FAULT_SIGBUS` when the page index is past
`i_size` (`mm/filemap.c` lines 3313–3315, and again under the folio lock
at lines 3417–3421). x86 turns that code into a signal:

```c
/* tree adc218676eef (Linux 6.12)
 * path: arch/x86/mm/fault.c
 * lines: 910–949
 * function: do_sigbus
 */
static void
do_sigbus(struct pt_regs *regs, unsigned long error_code, unsigned long address,
	  vm_fault_t fault)
{
	/* Kernel mode? Handle exceptions or die: */
	if (!user_mode(regs)) {
		kernelmode_fixup_or_oops(regs, error_code, address,
					 SIGBUS, BUS_ADRERR, ARCH_DEFAULT_PKEY);
		return;
	}

	/* User-space => ok to do another page fault: */
	if (is_prefetch(regs, error_code, address))
		return;

	sanitize_error_code(address, &error_code);

	if (fixup_vdso_exception(regs, X86_TRAP_PF, error_code, address))
		return;

	set_signal_archinfo(address, error_code);

#ifdef CONFIG_MEMORY_FAILURE
	if (fault & (VM_FAULT_HWPOISON|VM_FAULT_HWPOISON_LARGE)) {
		struct task_struct *tsk = current;
		unsigned lsb = 0;

		pr_err(
	"MCE: Killing %s:%d due to hardware memory corruption fault at %lx\n",
			tsk->comm, tsk->pid, address);
		if (fault & VM_FAULT_HWPOISON_LARGE)
			lsb = hstate_index_to_shift(VM_FAULT_GET_HINDEX(fault));
		if (fault & VM_FAULT_HWPOISON)
			lsb = PAGE_SHIFT;
		force_sig_mceerr(BUS_MCEERR_AR, (void __user *)address, lsb);
		return;
	}
#endif
	force_sig_fault(SIGBUS, BUS_ADRERR, (void __user *)address);
}
```

`do_user_addr_fault` calls `do_sigbus` when the fault code includes
`VM_FAULT_SIGBUS` (lines 1444–1446). For a user-mode fault that is not a
hardware-poison case, the signal is `SIGBUS` with `BUS_ADRERR` and the
fault address. The default disposition terminates the process. A
handler that returns resumes the faulting instruction, because a page
fault returns to the instruction pointer the trap saved, which Day 6
traced. If the VMA is still there and the file is still short, the
instruction faults again. A handler that returns without changing the
mapping or the file size livelocks the task in that fault. The safe
description for this week's labs is: record the address, treat the
access as fatal, and do not return into the faulting instruction
unless the program has already removed the cause, for example by
`munmap` in a handler that then long-jumps to a recovery path it
designed. Catching `SIGBUS` and ignoring it is not recovery. A load of
an address that `munmap` already removed is a different signal: there
is no VMA, and `do_user_addr_fault` takes `bad_area_nosemaphore` before
`filemap_fault`.

## 7. One failure, all the way through

The shape is fixed. Symptom, hypothesis, evidence, root cause, fix,
regression. The evidence column names its layer: source, API, trace,
proc, or GDB.

**Symptom.** A producer process disappears while a consumer is starting.
The shell reports that the producer died from `SIGPIPE`, or it reports
nothing and `waitpid` shows death by that signal. No `EPIPE` is visible
in the producer's own logging, because the producer has no logging
after the `write` that killed it.

**Hypothesis.** `pipe_write` observed `readers == 0`. Either the
consumer closed the read end, or the parent closed it before the child
inherited a reference, or the child exited without reading. The write
posted `SIGPIPE` and returned `-EPIPE` from the kernel, and the default
disposition never let user code see the errno.

**Evidence.**

- API, predicted from Day 4: with `SIGPIPE` ignored, `write` returns
  `-1` and `errno == EPIPE`. With the default disposition, the process
  dies and that return is not observed in the program.
- Trace: `strace -f` shows `write` and then `SIGPIPE` for the default
  case, or `write(...) = -1 EPIPE` when the signal is ignored. There is
  no second `write` of the bytes that were supposed to follow.
- Proc, if you catch the process before it dies, or on the consumer:
  `/proc/PID/fd` for the consumer does not contain a `pipe:[inode]`
  matching the producer's write end, or the consumer has already
  exited and its `/proc` directory is gone. `ENOENT` on that directory
  is evidence the task is gone, not evidence about the pipe.
- Source: `fs/pipe.c` `pipe_write`, lines 457–460, tree
  `adc218676eef`. `send_sig(SIGPIPE, current, 0)` and `ret = -EPIPE`.
- GDB, optional: a breakpoint on `write` is too late to see the kernel
  check, and a catch of `SIGPIPE` shows the user instruction pointer
  still inside the libc `write` wrapper. That confirms the signal
  arrived on the way out of the syscall. It does not by itself show
  `readers`.

**Root cause.** The last reference on the read-end `struct file` was
dropped before this `write`. `pipe_release` decremented `readers`. The
producer's `write` is behaving as specified. The bug is the close order
in the pipeline, the same missing `dup2`/`close` protocol as Day 4's
section 7.

**Fix.** The parent keeps the read end open only until the child has
inherited it, then closes the parent's copy. The child maps the read
end onto descriptor 0 and closes the original indexes so it is not
itself a writer. The producer ignores `SIGPIPE` only if a vanished
consumer is a normal event, and then it treats `EPIPE` as "this
connection is gone" rather than as a short write to retry blindly.

**Regression.** Two checks. A pipeline that writes three bytes and
closes the write end makes the child `read` return 3 and then 0, and
`waitpid` collects the child. A second program ignores `SIGPIPE`,
closes the read end, writes one byte, and requires `errno == EPIPE`
while the process is still alive. The first check guards the success
path. The second guards the error path Day 4 separated from end-of-file.
A test that only checks "the process exited" passes for both the bug
and the fix.

## 8. The same shape for a readiness stall

**Symptom.** An edge-triggered epoll loop stops calling `read` while a
writer has already pushed more bytes than one `read` accepted. The
process sits in `epoll_wait`. A later write unsticks it. A quiet writer
does not.

**Hypothesis.** `EPOLLET` did not requeue the item, and the program
issued one `read` and went back to sleep. The pipe ring still holds
bytes. No new `pipe_write` wakeup has run `ep_poll_callback`.

**Evidence.**

- Trace: `epoll_ctl` with `EPOLLET`, one `read` returning a positive
  count smaller than the write, then `epoll_wait` not returning.
  `ioctl(FIONREAD)` on the same pipe, from a debugger or a side path,
  returns a nonzero count. That ioctl is `pipe_ioctl` summing buffer
  lengths, not a guess.
- Proc: `/proc/PID/fdinfo/N` for the read end. If `O_NONBLOCK` is
  absent from `flags`, a different bug is also available: the next
  `read` can sleep inside the kernel instead of returning `EAGAIN`.
  The symlink still says `pipe:[inode]`.
- Source: `ep_send_events` requeues only when `EPOLLET` is clear
  (`fs/eventpoll.c` lines 1885–1897). `pipe_read` returns `-EAGAIN` for
  a nonblocking empty ring (lines 356–360).
- API: `epoll_wait` returning 1 did not mean the pipe was empty
  afterwards. The following `read` is the byte count.

**Root cause.** The readiness bit was consumed as if it were a
completion of the whole buffer. Edge trigger requires the caller to
drain until `EAGAIN`.

**Fix.** Set `O_NONBLOCK` on the read end. After `epoll_wait` reports
`EPOLLIN`, `read` until `EAGAIN`, and treat `0` as end-of-file. Do not
treat `EAGAIN` as failure.

**Regression.** Write more than one `read` buffer, do not write again,
and require the reader to observe every byte before it blocks in
`epoll_wait`. A timeout of zero on a second `epoll_wait`, issued after
a single short `read` and before the drain, must be allowed to return
0 on an edge-triggered set. That 0 is the bug's fingerprint. After the
fix, that second wait is not reached until `EAGAIN`, and the byte
count matches the write.

## 9. The same shape for `SIGBUS`

**Symptom.** A process that mapped a file dies with `SIGBUS`. strace
shows `mmap` and `ftruncate` and then the signal. It does not show a
failing `read`.

**Hypothesis.** The mapping still covers the address, and the file's
`i_size` no longer does. `filemap_fault` returned `VM_FAULT_SIGBUS`.

**Evidence.**

- Trace: `mmap`, then `ftruncate` to a shorter length, then `SIGBUS`.
  No `read` syscall on the faulting thread.
- GDB: the instruction is a load or a store, and the fault address
  lies inside the mapping and at or beyond the new end of the file.
- Source: `filemap_fault` lines 3313–3315, and `do_sigbus` line 948,
  `force_sig_fault(SIGBUS, BUS_ADRERR, address)`, tree
  `adc218676eef`.
- API: there is no errno, because the faulting access was not a
  syscall. `msync` and `munmap` are the syscalls that could have been
  used before the load.

**Root cause.** The program kept using a mapping across a shrink of
the file.

**Fix.** `munmap` before the access, or do not shrink a file that is
still mapped, or bound the load to the size you sampled under a
protocol that cannot race the truncate. A signal handler is not the
fix unless it removes the VMA and does not return to the faulting
instruction.

**Regression.** A load inside `i_size` succeeds and sees the file byte
under `MAP_SHARED`. A load after `ftruncate` below that offset, with
the mapping intact, is the expected `SIGBUS` and is not reported as a
passing read. A load after `munmap` is a segmentation fault instead,
which shows the two faults stayed distinct.

## 10. What each layer is allowed to prove

```text
layer      answers                                      does not answer
source     which function returns -EPIPE, SIGBUS,       what this run did
           EAGAIN on tree adc218676eef
API        what this call returned, or which signal     why the ring was empty
           arrived, in this process
strace     the syscalls and the signal, with arguments  a load that never entered
           and return values                            the kernel as a syscall
/proc fd   the path text and the flags/pos at one read  head, tail, or the waiter
GDB        the faulting instruction and address        i_size, unless the target
                                                        is the kernel
```

A sentence in the report is done when it names one cell. "The writer
died from `SIGPIPE`" is an API fact if `waitpid` decoded the signal,
or a trace fact if strace printed it. "Because `readers` was zero" is
a source fact, earned by the trace showing no live read end and by
`pipe_write` being the function that posts that signal. Neither
sentence alone is the root cause. The root cause is the close that
dropped the last reader, which you earn by showing the missing
descriptor in `/proc` or the `close` in the trace.

Optional tools stay optional. A ptrace policy that refuses strace or
GDB belongs in the report as a refusal and the tool's diagnostic, the
way [Week 10](../../10-linux-processes-and-signals/sections/day-007-proc-strace-gdb-comparison-report.md)
and [Week 11](../../11-threads-and-concurrency/sections/day-007-debuggers-sanitizers-and-system-comparison.md)
already treat a missing sanitizer. The refusal is not evidence that
the pipe was empty. The source and the API checks still stand.

## 11. ToyOS, this tree, and version discipline

[Week 8's pipe](../../08-user-mode-syscalls-and-ipc/sections/day-006-process-fd-ipc.md)
is a 64-byte linear buffer, descriptors 3 and 4, and a read that
returns 0 as soon as the buffer is empty. There is no `writers` count,
no `SIGPIPE`, no sleep on a full ring, and no `dup2` pipeline because
there is no second process with its own fd table.
[Week 9's VFS](../../09-filesystems-and-virtual-devices/sections/day-001-vfs-objects-and-ownership.md)
names files with inodes and descriptor indexes, and it does not build
`/proc`, a page cache, or epoll. ToyOS evidence for those labs is the
serial line and QEMU GDB, not a proc symlink.

This tree's pipe is the object Days 4 and 5 read: a ring of pages, two
`struct file` objects, `pipe_read` / `pipe_write`, and `epoll` hooked
through `pipe_poll`. This tree's mapping is Day 6: `mmap` publishes a
VMA, `filemap_fault` fills a PTE, `SIGBUS` fires when the index is past
`i_size`. The comparison worth writing down is which invariant each
system actually has.

```text
question                         ToyOS Week 8–9              this Linux tree
where do pipe bytes live?       pipe_data[64]               pipe_buffer pages
empty, writer still open?       read returns 0              sleep, or -EAGAIN
last writer closed?             not represented             read returns 0
no reader?                       write fails the fit check   SIGPIPE and -EPIPE
many sources?                    one global pipe             select/poll/epoll
file bytes in user loads?       not implemented             mmap + filemap_fault
how do you observe it?          serial, QEMU GDB            proc, strace, GDB, source
```

Version discipline is part of the report, not a footer. `adc218676eef`
is the tree these lessons quote. It is 6.12. The lab machine's
`uname -r` may be a different release. `pipe_read`, `ep_send_events_proc`,
`filemap_fault`, and `seq_show` move, split, and gain arguments. A line
number copied from this lesson into a report about a different tree is
a false citation. The honest citation is: the behavior I observed
(return value, signal, proc text), the tree I opened, and the function
I found there. If you did not open a tree, say so, and keep the claim
at the API layer.

The same discipline applies to ToyOS. A serial line from the Week 8
image is evidence about that image. It is not evidence about
`pipe_release`. Writing "ToyOS returns `EPIPE`" would be a false
citation in the other direction: that image returns 0 on an empty
buffer and has no `EPIPE`.

## 12. A report you can reread in a month

Record, in this order, before any interpretation:

```text
uname -r
date
compiler and the lab binary's hash, if the wrapper prints one
tree id, if you opened sources (adc218676eef for these notes)

for the failing run:
  the argv
  the wait status or the signal
  the strace lines you are willing to quote, or "strace refused: …"
  the /proc/PID/fd and fdinfo text, or "process already gone"
  the GDB instruction and fault address, or "GDB refused: …"

then the six headings:
  symptom
  hypothesis
  evidence   (each bullet tagged source / API / trace / proc / GDB)
  root cause
  fix
  regression (the command and the expected line)
```

The three cases in sections 4–6 are the templates. A new bug gets the
same headings. A bug you have not localized yet stops at hypothesis and
lists the evidence you still need. Filling in a root cause without a
layer tag is how a short `read` becomes a fictional `EPIPE`.

The weekday's wrapper collects what it can collect without root and
without tracing unrelated processes. A passing wrapper is not the
report. The report is the six headings in your own words, tied to a
tree id when you cite a function.

## 13. Mastery: Explain · Draw · Observe · Build

The exercise is
[../challenges/day-007-io-debug-report.sh](../challenges/day-007-io-debug-report.sh).
Predict which of its checks are API assertions and which facts are left
for the write-up.

### Explain

- What `seq_show` prints for every descriptor, and why a pipe in this
  tree adds nothing through `show_fdinfo`.
- How `proc_fd_link` and `pipefs_dname` together make both ends of one
  pipe print the same `pipe:[inode]` text.
- Why `SIGPIPE` and `EPIPE` can describe one `write`, and which
  disposition hides the errno.
- Why a `SIGBUS` handler that returns, without changing the VMA or the
  file size, faults again on the same instruction.
- What ToyOS can show you about a pipe, and which of `EPIPE`,
  `epoll_wait`, and `SIGBUS` it cannot produce.

### Draw

Draw one failing `write` with five boxes: the close that dropped the
last reader, `pipe_release`, `pipe_write`, the signal disposition, and
the strace line. Under it, draw a `SIGBUS` load: the store or load,
the VMA, `filemap_fault`, `do_sigbus`, and the instruction pointer a
handler would return to. Mark which boxes are source and which are
trace.

### Observe

Run the wrapper. Keep `uname -r`, the tree id if you opened sources,
and one proc excerpt or an explicit "process already gone." Quote a
kernel line only from a tree you identified. If strace or GDB is
refused, keep the refusal text.

### Build

Write one of sections 4, 5, or 6 as a one-page report with the six
headings, each evidence bullet tagged with its layer. Include one
ToyOS sentence and one version sentence: what the educational pipe
does instead, and which tree the Linux function name came from. Leave
the regression command in the write-up so the check can be run again
without reconstructing it from memory.
