#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

check_demo() {
    label=$1
    bin=$2
    shift 2
    output=$("./build/$bin" demo)
    printf '%s\n' "$output"
    for pattern in "$@"; do
        printf '%s\n' "$output" | grep -F -- "$pattern" >/dev/null
    done
    printf 'PASS %s\n' "$label"
}

check_demo "descriptors" descriptor_lab \
    "dup_shares_offset=yes" \
    "lseek_shared=yes" \
    "separate_open_independent=yes" \
    "descendants=none" \
    "temp_cleanup=done"

check_demo "stream-io" stream_lab \
    "short_write_lt_request=yes" \
    "eof=0" \
    "nonblock_read=EAGAIN" \
    "eintr=yes" \
    "robust_transfer=262144" \
    "robust_saw_short_or_eagain=yes" \
    "children_reaped=yes" \
    "temp_files=none"

check_demo "page-cache" pagecache_lab \
    "buffered_readback=match" \
    "visibility=page_cache_not_crash_durability" \
    "fsync_completed=yes" \
    "fsync_is_not_crash_proof=yes" \
    "stat_matches_fstat=yes" \
    "hole_byte=0" \
    "sparse_logical_size=1048577" \
    "allocated_blocks_lt_logical=yes" \
    "durability_claim=none" \
    "descendants=none" \
    "temp_cleanup=done"

check_demo "pipes" pipe_lab \
    "dup_write_end_shares_pipe=yes" \
    "writer_remaining_not_eof=EAGAIN" \
    "all_writers_closed_eof=0" \
    "dup2_stdout=redir-ok" \
    "backpressure=EAGAIN" \
    "pipe_buf_atomic_eagain=yes" \
    "pipe_buf_atomic_full=yes" \
    "children_reaped=yes" \
    "temp_files=none"

check_demo "readiness" readiness_lab \
    "ready_short_read=4 requested=100" \
    "select_empty=0" \
    "select_ready=1" \
    "poll_ready=1" \
    "poll_empty=0" \
    "epoll_lt_partial=ready" \
    "epoll_et_partial=not_ready" \
    "epoll_et_drain=EAGAIN" \
    "epoll_et_new_edge=ready" \
    "limit=readiness_is_not_a_full_transfer" \
    "descendants=none" \
    "temp_files=none"

check_demo "mmap" mmap_lab \
    "mmap_store_visible_to_pread=yes" \
    "msync_completed=yes" \
    "msync_is_not_crash_proof=yes" \
    "pwrite_visible_to_mapping=yes" \
    "performance_claim=none" \
    "timing=single_run_not_universal" \
    "descendants=none" \
    "temp_cleanup=done"

check_demo "io-debug" debug_lab \
    "proc_fd_matches=yes" \
    "proc_self_fd_listed=yes" \
    "deliberate_ebadf=yes" \
    "deliberate_epipe=yes" \
    "epoll_bad_fd=EBADF" \
    "epoll_regular_file=EPERM" \
    "epoll_wait_closed=EBADF" \
    "limit=errno_is_the_evidence" \
    "descendants=none" \
    "temp_cleanup=done"

for item in /tmp/week12-desc.* /tmp/week12-page.* /tmp/week12-mmap.* /tmp/week12-debug.*; do
    if [ -e "$item" ]; then
        printf 'FAIL leftover %s\n' "$item" >&2
        exit 1
    fi
done

printf 'WEEK12 PASS\n'
