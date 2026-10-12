# Day 3 — Dirty Memory Is Not a Durable Write

**Target time:** approximately 3 hours

- Why a path, an open file, and a cached page are different objects: 30 minutes
- A buffered write dirties a page: 45 minutes
- Writeback, `fsync`, and the error that comes back later: 45 minutes
- Holes, a trace, and the ToyOS boundary: 35 minutes
- Mastery, then the wrapper: 25 minutes

> `write` returned 4096. The machine loses power before `fsync` returns.
> Which of those bytes can a later `open` be expected to see?

## 1. Syntax needed for this source

A cache here is an array of block headers. A loop walks them, and a member flag says a block has been modified.

```c
struct buffer *bh = start;
for (int i = 0; i < n; i++, bh++) {
    if (bh->dirty)
        write_block(bh);
}
```

`bh++` steps to the next header because `bh` points at one `struct buffer`. The flag is data. The loop does not know which file the bytes belong to.

## 2. Linux 0.12 first: a buffer cache of dirty blocks

A `write` that returns has copied bytes somewhere the next `read` can see. Linux 0.12's somewhere is the buffer cache: fixed block buffers, each with a `buffer_head`, and `b_dirt` set when the block is modified. `sys_sync` in `fs/buffer.c` (printed page 718) walks that array and pushes dirty blocks out:

```c
int sys_sync(void)
{
	int i;
	struct buffer_head * bh;

	sync_inodes();
	bh = start_buffer;
	for (i=0 ; i<NR_BUFFERS ; i++,bh++) {
		wait_on_buffer(bh);
		if (bh->b_dirt)
			ll_rw_block(WRITE,bh);
	}
	return 0;
}
```

Trace. `sync_inodes` writes modified in-memory inodes into buffers, which can set more `b_dirt` bits. `bh` starts at `start_buffer` and steps once per slot. `wait_on_buffer` sleeps while `b_lock` is set. A dirty header becomes a `WRITE` request through `ll_rw_block`. The function then returns 0. `sync_dev` (same file, just below) does two such passes around `sync_inodes` for one device.

What carries forward is the dirty bit and an explicit sync that asks for write-out. The fixed `NR_BUFFERS` array, the per-block `buffer_head`, and `ll_rw_block` belong to this kernel. This is a block buffer cache. Linux 0.12 has no VFS `file_operations` table and no unified page cache hanging off an inode. Linux 5.10 keeps "dirty memory, then a sync that waits" and implements it as the page cache: an `address_space` of pages shared by every open of that inode, with `fsync` waiting for writeback. A successful `write` is still a cache update.

ToyOS finishes the byte update inside the call. [`vfs_write`](../../projects/toyos/src/vfs.c) stores into `inode->data` before it returns. There is no dirty buffer and no later sync.

The shared [Linux 0.12 → Linux 5.10 map](../../references/EARLY_LINUX_012_MAP.md) marks this as where the early map stops. The sections below are the VFS and the unified page cache in Linux 5.10.134, tree `c9b2385d6793`. Read 0.12 as the first complete picture of a dirty cache. Develop against the 5.10 tree.

Day 1 separated the descriptor, the `struct file`, and the inode. Day 2
separated a requested count from the bytes one attempt actually moved. A
positive `write` on a regular file still has a third gap. The bytes can
sit in RAM, tagged dirty, while the storage underneath has not taken
them. `fsync` is the call that waits. A crash, or a writeback error,
lives in that gap.

The excerpts are from the local tree
`5.10.134-15_rc1-10333-gc9b2385d6793` (commit `c9b2385d6793`). This
revision's `mm/filemap.c` still speaks of `struct page`, `PageDirty`, and
the page cache. Later mainline trees renamed much of that to folios. The
names below are this tree's names.

## 3. Why the cache is not the open file

A path is input to lookup. A dentry remembers a name; `d_inode` is
"where the name belongs" (`include/linux/dcache.h`). An inode is the
object: mode, size, and the operations. An open file is one description
of that object, with its own `f_pos` and `f_count`. None of those is a
cache of the bytes.

The cache hangs off the inode. `inode_init_always` in `fs/inode.c` sets
`mapping` to `&inode->i_data` (line 137), points `mapping->host` back at
the inode (line 182), and stores that pointer in `inode->i_mapping` (line
195). `do_dentry_open` copies it onto the open file (`fs/open.c` line
796, `f->f_mapping = inode->i_mapping`). Two opens of one inode share one
`address_space`. They do not share `f_pos`. Day 1's duplicates share
`f_pos` and also share this cache, because they share the file and
therefore the inode pointer inside it.

```c
/* include/linux/fs.h :: struct address_space :: tree c9b2385d6793
 * lines 470–501
 */
struct address_space {
	struct inode		*host;
	struct xarray		i_pages;
	gfp_t			gfp_mask;
	atomic_t		i_mmap_writable;
#ifdef CONFIG_READ_ONLY_THP_FOR_FS
	/* number of thp, only for non-shmem files */
	atomic_t		nr_thps;
#endif
#ifdef CONFIG_DUPTEXT
	/* number of dup text, in pages */
	atomic_t                nr_duptext;
#endif
	struct rb_root_cached	i_mmap;
	struct rw_semaphore	i_mmap_rwsem;
	unsigned long		nrpages;
	unsigned long		nrexceptional;
	pgoff_t			writeback_index;
	const struct address_space_operations *a_ops;
	unsigned long		flags;
	errseq_t		wb_err;
	spinlock_t		private_lock;
	struct list_head	private_list;
	void			*private_data;

	struct fast_reflink_work *fast_reflink_work;

	CK_KABI_RESERVE(1)
	CK_KABI_RESERVE(2)
	CK_KABI_RESERVE(3)
	CK_KABI_RESERVE(4)
} __attribute__((aligned(sizeof(long)))) __randomize_layout;
```

`i_pages` is the cache, an xarray of pages. `nrpages` counts them.
`a_ops` is how this inode reads a missing page, begins a write, and
writes pages back. `wb_err` is an `errseq_t` for the last writeback
error. `host` is the inode. Just below the struct, lines 508–511 name
the xarray tags: `PAGECACHE_TAG_DIRTY`, `PAGECACHE_TAG_WRITEBACK`, and
`PAGECACHE_TAG_TOWRITE`. Dirty and writeback are marks on the cache, not
bits in the user's buffer.

`i_mmap` is the tree of VM areas that map this file. A buffered `read`
copies from a cache page into a user buffer translated by the process
page tables. The page frame can later be the same frame a PTE names.
The process page tables and `i_pages` are two indexes.

```text
path string                 lookup input
    |
    v
dentry.d_inode  ---------> inode
                             i_mapping -------+
                             i_data           |
                             i_size           v
                                         address_space
                                           i_pages[index] -> struct page
                                           PAGECACHE_TAG_DIRTY
                                           PAGECACHE_TAG_WRITEBACK
                                           wb_err
struct file
  f_pos          one open's cursor
  f_mapping  ---> the inode's address_space
  f_op       ---> read / write / fsync
```

The page index is a file offset in page units. `generic_perform_write`
computes the byte offset inside a page as `pos & (PAGE_SIZE - 1)`.
`grab_cache_page_write_begin` takes that cache position as a `pgoff_t
index`. The process's page tables are not consulted to find it. They are
consulted to find the user buffer the bytes are copied from.

## 4. The method table is where read, write, and fsync meet

```c
/* include/linux/fs.h :: struct file_operations :: tree c9b2385d6793
 * lines 1981–2001
 */
struct file_operations {
	struct module *owner;
	fop_flags_t fop_flags;
	loff_t (*llseek) (struct file *, loff_t, int);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
	ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
	ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
	int (*iopoll)(struct kiocb *kiocb, struct io_comp_batch *,
			unsigned int flags);
	int (*iterate) (struct file *, struct dir_context *);
	int (*iterate_shared) (struct file *, struct dir_context *);
	__poll_t (*poll) (struct file *, struct poll_table_struct *);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	int (*mmap) (struct file *, struct vm_area_struct *);
	unsigned long mmap_supported_flags;
	int (*open) (struct inode *, struct file *);
	int (*flush) (struct file *, fl_owner_t id);
	int (*release) (struct inode *, struct file *);
	int (*fsync) (struct file *, loff_t, loff_t, int datasync);
```

Day 2's `vfs_read` ends by calling one of those read methods:

```c
/* fs/read_write.c :: vfs_read :: tree c9b2385d6793
 * lines 493–505
 */
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

`new_sync_read` calls `read_iter`. For a page-cache filesystem that
method is `generic_file_read_iter` (`mm/filemap.c` lines 2719–2724).
A miss finds or allocates a `struct page` in `i_pages` and copies it out.
`page_cache_sync_ra` (`mm/readahead.c` lines 581–607) widens the fill
through `ondemand_readahead`, unless random access or congestion forces
a smaller read. `f_op->fsync` is a different method: write accepts bytes,
and `fsync` pushes them and reports failure.

## 5. A buffered write copies into a page and marks it dirty

`grab_cache_page_write_begin` is the page-cache vocabulary of this tree:
a `struct page`, a page-cache index, a locked page.

```c
/* mm/filemap.c :: grab_cache_page_write_begin :: tree c9b2385d6793
 * lines 3510–3528
 */
/*
 * Find or create a page at the given pagecache position. Return the locked
 * page. This function is specifically for buffered writes.
 */
struct page *grab_cache_page_write_begin(struct address_space *mapping,
					pgoff_t index, unsigned flags)
{
	struct page *page;
	int fgp_flags = FGP_LOCK|FGP_WRITE|FGP_CREAT;

	if (flags & AOP_FLAG_NOFS)
		fgp_flags |= FGP_NOFS;
	if (flags & AOP_FLAG_DONTCACHE)
		fgp_flags |= FGP_DONTCACHE;

	page = pagecache_get_page(mapping, index, fgp_flags,
			mapping_gfp_mask(mapping));
	if (page)
		wait_for_stable_page(page);
```

`generic_perform_write` is the loop a buffered `write_iter` uses. Each
pass covers one page, faults the user side in first, and copies.

```c
/* mm/filemap.c :: generic_perform_write :: tree c9b2385d6793
 * lines 3556–3588
 */
		offset = (pos & (PAGE_SIZE - 1));
		bytes = min_t(unsigned long, PAGE_SIZE - offset,
						iov_iter_count(i));

again:
		/*
		 * Bring in the user page that we will copy from _first_.
		 * Otherwise there's a nasty deadlock on copying from the
		 * same page as we're writing to, without it being marked
		 * up-to-date.
		 */
		if (unlikely(iov_iter_fault_in_readable(i, bytes))) {
			status = -EFAULT;
			break;
		}

		if (fatal_signal_pending(current)) {
			status = -EINTR;
			break;
		}

		if (iocb->ki_flags & IOCB_DONTCACHE)
			flags |= AOP_FLAG_DONTCACHE;

		status = a_ops->write_begin(file, mapping, pos, bytes, flags,
						&page, &fsdata);
		if (unlikely(status < 0))
			break;

		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);

		copied = iov_iter_copy_from_user_atomic(page, i, offset, bytes);
```

The user pointer is still a virtual address. `iov_iter_fault_in_readable`
walks the caller's page tables and brings the source page in before the
filesystem locks the cache page. That order is the deadlock comment: the
source and the destination might be the same frame if the process has the
file mapped. `fatal_signal_pending` produces `-EINTR` directly, the case
Day 2 distinguished from a pipe's `-ERESTARTSYS`. `write_begin` is the
filesystem method that finds or creates the cache page. The copy then
moves bytes from the user map into that `struct page`.

The function returns `written ? written : status` (line 3617). A positive
count is how far the copy got. `ksys_write` will publish that into
`f_pos`. The page is not yet a completed device write.

For a buffer-head filesystem, `write_end` reaches `__block_commit_write`,
which marks the touched buffers dirty:

```c
/* fs/buffer.c :: __block_commit_write :: tree c9b2385d6793
 * lines 2077–2101
 */
	do {
		block_end = block_start + blocksize;
		if (block_end <= from || block_start >= to) {
			if (!buffer_uptodate(bh))
				partial = 1;
		} else {
			set_buffer_uptodate(bh);
			mark_buffer_dirty(bh);
		}
		if (buffer_new(bh))
			clear_buffer_new(bh);

		block_start = block_end;
		bh = bh->b_this_page;
	} while (bh != head);

	/*
	 * If this is a partial write which happened to make all buffers
	 * uptodate then we can optimize away a bogus readpage() for
	 * the next read(). Here we 'discover' whether the page went
	 * uptodate as a result of this (potentially partial) write.
	 */
	if (!partial)
		SetPageUptodate(page);
	return 0;
```

`mark_buffer_dirty` says what "dirty" means here:

```c
/* fs/buffer.c :: mark_buffer_dirty :: tree c9b2385d6793
 * lines 1099–1111
 */
/**
 * mark_buffer_dirty - mark a buffer_head as needing writeout
 * @bh: the buffer_head to mark dirty
 *
 * mark_buffer_dirty() will set the dirty bit against the buffer, then set
 * its backing page dirty, then tag the page as dirty in the page cache
 * and then attach the address_space's inode to its superblock's dirty
 * inode list.
 *
 * mark_buffer_dirty() is atomic.  It takes bh->b_page->mapping->private_lock,
 * i_pages lock and mapping->host->i_lock.
 */
void mark_buffer_dirty(struct buffer_head *bh)
```

The tag is `PAGECACHE_TAG_DIRTY` on `i_pages`. The page's `PageDirty` bit
is set along the way (`TestSetPageDirty` in the body at line 1134). The
inode joins the superblock's dirty list. All of that is bookkeeping in
memory. `generic_write_end` may also extend `i_size` while the page is
still locked (the comment at lines 2183–2185), then `mark_inode_dirty` if
the size changed. A larger `i_size` is metadata. It is not a promise that
the device has the new bytes.

`balance_dirty_pages_ratelimited` can start cleaning so the cache does
not grow without bound. That cleaning is writeback. The `write` syscall
can return while tagged pages remain.

## 6. Writeback moves dirty pages; `fsync` waits

```c
/* mm/filemap.c :: __filemap_fdatawrite_range :: tree c9b2385d6793
 * lines 414–433
 */
int __filemap_fdatawrite_range(struct address_space *mapping, loff_t start,
				loff_t end, int sync_mode)
{
	int ret;
	struct writeback_control wbc = {
		.sync_mode = sync_mode,
		.nr_to_write = LONG_MAX,
		.range_start = start,
		.range_end = end,
	};

	if (!mapping_can_writeback(mapping) ||
	    !mapping_tagged(mapping, PAGECACHE_TAG_DIRTY))
		return 0;

	wbc_attach_fdatawrite_inode(&wbc, mapping->host);
	ret = do_writepages(mapping, &wbc);
	wbc_detach_inode(&wbc);
	return ret;
}
```

The comment above this function (lines 404–410) calls `WB_SYNC_ALL` a
data-integrity operation: a dirty page must be waited on, not skipped.
`do_writepages` is the handoff from the cache to `a_ops`. Below that call
are the filesystem's block mapping and the block device. This lesson
stops at that handoff. The page-cache fact is the tag. Starting writeback
moves a page from the dirty tag toward `PAGECACHE_TAG_WRITEBACK`.

Waiting is a second step:

```c
/* mm/filemap.c :: filemap_fdatawait_range :: tree c9b2385d6793
 * lines 561–582
 */
/**
 * filemap_fdatawait_range - wait for writeback to complete
 * @mapping:		address space structure to wait for
 * @start_byte:		offset in bytes where the range starts
 * @end_byte:		offset in bytes where the range ends (inclusive)
 *
 * Walk the list of under-writeback pages of the given address space
 * in the given range and wait for all of them.  Check error status of
 * the address space and return it.
 *
 * Since the error status of the address space is cleared by this function,
 * callers are responsible for checking the return value and handling and/or
 * reporting the error.
 *
 * Return: error status of the address space.
 */
int filemap_fdatawait_range(struct address_space *mapping, loff_t start_byte,
			    loff_t end_byte)
{
	__filemap_fdatawait_range(mapping, start_byte, end_byte);
	return filemap_check_errors(mapping);
}
```

`__filemap_fdatawait_range` looks up `PAGECACHE_TAG_WRITEBACK` and calls
`wait_on_page_writeback` on each `struct page`. `filemap_check_errors`
(lines 373–384) returns `-ENOSPC` if `AS_ENOSPC` is set and `-EIO` if
`AS_EIO` is set, clearing the bit it reports. The comment says the caller
must check that return. A wait that ignores it has observed the sleep and
dropped the error.

The syscall entry does not call these helpers by name. It calls the
filesystem method:

```c
/* fs/sync.c :: do_fsync, fsync, fdatasync :: tree c9b2385d6793
 * lines 220–240
 */
static int do_fsync(unsigned int fd, int datasync)
{
	struct fd f = fdget(fd);
	int ret = -EBADF;

	if (f.file) {
		ret = vfs_fsync(f.file, datasync);
		fdput(f);
	}
	return ret;
}

SYSCALL_DEFINE1(fsync, unsigned int, fd)
{
	return do_fsync(fd, 0);
}

SYSCALL_DEFINE1(fdatasync, unsigned int, fd)
{
	return do_fsync(fd, 1);
}
```

`vfs_fsync` calls `vfs_fsync_range(file, 0, LLONG_MAX, datasync)`. That
helper's comment (lines 184–192) says it writes back data in the range
and metadata for the file, and that `datasync` limits metadata to what is
needed to access the modified data. The code then requires `f_op->fsync`
and calls it. `fsync` passes `datasync = 0`. `fdatasync` passes 1. On
x86-64 those syscalls are numbers 74 and 75 in `syscall_64.tbl`. The
generic table numbers them 82 and 83. The descriptor is still Day 1's
index. `fdget` turns it into the `struct file` whose `f_mapping` names
the cache.

## 7. The failure is recorded on the mapping

A writeback error is stored where a later `fsync` can find it.

```c
/* include/linux/pagemap.h :: mapping_set_error :: tree c9b2385d6793
 * lines 39–70
 */
/**
 * mapping_set_error - record a writeback error in the address_space
 * @mapping: the mapping in which an error should be set
 * @error: the error to set in the mapping
 *
 * When writeback fails in some way, we must record that error so that
 * userspace can be informed when fsync and the like are called.  We endeavor
 * to report errors on any file that was open at the time of the error.  Some
 * internal callers also need to know when writeback errors have occurred.
 *
 * When a writeback error occurs, most filesystems will want to call
 * mapping_set_error to record the error in the mapping so that it can be
 * reported when the application calls fsync(2).
 */
static inline void mapping_set_error(struct address_space *mapping, int error)
{
	if (likely(!error))
		return;

	/* Record in wb_err for checkers using errseq_t based tracking */
	__filemap_set_wb_err(mapping, error);

	/* Record it in superblock */
	if (mapping->host)
		errseq_set(&mapping->host->i_sb->s_wb_err, error);

	/* Record it in flags for now, for legacy callers */
	if (error == -ENOSPC)
		set_bit(AS_ENOSPC, &mapping->flags);
	else
		set_bit(AS_EIO, &mapping->flags);
}
```

Three records: `mapping->wb_err`, the superblock's `s_wb_err`, and the
legacy `AS_ENOSPC` / `AS_EIO` bits that `filemap_check_errors` reads.
`mark_buffer_write_io_error` in `fs/buffer.c` (lines 1146–1161) calls
`mapping_set_error` with `-EIO` when a buffer write fails. The `write`
that dirtied the page may already have returned a positive count. The
error belongs to a later integrity call.

`__delete_from_page_cache`'s helper states the loss plainly. The comment
at `mm/filemap.c` lines 219–222 says a dirty page at deletion "signals a
bug and loss of unwritten data." Dirty means the bytes are still the
cache's responsibility. Dropping the page before writeback finishes drops
the only copy.

That is the failure boundary. A positive `write` means the copy and the
dirty tag happened. `fsync` returning 0 means `f_op->fsync` returned 0.
`-EIO` or `-ENOSPC` means an error recorded on the mapping, or an error
the method returned, reached the syscall. Between those two returns the
dirty page is still RAM.

## 8. A hole is an extent fact, not a missing cache page

`generic_file_llseek_size` has no hole map of its own. Its `SEEK_DATA`
arm (`fs/read_write.c` lines 111–118) treats every offset below the end
as data. Its `SEEK_HOLE` arm (lines 119–127) says there is a virtual hole
at the end of the file and, when the offset is below that end, returns
`i_size`. A filesystem that stores holes replaces `llseek`. ext4 does:

```c
/* fs/ext4/file.c :: ext4_llseek :: tree c9b2385d6793
 * lines 868–888
 */
loff_t ext4_llseek(struct file *file, loff_t offset, int whence)
{
	struct inode *inode = file->f_mapping->host;
	loff_t maxbytes = ext4_get_maxbytes(inode);

	switch (whence) {
	default:
		return generic_file_llseek_size(file, offset, whence,
						maxbytes, i_size_read(inode));
	case SEEK_HOLE:
		inode_lock_shared(inode);
		offset = iomap_seek_hole(inode, offset,
					 &ext4_iomap_report_ops);
		inode_unlock_shared(inode);
		break;
	case SEEK_DATA:
		inode_lock_shared(inode);
		offset = iomap_seek_data(inode, offset,
					 &ext4_iomap_report_ops);
		inode_unlock_shared(inode);
		break;
```

The inode lock is shared. The answer comes from `iomap_seek_hole`.
`fs/ext4/extents_status.c` lines 20–38 describe the extent-status tree
that answers `SEEK_DATA` / `SEEK_HOLE`. A hole is a range of file offsets
with no allocated blocks. `i_size`, that map, and `i_pages` are three
records of the file.

## 9. Execution trace: write, dirty, writeback, fsync, error

One regular file, buffer-head `a_ops`, descriptor 3, `f_pos` at 0. The
user calls `write(3, buf, 4096)` and the copy succeeds. Then `fsync(3)`.

```text
write(3, buf, 4096)
  ksys_write -> vfs_write -> write_iter
    generic_perform_write
      offset = 0 within the page
      iov_iter_fault_in_readable(user buf)     page tables of current->mm
      a_ops->write_begin                       grab or create struct page
                                               at this page-cache index
      copy from user into that page
      __block_commit_write
        mark_buffer_dirty
          PageDirty, PAGECACHE_TAG_DIRTY
          inode on the superblock dirty list
      i_size updated if the write extended the file
    return 4096
  f_pos = 4096
  user sees 4096                               the device has not been asked

later, memory pressure or a flusher
  __filemap_fdatawrite_range
    mapping is tagged DIRTY
    do_writepages                 filesystem block mapping, then the device

fsync(3), ext4
  do_fsync(fd, 0) -> ext4_sync_file          fs/ext4/fsync.c
    file_write_and_wait_range                mm/filemap.c 824–839
      __filemap_fdatawrite_range(WB_SYNC_ALL)
      __filemap_fdatawait_range
      file_check_and_advance_wb_err
    journal commit, or ext4_fsync_nojournal
    blkdev_issue_flush if needs_barrier
    file_check_and_advance_wb_err once more
  return 0, or -EIO / -ENOSPC from that cursor

power loss after write returned and before fsync returned
  the dirty page was still RAM
```

The positive `write` and the successful `fsync` are two syscalls. Code
that treats the first as durability has stopped at the dirty tag.
Code that ignores `fsync`'s return has waited and dropped
`filemap_check_errors`.

## 10. A small C picture of the two calls

```c
#include <unistd.h>

int store(int fd, const void *buf, size_t count)
{
    const char *p = buf;
    size_t got = 0;

    while (got < count) {
        ssize_t n = write(fd, p + got, count - got);
        if (n < 0)
            return -1;
        if (n == 0)
            return -1;
        got += (size_t)n;
    }
    return fsync(fd);
}
```

`write` looping until `got == count` is Day 2: each positive return is
progress in the page cache and in `f_pos`. `fsync` is the integrity
call on the same descriptor. Its zero means the method returned zero.
`fdatasync` is the same path with `datasync` set. Neither call is a
substitute for checking the other's return. A short `write` that is not
retried leaves a prefix dirty and the tail unsent. An `fsync` error
after a successful `write` means the prefix was accepted into the cache
and the integrity path later failed.

## 11. Registers, locks, and ToyOS

`fsync`'s descriptor arrives in the same argument register as `read`'s:
`rdi` on x86-64, `x0` on AArch64, `a0` on RISC-V. The syscall number is
in `rax`, `regs[8]`, or `a7`. The return is 0 or a negative errno. No
user buffer is copied. The objects are reached from `current->files`.

`generic_write_end` updates `i_size` while the page is locked so
writeback cannot zero past `i_size` (`fs/buffer.c` lines 2183–2185).
`mark_buffer_dirty` takes `mapping->private_lock`, the `i_pages` lock,
and `mapping->host->i_lock`. `ext4_llseek` takes `inode_lock_shared`.
The user buffer is translated by the process page tables. The cache page
is found through `i_pages`. When `mapping_writably_mapped` is set, the
write flushes the data cache so a user mapping of that page sees the new
bytes.

ToyOS writes `inode->data` before
[`vfs_write`](../../projects/toyos/src/vfs.c) returns. The offset lives
on the open slot. Linux returns at the dirty tag and waits when the
program calls `fsync`.

## 12. Mastery: Explain · Draw · Observe · Build

Run [`../challenges/day-003-page-cache.sh`](../challenges/day-003-page-cache.sh)
after this explanation.

### Explain

The page cache is `inode->i_mapping`, an `address_space` whose `i_pages`
hold `struct page` objects tagged dirty or writeback. `struct file`
points at that mapping and keeps its own `f_pos`. A buffered `write`
copies into a cache page, `mark_buffer_dirty` tags it, and `write` can
return. `__filemap_fdatawrite_range` starts writeback through
`do_writepages`. `fsync` and `fdatasync` are `do_fsync` with `datasync`
0 and 1, and they call `f_op->fsync`. `mapping_set_error` records a
writeback failure on `wb_err` and on `AS_EIO` / `AS_ENOSPC`. A hole, on
ext4, is an extent-map answer from `iomap_seek_hole`. The generic seek
has one virtual hole at `i_size`.

### Draw

1. Path, dentry, inode, `address_space`, and two `struct file`s sharing
   `f_mapping` with different `f_pos` values.
2. One page: user buffer via the process page tables, cache page via
   `i_pages`, dirty tag, then writeback tag.
3. The timeline: `write` returns, writeback runs, `fsync` waits, and the
   arrow where `mapping_set_error` is reported.

### Observe

On the lab run, record a `write` return and the later `fsync` return as
two observations. Note whether a short `write` was retried before
`fsync`. If the run reports a writeback error, name `mapping_set_error`
and `filemap_check_errors` as the record and the report.

### Build

In the C picture, split `store` so the caller can tell a `write` error
from an `fsync` error. Predict `f_pos` and the dirty tag after `write`
returns 4096 and before `fsync` is called.

### Why? notebook

1. Why do two opens of one inode share `i_pages` and not `f_pos`?
2. Why can `write` return before `do_writepages` runs?
3. Why does `generic_perform_write` fault the user page in before
   `write_begin`?
4. Why is `PAGECACHE_TAG_DIRTY` a different mark from
   `PAGECACHE_TAG_WRITEBACK`?
5. Why does `filemap_fdatawait_range` return `filemap_check_errors`?
6. Why do `fsync` and `fdatasync` differ by the `datasync` argument?
7. Why does generic `SEEK_HOLE` return `i_size`, while ext4 calls
   `iomap_seek_hole`?
8. Why does ToyOS finish the byte update inside `vfs_write`?

**Next:** the descriptor, the short transfer, and the dirty page are the
three objects later weeks use when they talk about readiness and durable
storage.
