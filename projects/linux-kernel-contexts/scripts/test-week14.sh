#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

LINUX_SRC=${LINUX_SRC:-/home/jd/Documents/linux-6.12}

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

if [ ! -f "$LINUX_SRC/Makefile" ]; then
    printf 'FAIL missing %s/Makefile\n' "$LINUX_SRC" >&2
    exit 1
fi

before=$(sha256sum "$LINUX_SRC/Makefile" | awk 'NR==1 { print $1 }')

check_demo "syscall" syscall_lab \
    "getpid_matches=yes" \
    "pipe_byte=k" \
    "syscall_completed=yes" \
    "userspace_syscall=yes" \
    "entry_path_claim=none" \
    "descendants=none"

check_demo "fault" fault_lab \
    "child_sigsegv=yes" \
    "child_sigusr1_caught=yes" \
    "children_reaped=yes" \
    "fault_is_userspace_observation=yes" \
    "irq_claim=none"

check_demo "analog" analog_lab \
    "analog=userspace_poll" \
    "not_an_irq_handler=yes" \
    "not_a_softirq=yes" \
    "not_a_workqueue=yes" \
    "eventfd_empty_poll=0" \
    "eventfd_ready_poll=1" \
    "eventfd_value=1" \
    "timerfd_poll=ready" \
    "descendants=none"

check_demo "context" context_lab \
    "rule_hardirq_may_sleep=no" \
    "rule_softirq_may_sleep=no" \
    "rule_wq_bh_may_sleep=no" \
    "rule_worker_thread_may_sleep=yes" \
    "rule_process_may_sleep=yes" \
    "zero_poll_immediate=yes" \
    "blocking_poll_waited=yes" \
    "userspace_poll_is_not_kernel_context=yes" \
    "kernel_preempt_claim=none" \
    "scheduler_dive=deferred_to_week_15" \
    "descendants=none"

if [ -r /proc/interrupts ]; then
    check_demo "interrupts" interrupts_lab \
        "proc_interrupts=readable" \
        "cpu_header=yes" \
        "irq_rows_observed=yes" \
        "counts_not_asserted=yes" \
        "proc_read_is_not_irq_handler=yes" \
        "kernel_source_modified=no"
    interrupt_report="proc_interrupts=readable"
else
    check_demo "interrupts" interrupts_lab \
        "proc_interrupts=unreadable" \
        "cpu_header=no" \
        "irq_rows_observed=no" \
        "counts_not_asserted=yes" \
        "proc_read_is_not_irq_handler=yes" \
        "kernel_source_modified=no"
    interrupt_report="proc_interrupts=unreadable"
fi

if [ -r /proc/self/status ]; then
    switch_field="context_switch_field=present"
else
    switch_field="context_switch_field=absent"
fi

check_demo "report" report_lab \
    "host_observed=yes" \
    "$interrupt_report" \
    "$switch_field" \
    "tracefs_written=no" \
    "modules_loaded=no" \
    "network_used=no" \
    "kernel_source_modified=no" \
    "scheduler_dive=deferred_to_week_15" \
    "mastery_claim=none"

./scripts/check-source.sh all > build/source-map.txt
for day in 1 2 3 4 5 6 7; do
    grep -F -x -- "day=$day" build/source-map.txt >/dev/null
done
grep -F -x -- "tree=v6.12" build/source-map.txt >/dev/null
grep -F -x -- "commit=adc218676eef25575469234709c2d87185ca223a" build/source-map.txt >/dev/null
grep -F -x -- "exact=VERSION = 6" build/source-map.txt >/dev/null
grep -F -x -- "exact=PATCHLEVEL = 12" build/source-map.txt >/dev/null
grep -F -x -- "exact=SUBLEVEL = 0" build/source-map.txt >/dev/null
cp build/source-map.txt build/source-map-again.txt
./scripts/check-source.sh all > build/source-map-again.txt
cmp build/source-map.txt build/source-map-again.txt
printf 'PASS source-map\n'

after=$(sha256sum "$LINUX_SRC/Makefile" | awk 'NR==1 { print $1 }')
if [ "$before" != "$after" ]; then
    printf 'FAIL kernel Makefile changed\n' >&2
    exit 1
fi
printf 'PASS source-unmodified\n'

fake=$(mktemp -d /tmp/week14-fake.XXXXXX)
printf 'VERSION = 6\n' > "$fake/Makefile"
if LINUX_SRC="$fake" ./scripts/check-source.sh 1 >/tmp/week14-fake.out 2>/tmp/week14-fake.err; then
    rm -rf "$fake"
    rm -f /tmp/week14-fake.out /tmp/week14-fake.err
    printf 'FAIL negative source check accepted a partial tree\n' >&2
    exit 1
fi
rm -rf "$fake"
rm -f /tmp/week14-fake.out /tmp/week14-fake.err
if [ -e "$fake" ]; then
    printf 'FAIL leftover %s\n' "$fake" >&2
    exit 1
fi
printf 'PASS negative-tree\n'

if grep -E -n 'sudo|insmod|modprobe|rmmod|curl |wget ' \
    src/* scripts/check-source.sh scripts/optional-tools.sh Makefile >/dev/null; then
    printf 'FAIL risky command in project sources\n' >&2
    exit 1
fi
printf 'PASS no-risky-commands\n'
printf 'WEEK14 PASS\n'
