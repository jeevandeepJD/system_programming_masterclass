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

run_expect "pthread-create-join" "pthread_create_join=ok" ./build/thread_lab basic
run_expect "mutex-counter" "mutex_counter=800000 expected=800000" ./build/thread_lab mutex
run_expect "atomic-counter" "atomic_counter=800000 expected=800000" ./build/thread_lab atomic
run_expect "thread-ownership" "joined_value=42" ./build/thread_lab ownership
run_expect "condition-bounded-queue" "condition_waits=while_predicate_loops" ./build/queue_lab
run_expect "deadlock-safe-detection" "test_hung=no" ./build/deadlock_lab detect
run_expect "global-lock-order" "completed=yes" ./build/deadlock_lab ordered
run_expect "false-sharing-caveat" "not_a_correctness_test" ./build/thread_lab false-sharing
run_expect "memory-order-litmus" "not_C_language_proof" ./build/litmus_lab
printf 'INFO race mode is intentionally excluded: run under ThreadSanitizer via make tools\n'
printf 'WEEK11 PASS\n'
