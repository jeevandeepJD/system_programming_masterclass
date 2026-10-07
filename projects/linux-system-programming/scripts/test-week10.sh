#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

run_expect() {
    label=$1
    pattern=$2
    shift 2
    output=$("$@")
    printf '%s\n' "$output"
    printf '%s\n' "$output" | grep -F "$pattern" >/dev/null
    printf 'PASS %s\n' "$label"
}

run_expect "fork-exec-wait-status" "signal_exit=SIGTERM" ./build/process_lab status
run_expect "process-tree-pids" "node=" ./build/process_lab tree
run_expect "controlled-zombie" "zombie_reaped=yes" ./build/process_lab zombie
run_expect "controlled-reparenting" "reaped=yes" ./build/process_lab orphan
run_expect "process-failure-injection" "wait_failure=ECHILD" ./build/process_lab failures
run_expect "blocked-pending-delivery" "handled_after_unblock=1" ./build/signal_lab pending
run_expect "async-safe-self-pipe" "async_handler_used=write" ./build/signal_lab self-pipe
run_expect "signal-failure-injection" "kill_missing_pid=ESRCH" ./build/signal_lab failures
run_expect "proc-inspection" "proc_inspection=ok" ./build/proc_lab
printf 'WEEK10 PASS\n'
