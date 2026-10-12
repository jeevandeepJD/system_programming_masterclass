# Day 6 — `mmap`, Shared Mappings, and Data Movement

**Target time:** approximately 3 hours

> `mmap` does not copy the file. It publishes a virtual range whose
> page-table entries, filled in by faults, name pages in the page cache.
> After that, the CPU's ordinary loads and stores are the I/O.

`read` and `write` move bytes by copying. Day 4's pipe does it twice:
once from the writer's buffer into a kernel page, once from that page
into the reader's buffer. A file `read` does the same shape through the
page cache. [Week 9](../../09-filesystems-and-virtual-devices/sections/day-004-storage-stack-boundary.md)
drew that chain and stopped at the boundary ToyOS does not cross. Today
the chain is real, and there is a second way to finish it. Instead of
asking the kernel to copy a range into a buffer you allocated, you ask
the kernel to add a VMA. The first load or store of a missing page
faults. The fault handler finds or reads the cache page and installs a
PTE. The faulting instruction is then restarted. The load that faulted
is the load that observes the file byte.

Neither path is the fast one for every program. The copy path pays a
memory copy and gets a simple lifetime: the buffer is anonymous memory,
the call returns a count, and a short file returns a short count.
The mapping path pays faults, TLB entries, and a signal if the file
shrinks under you, and it gets a path where a later load does not enter
the kernel at all. The excerpts are Linux 5.10.134 at tree
`c9b2385d6793`.

## 1. Why a load should be able to name a file byte

A userspace buffer and a page-cache page are both physical memory.
[Week 7's page tables](../../07-memory-processes-and-scheduling/sections/day-002-page-tables-mapper-and-faults.md)
are what decide which virtual address names which physical page. `read`
keeps those names apart. The cache page has a kernel virtual address,
or is mapped temporarily, and `copy_to_user` stores into the user
buffer's page. The user program never holds a virtual address for the
cache page. That is easy to reason about, and it is mandatory when the
consumer is not allowed to keep the page, or when the data is a pipe
buffer that has no file offset.

It is a tax when the program's next act is to load those same bytes
with ordinary instructions. The copy exists so the loads have something
to hit. If the PTE can name the cache page directly, the copy is
unnecessary for every byte the program actually touches, and the bytes
it never touches are never copied. That is the reason `mmap` exists.
The cost moves to the first reference, which becomes a page fault, and
to every later protection or lifetime change, which becomes a TLB
shootdown and possibly a signal rather than an errno.

The CPU does not know the name "page cache." It knows a load or a
store, a virtual address, and a translation. [Week 2](../../02-assembly-memory-and-c/sections/day-001-x86-64-assembly-foundations.md)
is that instruction. [Week 6](../../06-toyos-bootstrap-and-exceptions/sections/day-006-tss-ist-page-fault-and-gdb.md)
is the trap that runs when the translation is missing or forbidden.
The kernel's job on that trap is to make the translation true and
return to the faulting instruction, or to deliver a signal if it
cannot.

## 2. `mmap` builds a VMA and returns

On x86-64 the syscall checks the file offset before it will call the
common path. A byte offset that is not a multiple of the page size is
rejected. The shift that follows is what `ksys_mmap_pgoff` receives as
a page offset.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: arch/x86/kernel/sys_x86_64.c
 * lines: 86–98
 * function: mmap (SYSCALL_DEFINE6)
 */
SYSCALL_DEFINE6(mmap, unsigned long, addr, unsigned long, len,
		unsigned long, prot, unsigned long, flags,
		unsigned long, fd, unsigned long, off)
{
	long error;
	error = -EINVAL;
	if (off & ~PAGE_MASK)
		goto out;

	error = ksys_mmap_pgoff(addr, len, prot, flags, fd, off >> PAGE_SHIFT);
out:
	return error;
}
```

`ksys_mmap_pgoff` turns a descriptor into a `struct file`, or refuses
to. `MAP_ANONYMOUS` skips the file. Anything else takes a reference
with `fget` and returns `-EBADF` when the descriptor does not name one.
The same `-EBADF` is what a bad pipe descriptor produces in Day 4's
table lookup. The object behind a successful file mapping is the open
file Week 9 already defined, not a new kind of kernel object.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: mm/mmap.c
 * lines: 1690–1707
 * function: ksys_mmap_pgoff
 */
unsigned long ksys_mmap_pgoff(unsigned long addr, unsigned long len,
			      unsigned long prot, unsigned long flags,
			      unsigned long fd, unsigned long pgoff)
{
	struct file *file = NULL;
	unsigned long retval;

	if (!(flags & MAP_ANONYMOUS)) {
		audit_mmap_fd(fd, flags);
		file = fget(fd);
		if (!file)
			return -EBADF;
		if (is_file_hugepages(file)) {
			len = ALIGN(len, huge_page_size(hstate_file(file)));
		} else if (unlikely(flags & MAP_HUGETLB)) {
			retval = -EINVAL;
			goto out_fput;
		}
```

`do_mmap` is where the range becomes a mapping. The length is rounded
up to a page. A length that rounds to zero returns `-ENOMEM`. An
offset that would wrap the page-count addition returns `-EOVERFLOW`.
Too many VMAs returns `-ENOMEM` as well. The function then asks
`get_unmapped_area` for an address if the caller did not fix one, and
it turns `prot` into `vm_flags` with `calc_vm_prot_bits`. A later store
is allowed only if those flags include write permission. `access_error`
in `arch/x86/mm/fault.c` returns failure for a write fault when the VMA
lacks `VM_WRITE` (lines 1134–1137 of that file), and `do_user_addr_fault`
does not call `handle_mm_fault` in that case.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: mm/mmap.c
 * lines: 1490–1501
 * function: do_mmap
 */
	/* Careful about overflows.. */
	len = PAGE_ALIGN(len);
	if (!len)
		return -ENOMEM;

	/* offset overflow? */
	if ((pgoff + (len >> PAGE_SHIFT)) < pgoff)
		return -EOVERFLOW;

	/* Too many mappings? */
	if (mm->map_count > sysctl_max_map_count)
		return -ENOMEM;
```

For a file, `MAP_SHARED` and `MAP_PRIVATE` share the checks that the
file is readable, that a `mmap` file operation exists, and that the VMA
is not allowed to grow like a stack. They differ in `VM_SHARED`.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: mm/mmap.c
 * lines: 1595–1618
 * function: do_mmap
 */
			/*
			 * Make sure there are no mandatory locks on the file.
			 */
			if (locks_verify_locked(file))
				return -EAGAIN;

			vm_flags |= VM_SHARED | VM_MAYSHARE;
			if (!(file->f_mode & FMODE_WRITE))
				vm_flags &= ~(VM_MAYWRITE | VM_SHARED);
			fallthrough;
		case MAP_PRIVATE:
			if (!(file->f_mode & FMODE_READ))
				return -EACCES;
			if (path_noexec(&file->f_path)) {
				if (vm_flags & VM_EXEC)
					return -EPERM;
				vm_flags &= ~VM_MAYEXEC;
			}

			if (!file->f_op->mmap)
				return -ENODEV;
			if (vm_flags & (VM_GROWSDOWN|VM_GROWSUP))
				return -EINVAL;
			break;
```

`MAP_SHARED` sets `VM_SHARED | VM_MAYSHARE`, then clears `VM_MAYWRITE`
and `VM_SHARED` again if the open file itself is not writable. A shared
mapping of a read-only file descriptor cannot become a write into the
page cache through this path. `MAP_PRIVATE` does not set `VM_SHARED`.
That one bit is what `do_fault` later uses to choose a copy-on-write
fault instead of a write into the cache page.

`do_mmap` finishes by calling `mmap_region`, which installs the VMA.
Unless the caller set `MAP_POPULATE` (and not `MAP_NONBLOCK`) or
`MAP_LOCKED`, the function does not fill PTEs. The syscall returns the
start address. Every page in the range is still a hole as far as the
TLB is concerned. The return of `mmap` is the publication of a
permission and a file offset, not the arrival of the bytes.

`generic_file_mmap` is what a normal disk file's `mmap` operation
installs as the fault ops. The connection from a VMA to `filemap_fault`
is this table:

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: mm/filemap.c
 * lines: 3175–3192
 * function: generic_file_vm_ops, generic_file_mmap
 */
const struct vm_operations_struct generic_file_vm_ops = {
	.fault		= filemap_fault,
	.map_pages	= filemap_map_pages,
	.page_mkwrite	= filemap_page_mkwrite,
};

/* This is used for a general mmap of a disk file */

int generic_file_mmap(struct file * file, struct vm_area_struct * vma)
{
	struct address_space *mapping = file->f_mapping;

	if (!mapping->a_ops->readpage)
		return -ENOEXEC;
	file_accessed(file);
	vma->vm_ops = &generic_file_vm_ops;
	return 0;
}
```

A mapping whose address space cannot `readpage` fails here with
`-ENOEXEC`. Pipes are not this path. A pipe has no page-cache offset
for a byte in the ring. Mapping a pipe is a different question from
mapping a regular file, and this lesson's fault trace is the regular
file.

## 3. The fault is the data movement

The instruction that touches the new range is an ordinary load or
store. The TLB has no entry. The walker from
[Week 7](../../07-memory-processes-and-scheduling/sections/day-002-page-tables-mapper-and-faults.md)
finds no present PTE, or a PTE whose permission is short. The CPU
raises a page fault. The saved frame is the one
[Week 6](../../06-toyos-bootstrap-and-exceptions/sections/day-005-idt-stubs-and-uniform-frame.md)
built a uniform version of. On x86 the fault address arrives beside
the registers. The handler does not emulate the load. It installs a
translation and returns to the same instruction pointer.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: arch/x86/mm/fault.c
 * lines: 1426–1440
 * function: do_user_addr_fault
 */
retry:
	vma = lock_mm_and_find_vma(mm, address, regs);
	if (unlikely(!vma)) {
		bad_area_nosemaphore(regs, hw_error_code, address);
		return;
	}

	/*
	 * Ok, we have a good vm_area for this memory access, so
	 * we can handle it..
	 */
	if (unlikely(access_error(hw_error_code, vma))) {
		bad_area_access_error(regs, hw_error_code, address, vma);
		return;
	}
```

Line 1455 is the call that continues a permitted fault:
`fault = handle_mm_fault(vma, address, flags, regs)`. The comment
immediately above it says a `VM_FAULT_RETRY` means `mmap_lock` was
dropped, so the handler must not assume the VMA pointer is still valid
on that return.

No VMA means the address was never mapped, or was already removed by
`munmap`. That path is `bad_area_nosemaphore`, a segmentation fault, not
a bus error. A VMA that exists but does not allow the access is
`access_error`, still before any page-cache work. Only a VMA that
covers the address and permits the access reaches `handle_mm_fault`.
Lines 5251–5262 count `PGFAULT` and mark the task running before this
dispatch.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: mm/memory.c
 * lines: 5264–5285
 * function: handle_mm_fault
 */
	if (!arch_vma_access_permitted(vma, flags & FAULT_FLAG_WRITE,
					    flags & FAULT_FLAG_INSTRUCTION,
					    flags & FAULT_FLAG_REMOTE))
		return VM_FAULT_SIGSEGV;

	/*
	 * Enable the memcg OOM handling for faults triggered in user
	 * space.  Kernel faults are handled more gracefully.
	 */
	if (flags & FAULT_FLAG_USER)
		mem_cgroup_enter_user_fault();

	lru_gen_enter_fault(vma);

	if (unlikely(is_vm_hugetlb_page(vma)))
		ret = hugetlb_fault(vma->vm_mm, vma, address, flags);
#ifdef CONFIG_PAGETABLE_SHARE
	else if  (unlikely(vma_is_pgtable_shared(vma)))
		ret = pgtable_share_fault(vma, address, flags);
#endif
	else
		ret = __handle_mm_fault(vma, address, flags);
```

`__handle_mm_fault` (lines 5072–5086) sets `vmf.address` to
`address & PAGE_MASK` and `vmf.pgoff` to `linear_page_index`. That index
is how many pages from the start of the VMA, which `do_mmap` tied to
`pgoff`. The first and last byte of one page share one cache page. The
function walks the page-table levels and returns
`handle_pte_fault(&vmf)` at line 5160. A file-backed hole in the PTE
reaches `do_fault`. The write bit and `VM_SHARED` pick the kind of
fault:

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: mm/memory.c
 * lines: 4675–4688
 * function: do_fault
 */
	} else if (!(vmf->flags & FAULT_FLAG_WRITE))
		ret = do_read_fault(vmf);
	else if (!(vma->vm_flags & VM_SHARED))
		ret = do_cow_fault(vmf);
	else
		ret = do_shared_fault(vmf);

	/* preallocated pagetable is unused: free it */
	if (vmf->prealloc_pte) {
		pte_free(vm_mm, vmf->prealloc_pte);
		vmf->prealloc_pte = NULL;
	}
	return ret;
}
```

A read fault calls `do_read_fault`, which calls `__do_fault`, which
calls `vma->vm_ops->fault`. For the ops installed above, that function
is `filemap_fault`. The comment above `__do_fault` names that
function (lines 4012–4015). The call is line 4044:

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: mm/memory.c
 * lines: 4037–4049
 * function: __do_fault
 */
	if (pmd_none(*vmf->pmd) && !vmf->prealloc_pte) {
		vmf->prealloc_pte = pte_alloc_one(vma->vm_mm);
		if (!vmf->prealloc_pte)
			return VM_FAULT_OOM;
		smp_wmb(); /* See comment in __pte_alloc() */
	}

	ret = vma->vm_ops->fault(vmf);
	if (unlikely(ret & (VM_FAULT_ERROR | VM_FAULT_NOPAGE | VM_FAULT_RETRY |
			    VM_FAULT_DONE_COW)))
		return ret;

	if (unlikely(PageHWPoison(vmf->page))) {
```

`filemap_fault` turns the page index into a cache lookup. If the index
is past `i_size`, there is no byte to name, and the fault becomes
`VM_FAULT_SIGBUS`. If the page is already cached, it is used. If it is
not, the fault is counted as major and the address space's `readpage`
path is started by allocating a cache page.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: mm/filemap.c
 * lines: 2959–2988
 * function: filemap_fault
 */
	max_off = DIV_ROUND_UP(i_size_read(inode), PAGE_SIZE);
	if (unlikely(offset >= max_off))
		return VM_FAULT_SIGBUS;

	/*
	 * Do we have something in the page cache already?
	 */
	page = find_get_page(mapping, offset);
	if (likely(page) && !(vmf->flags & FAULT_FLAG_TRIED)) {
		/*
		 * We found the page, so try async readahead before
		 * waiting for the lock.
		 */
		fpin = do_async_mmap_readahead(vmf, page);
	} else if (!page) {
		/* No page in the page cache at all */
		count_vm_event(PGMAJFAULT);
		count_memcg_event_mm(vmf->vma->vm_mm, PGMAJFAULT);
		ret = VM_FAULT_MAJOR;
		fpin = do_sync_mmap_readahead(vmf);
retry_find:
		page = pagecache_get_page(mapping, offset,
					  FGP_CREAT|FGP_FOR_MMAP,
					  vmf->gfp_mask);
		if (!page) {
			if (fpin)
				goto out_retry;
			return VM_FAULT_OOM;
		}
	}
```

When the locked page is up to date and still inside `i_size`,
`filemap_fault` stores it in `vmf->page` and returns `VM_FAULT_LOCKED`
(lines 3022–3030). The finish path installs a PTE and unlocks the page.
`VM_FAULT_RETRY` sets `FAULT_FLAG_TRIED` and jumps back to `retry`
(lines 1470–1473), because `mmap_lock` was dropped. `VM_FAULT_SIGBUS`
becomes `SIGBUS` in `mm_fault_error`, which Day 7 keeps distinct from a
syscall errno. The trap return restores the saved instruction pointer.
The load or store runs again, the TLB fills from the new PTE, and the
bytes move between the cache page and a register with no
`copy_to_user`.

```text
load/store
  TLB miss, page-table walk misses or denies
  page-fault trap, fault address, error code
do_user_addr_fault
  lock_mm_and_find_vma
  access_error?  -> SIGSEGV, no cache lookup
handle_mm_fault
  __handle_mm_fault: address & PAGE_MASK, linear_page_index
  do_fault -> vm_ops->fault == filemap_fault
filemap_fault
  offset >= i_size -> VM_FAULT_SIGBUS
  else find or create the page-cache page
finish: PTE points at that page
iret to the same instruction
  TLB fill, the load or store completes
```

A `read` of the same file does not take this trap. It enters on the
syscall instruction, finds the same cache page, and copies into the
user buffer. A `read` that filled the cache, then a fault on that
offset, takes `find_get_page` and does not start I/O again. A fault
that filled the cache, then a `read`, copies out of the page the fault
brought in. The page cache is the meeting point.

## 4. Shared writes the cache page; private writes a copy

A write fault on a `MAP_SHARED` VMA takes `do_shared_fault`. The page
`filemap_fault` returns is the cache page. `filemap_page_mkwrite`, the
third op in `generic_file_vm_ops`, marks that page dirty so writeback
can see it. Another process that has `MAP_SHARED` the same offset
faults in the same cache page. A store through either mapping is a
store to that page. A later `read` of the file copies those bytes out.
This is the sense in which the mapping is shared: one physical page,
several PTEs, the file's cache as the backing.

A write fault on a `MAP_PRIVATE` VMA takes `do_cow_fault`. Lines
4565–4584 allocate `cow_page` with `alloc_page_vma`, charge it, call
`__do_fault` for the cache page, then `copy_user_highpage` onto
`cow_page` and `finish_fault`. `copy_user_highpage` is the moment the private mapping stops being the
file. The PTE names `cow_page`. The cache page is unlocked and dropped.
A store through the private mapping does not dirty the file, and a
later `read` of the file does not see it. Another `MAP_PRIVATE` mapping
of the same offset gets its own copy on its own write. Until that
write, reads share the cache page, because `do_read_fault` does not
take this arm.
[Week 10's fork COW](../../10-linux-processes-and-signals/sections/day-002-fork-cow-and-lineage.md)
is the same copy applied to anonymous pages the parent already had.
Here the flag is `VM_SHARED` being clear, and the copy is
`copy_user_highpage` on the first write through this VMA.

## 5. Alignment, protection, `SIGBUS`, `msync`, `munmap`

Three alignments show up, and they are not the same rule.

The file offset passed to x86-64 `mmap` must be page-aligned, or the
syscall returns `-EINVAL` before `ksys_mmap_pgoff`. `do_mmap` then
page-aligns the length upward. A mapping of 100 bytes occupies a whole
page in the address space. The extra bytes past `i_size`, still inside
that page, are the ones `filemap_fault` refuses.

`msync` refuses a start address that is not page-aligned, and it
refuses `MS_ASYNC` together with `MS_SYNC` (lines 42–47). The length is
rounded up to a page. The comment at lines 18–31 says `MS_ASYNC` does
not start I/O and does not mark pages dirty. Both of those used to
happen. Dirty tracking is already done at fault time. The comment's
advice is to call `fsync` or `fadvise(FADV_DONTNEED)` when the program
actually wants writeback.

`MS_SYNC` on a shared file VMA is the path that actually waits. The
syscall drops `mmap_lock`, calls `vfs_fsync_range` on the file offsets
covered by the intersection of the range and the VMA, and continues
with the next VMA. `MS_INVALIDATE` on a `VM_LOCKED` VMA returns
`-EBUSY`. A hole in the range, where `find_vma` finds nothing or the
VMA starts after the requested start, becomes `-ENOMEM` at the end
through `unmapped_error`.

```c
/* tree c9b2385d6793 (Linux 5.10.134)
 * path: mm/msync.c
 * lines: 78–104
 * function: msync
 */
		if ((flags & MS_INVALIDATE) &&
				(vma->vm_flags & VM_LOCKED)) {
			error = -EBUSY;
			goto out_unlock;
		}
		file = vma->vm_file;
		fstart = (start - vma->vm_start) +
			 ((loff_t)vma->vm_pgoff << PAGE_SHIFT);
		fend = fstart + (min(end, vma->vm_end) - start) - 1;
		start = vma->vm_end;
		if ((flags & MS_SYNC) && file &&
				(vma->vm_flags & VM_SHARED)) {
			get_file(file);
			mmap_read_unlock(mm);
			error = vfs_fsync_range(file, fstart, fend, 1);
			fput(file);
			if (error || start >= end)
				goto out;
			mmap_read_lock(mm);
			vma = find_vma(mm, start);
		} else {
			if (start >= end) {
				error = 0;
				goto out_unlock;
			}
			vma = vma->vm_next;
		}
```

`MS_SYNC` does not include a private COW page. The condition requires
`VM_SHARED`. A private store never entered the file, so there is
nothing for `vfs_fsync_range` to flush on its behalf. Calling `msync`
on a private mapping and expecting the file to change is a
misunderstanding of section 4, not a property of the sync flag.

`munmap` removes the VMA. A later load of that address takes the
`!vma` arm in `do_user_addr_fault` and does not reach `filemap_fault`.
That fault is a segmentation fault. `SIGBUS` from this subsystem is the
other outcome: the VMA is still there, and `filemap_fault` finds
`offset >= max_off` because the file shrank, or it fails to make the
page up to date and returns `VM_FAULT_SIGBUS` at line 3055 after a
`readpage` error. The access was a CPU instruction. There is no
`errno`. Day 7 says what a handler is allowed to assume about the
restarted instruction.

Inside a mapped page, the load or store uses the alignment the ISA
requires for that instruction, not the page alignment `mmap` required
for the offset. A one-byte load at `mapping + 1` faults on the same
page as a load at `mapping + 0`, because `__handle_mm_fault` masks the
address with `PAGE_MASK` before it builds the PTE. The byte offset
inside the page is the low bits of the virtual address, applied by the
CPU on the retry.

## 6. A shared page is not a lock

`MAP_SHARED` makes two processes' loads and stores hit one cache page.
The hardware coherence protocol, the same one [Week 11's false-sharing
lesson](../../11-threads-and-concurrency/sections/day-006-liveness-false-sharing-and-scalability.md)
depends on, is what makes a store on one core visible to a load on
another. That is a statement about the memory system. It is not a
statement about the C abstract machine.

If both sides are C programs and both access the same object, the
program still needs a happens-before edge. [Week 11's data-race
rule](../../11-threads-and-concurrency/sections/day-002-data-races-and-the-c-abstract-machine.md)
does not grow an exception for "the bytes live in a mapping." Two
non-atomic stores to the same `int` in a shared mapping, with no mutex
and no atomic, are a data race. The mapping changed which physical page
the stores hit. It did not insert synchronization.

A mutex in the mapping has to be process-shared when the waiters are
different processes. A futex word in that page works for the same
reason as the [Week 11 futex](../../11-threads-and-concurrency/sections/day-003-mutexes-ownership-lock-order-futex.md):
the kernel waits on an address that both tasks translate to the same
page. Atomics from [Week 11 Day 5](../../11-threads-and-concurrency/sections/day-005-c11-atomics-and-memory-ordering.md)
are the other tool. `msync` pushes dirty file data. It does not order
one process's store against another's load. Threads already share the
mapping because they share the page tables
([Week 11 Day 1](../../11-threads-and-concurrency/sections/day-001-threads-processes-clone-stacks-tls.md)).
`MAP_SHARED` across `fork` adds a second mm and a second set of PTEs
aimed at the same pages. The synchronization requirement does not get
smaller because there are two page tables.

## 7. The copy and the fault win different cases

A single sequential pass the program will not share is often cheaper
as `read`. One syscall copies many pages, readahead runs either way,
there is no user fault per file page, and EOF is a short count rather
than a signal. Repeated or shared or random access is the fault path:
the first touch pays `filemap_fault`, later touches are loads and
stores, and a second `MAP_SHARED` mapping faults in the same cache page
instead of taking its own copy from `read`. Bytes the program never
touches are never copied.

A map, one touch per page, and `munmap` measures faults plus I/O plus
teardown. A `read` into a buffer the program then parses measures the
copy the mapping avoids, and a simpler failure mode. Quote the access
pattern with the number, or the number does not say which mechanism
won.

What both paths share is the page cache and the rule that the file's
size is the limit of the bytes. `read` reports the limit as a short
count. `mmap` reports a later violation of the limit as `SIGBUS` from
`filemap_fault`. Choosing `mmap` to avoid checking the return value of
`read` trades an errno for a signal on the instruction that walked off
the file.

## 8. A mapping small enough to fault on purpose

```c
long page = sysconf(_SC_PAGESIZE);
off_t off = 0;
size_t len = (size_t)page;
char *p;

if (off & (page - 1))
    return EINVAL;                 /* x86-64 mmap rejects this in-kernel */
p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, off);
if (p == MAP_FAILED)
    return -1;
p[0] = 'A';                        /* may page-fault into the cache page */
if (msync(p, len, MS_SYNC) < 0)
    return -1;                     /* vfs_fsync_range on a VM_SHARED file */
if (munmap(p, len) < 0)
    return -1;
```

`mmap` returns without a PTE. `p[0] = 'A'` misses, traps, and
`do_shared_fault` runs because the VMA is `VM_SHARED`. `filemap_fault`
allocates a cache page. The store runs again and writes `'A'` into that
page. `msync(MS_SYNC)` calls `vfs_fsync_range`. `munmap` drops the VMA,
and a later load of `p[0]` faults with no VMA.

The same source with `MAP_PRIVATE` takes `do_cow_fault`. `'A'` lands in
the copy. `msync` does not see `VM_SHARED`, so it does not sync that
VMA, and the file still holds the original byte. `ftruncate` the file
to zero while the shared mapping is live, then load `p[0]`. The VMA
remains. `filemap_fault` returns `VM_FAULT_SIGBUS` because
`offset >= max_off`. The load was not a syscall, so there is no errno.
Day 7 is the safe description of that signal.

## 9. Mastery: Explain · Draw · Observe · Build

The exercise is
[../challenges/day-006-mmap.sh](../challenges/day-006-mmap.sh).
Predict which byte reaches the file under `MAP_SHARED`, and which byte
stays in an anonymous copy under `MAP_PRIVATE`.

### Explain

- Why `mmap` can return before any file byte has been copied, and which
  fault function performs the cache lookup.
- What `VM_SHARED` changes in `do_fault`, and what `copy_user_highpage`
  is copying onto.
- Why a load past the end of a still-mapped file is `SIGBUS` from
  `filemap_fault`, while a load of an address `munmap` removed never
  reaches `filemap_fault`.
- What `MS_SYNC` calls for a shared file VMA, and what `MS_ASYNC` does
  in this tree.
- Why two C programs storing into a `MAP_SHARED` page still need a
  mutex, a futex, or an atomic.

### Draw

Draw the section 3 trace as a vertical strip: user load, TLB, trap,
VMA, `filemap_fault`, page-cache page, PTE, retry. Put the file offset
and `i_size` on the cache page. Beside it, draw the same file byte
moving through `read`: syscall, cache page, `copy_to_user`, user
buffer. Mark which path installs a PTE and which path returns a count.

### Observe

Run the wrapper. Record `uname -r` and the tree id if you opened the
sources. Note the mapping address, the store that faults, and whether
a later `read` of the file observes the byte. If you truncate under a
live mapping, record the signal and the fault address, and do not
describe it as a failed `read`.

### Build

Write a one-page account of one store to `MAP_SHARED` and the matching
store to `MAP_PRIVATE`, using the functions from this tree:
`ksys_mmap_pgoff`, `do_mmap`, `do_fault`, `filemap_fault`,
`do_cow_fault`, `msync`. End with the sentence you will use when
someone says mapping is faster: it removes a copy after the PTE exists,
and it adds a fault, a TLB entry, and `SIGBUS` at the edge of the file.
