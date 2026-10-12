# Linux 0.12 → Linux 5.10 Reading Map

Use this reference with Weeks 10–12.

Linux 0.12 is the first-pass map: its small source tree makes the whole path
visible. The local 5.10.134-based tree at `c9b2385d6793` remains the
engineering reference for modern Linux. Read in this order:

```text
question → Linux 0.12 whole mechanism → ToyOS reconstruction
         → Linux 5.10 implementation → current upstream verification
```

The early source comes from *A Heavily Commented Linux Kernel Source Code:
Kernel Version 0.12*, Chinese Revision 5 (Zhao Jiong, 2019). Page numbers
below are the printed book pages. The PDF viewer is about seven pages ahead
because of its front matter.

## Week 10 — Processes, fork, exec, exit, and files

Read these compact paths first:

- Chapter 8.8, pages 391–403: `kernel/exit.c`
- Chapter 8.9, pages 404–412: `kernel/fork.c`
- Chapter 12.14, pages 817–825: `fs/open.c`
- Chapter 12.15, pages 826–844: `fs/exec.c`

### Fork and copy-on-write

Linux 0.12 puts the complete address-space setup in a small `copy_mem`
function:

```c
int copy_mem(int nr, struct task_struct *p)
{
    unsigned long old_data_base, new_data_base, data_limit;
    unsigned long old_code_base, new_code_base, code_limit;

    code_limit = get_limit(0x0f);
    data_limit = get_limit(0x17);
    old_code_base = get_base(current->ldt[1]);
    old_data_base = get_base(current->ldt[2]);
    if (old_data_base != old_code_base)
        panic("We don't support separate I&D");
    /* code/data limit validation omitted from this excerpt */

    new_data_base = new_code_base = nr * TASK_SIZE;
    p->start_code = new_code_base;
    set_base(p->ldt[1], new_code_base);
    set_base(p->ldt[2], new_data_base);
    if (copy_page_tables(old_data_base, new_data_base, data_limit)) {
        free_page_tables(new_data_base, data_limit);
        return -ENOMEM;
    }
    return 0;
}
```

This exposes the durable idea: the child receives a new execution identity
and page-table view while physical pages initially remain shared through
copy-on-write. The segmentation layout, fixed `TASK_SIZE` slot, 80386 task
machinery, and function names are historical. Week 10 Day 2 then follows the
modern split through `kernel_clone`, `copy_process`, `copy_mm`, and the
write-protect fault path in the 5.10 tree.

### Exit and zombie state

The early `do_exit` makes the retained exit record easy to see:

```c
free_page_tables(get_base(current->ldt[1]), get_limit(0x0f));
free_page_tables(get_base(current->ldt[2]), get_limit(0x17));
for (i = 0; i < NR_OPEN; i++)
    if (current->filp[i])
        sys_close(i);
iput(current->pwd);
current->pwd = NULL;
iput(current->root);
current->root = NULL;
iput(current->executable);
current->executable = NULL;
current->state = TASK_ZOMBIE;
current->exit_code = code;
```

The program image and open resources are released while task state and the
exit code remain for the parent. Week 10 Day 4 shows the modern lifetime,
reparenting, notification, and wait paths.

### Exec

Linux 0.12 uses `a.out`, segment descriptors, and direct manipulation of the
saved return frame. It is excellent for seeing “replace the image, retain the
process identity.” It cannot teach the modern ELF loader, `PT_INTERP`,
auxiliary vector, ASLR, credentials, namespaces, or current security hooks.
Use Week 10 Day 3 and `fs/exec.c`/`fs/binfmt_elf.c` for those.

## Week 11 — Threads and concurrency

Read Chapter 8.6 (`kernel/sched.c`, pages 351–372) for a compact task,
sleep/wakeup, timer, and scheduling loop. Use it only for the base questions:

```text
Why does a task sleep?
Where is runnable state recorded?
Who wakes it?
How does another task receive the CPU?
```

Linux 0.12 is a uniprocessor kernel and has no modern pthread implementation,
`clone` sharing model, futex subsystem, C11 atomics, RCU, per-CPU runqueues,
or SMP memory-ordering problem. Therefore:

- use 0.12 to see the whole sleep/wakeup and task-switch shape;
- use ToyOS to reconstruct the minimal mechanism;
- use Week 11 and the 5.10 tree for threads, futexes, lock ownership,
  condition variables, atomics, barriers, false sharing, and tooling.

For Week 11, replacing the modern source with 0.12 would remove the mechanism
being studied.

## Week 12 — Descriptors, reads, pipes, and readiness

Read these early paths:

- Chapter 12.11, pages 802–806: `fs/pipe.c`
- Chapter 12.13, pages 810–816: `fs/read_write.c`
- Chapter 12.14, pages 817–825: `fs/open.c`
- Chapter 12.19, pages 854–867: `fs/select.c`

### Descriptor → open file → inode

The central ownership chain is visible inside early `sys_open`:

```c
current->close_on_exec &= ~(1 << fd);
f = file_table;
for (i = 0; i < NR_FILE; i++, f++)
    if (!f->f_count)
        break;
if (i >= NR_FILE)
    return -EINVAL;

(current->filp[fd] = f)->f_count++;
if ((i = open_namei(filename, flag, mode, &inode)) < 0) {
    current->filp[fd] = NULL;
    f->f_count = 0;
    return i;
}
/* character/block-device checks omitted from this excerpt */
f->f_mode = inode->i_mode;
f->f_flags = flag;
f->f_count = 1;
f->f_inode = inode;
f->f_pos = 0;
return fd;
```

The small integer indexes `current->filp`; the referenced `struct file` owns
flags, a reference count, inode, and offset. That is the early version of the
ownership model used in Week 12 Day 1. Modern Linux replaces the fixed arrays
and global table with scalable fd tables, RCU/refcount rules, richer
`struct file`, dentries, mount state, and `file_operations`.

### Read dispatch

Early `sys_read` keeps object dispatch in one function:

```c
if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd]))
    return -EINVAL;
if (!count)
    return 0;
verify_area(buf, count);
inode = file->f_inode;
if (inode->i_pipe)
    return (file->f_mode & 1) ? read_pipe(inode, buf, count) : -EIO;
if (S_ISCHR(inode->i_mode))
    return rw_char(READ, inode->i_zone[0], buf, count, &file->f_pos);
if (S_ISBLK(inode->i_mode))
    return block_read(inode->i_zone[0], &file->f_pos, buf, count);
if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)) {
    if (count + file->f_pos > inode->i_size)
        count = inode->i_size - file->f_pos;
    if (count <= 0)
        return 0;
    return file_read(inode, file, buf, count);
}
return -EINVAL;
```

This is a clear first picture of “one syscall, object-specific behavior.”
Linux 0.12 predates the modern VFS model. Week 12 follows the same purpose
through `vfs_read`, `file_operations`, user-access helpers, and filesystem or
device implementations in the 5.10 tree.

### Pipe sleep, wakeup, EOF, and backpressure

The early pipe is one page with head/tail indices:

```c
while (count > 0) {
    while (!(size = PIPE_SIZE(*inode))) {
        wake_up(&PIPE_WRITE_WAIT(*inode));
        if (inode->i_count != 2)
            return read;
        if (current->signal & ~current->blocked)
            return read ? read : -ERESTARTSYS;
        interruptible_sleep_on(&PIPE_READ_WAIT(*inode));
    }
    chars = PAGE_SIZE - PIPE_TAIL(*inode);
    if (chars > count)
        chars = count;
    if (chars > size)
        chars = size;
    /* advance the tail and copy chars to user space */
}
wake_up(&PIPE_WRITE_WAIT(*inode));
return read;
```

It gives the full causal picture: empty readers sleep, writers wake them,
closing the writer creates EOF, and consuming bytes wakes blocked writers.
Week 12 Day 4 then adds modern pipe buffers, poll integration, atomicity
rules, nonblocking behavior, and current lifetime handling.

### Where the early map stops

Linux 0.12 has `select`, a buffer cache, Minix filesystem code, and 80386
paging. It does not provide the modern VFS/page-cache model, `poll`, `epoll`,
edge-triggered readiness, current `mmap` behavior, SMP locking, or present-day
durability stack. Use it for the initial control-flow picture only. Weeks 12
Days 3, 5, and 6 must remain grounded in the 5.10 source and current API
contracts.

## Source-discipline rule

For every early excerpt, state three things:

1. the stable mechanism it makes visible;
2. the historical implementation detail that must not be generalized;
3. the 5.10/current source path that verifies the modern implementation.

This preserves the clarity of Linux 0.12 without converting historical code
into modern kernel guidance.
