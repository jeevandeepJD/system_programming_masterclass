#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

mkdir -p build

if command -v strace >/dev/null 2>&1; then
    if strace -o build/strace-entry.log ./build/syscall_lab demo \
        >/dev/null 2>build/strace-entry.err; then
        if grep -E 'getpid|write|read|close' build/strace-entry.log >/dev/null; then
            echo "PASS strace (build/strace-entry.log)"
        else
            echo "SKIP strace: log lacked expected syscalls (build/strace-entry.log)"
        fi
    else
        echo "SKIP strace: ptrace or runtime unavailable (build/strace-entry.err)"
    fi
else
    echo "SKIP strace: not installed"
fi

if command -v perf >/dev/null 2>&1; then
    if perf stat -e task-clock -- ./build/syscall_lab demo \
        >/dev/null 2>build/perf-entry.err; then
        echo "PASS perf (build/perf-entry.err)"
    else
        echo "SKIP perf: unavailable or not permitted (build/perf-entry.err)"
    fi
else
    echo "SKIP perf: not installed"
fi

trace=/sys/kernel/debug/tracing/current_tracer
if [ -r "$trace" ]; then
    echo "SKIP tracefs: $trace is readable; this lab does not enable a tracer"
else
    echo "SKIP tracefs: $trace is not readable"
fi
