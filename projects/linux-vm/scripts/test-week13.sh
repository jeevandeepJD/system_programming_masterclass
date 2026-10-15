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

check_demo "address-space" address_lab \
    "page_size=" \
    "maps_contains_anon=yes" \
    "anon_perms_writable=yes" \
    "smaps_size_matches_page=yes" \
    "anon_untouched_resident=no" \
    "maps_has_stack=yes" \
    "heap_presence_not_a_glibc_contract=yes" \
    "descendants=none" \
    "temp_files=none"

check_demo "page-faults" fault_lab \
    "anon_untouched_resident=no" \
    "anon_untouched_rss_kb=0" \
    "zero_fill=yes" \
    "minor_fault_delta_ge_1=yes" \
    "fault_count_caveat=other_activity_can_raise_the_count" \
    "anon_fault_is_not_a_disk_claim=yes" \
    "read_rss_not_asserted=shared_zero_page_or_private" \
    "anon_store_visible=yes" \
    "anon_touched_resident=yes" \
    "touched_rss_matches_page=yes" \
    "descendants=none" \
    "temp_files=none"

check_demo "mappings" mapping_lab \
    "shared_child_store_visible=yes" \
    "private_parent_unchanged=yes" \
    "private_child_store=yes" \
    "mprotect_child_sigsegv=yes" \
    "parent_read_survived=yes" \
    "mprotect_fault_in_child_only=yes" \
    "children_reaped=yes" \
    "temp_cleanup=done"

check_demo "allocators" allocator_lab \
    "brk_changed=" \
    "bounded_alloc_bytes=2097152" \
    "glibc_mmap_threshold_asserted=no" \
    "regions_are_observations=yes" \
    "malloc_lifetime_released=yes" \
    "explicit_mmap_in_maps=yes" \
    "munmap_absent_from_maps=yes" \
    "descendants=none" \
    "temp_files=none"

check_demo "buddy-numa" buddy_lab \
    "policy=teaching_buddy_not_kernel_fallback" \
    "simulator_only=yes" \
    "host_pages_untouched=yes" \
    "buddy_split=yes" \
    "buddy_coalesce=yes" \
    "numa_local_hit=yes" \
    "numa_fallback=yes" \
    "dma_request_uses_dma=yes" \
    "kernel_request_skips_dma=yes" \
    "exhausted_alloc=fail" \
    "simulator_balanced=yes"

check_demo "reclaim" reclaim_lab \
    "frames=4" \
    "swap_slots=2" \
    "policy=teaching_lru_not_kernel_scan" \
    "evict_dirty_file=writeback_then_drop" \
    "evict_clean_file=drop" \
    "evict_anon=swap" \
    "swap_full_anon=simulated_oom_choice" \
    "simulated_oom_choice=task_b" \
    "oom_kills_host_process=no" \
    "writeback_count=1" \
    "clean_drop_count=1" \
    "swapout_count=2" \
    "simulated_oom_choices=1" \
    "reclaim_host=no" \
    "swap_host=no" \
    "pressure_induced=no" \
    "simulator_only=yes"

check_demo "vm-debug" debug_lab \
    "maps_anon=yes" \
    "zero_fill=yes" \
    "touched_rss_matches_page=yes" \
    "status_has_vmrss=yes" \
    "status_has_vmsize=yes" \
    "counters_are_observations=yes" \
    "pressure_induced=no" \
    "oom_induced=no" \
    "swap_induced=no" \
    "vm_observation=proc_self_only" \
    "descendants=none" \
    "temp_files=none"

for item in /tmp/week13-map.*; do
    if [ -e "$item" ]; then
        printf 'FAIL leftover %s\n' "$item" >&2
        exit 1
    fi
done

printf 'WEEK13 PASS\n'
