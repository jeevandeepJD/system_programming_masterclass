#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

mkdir -p build
echo "observation_only=yes"
echo "optional_tools_do_not_induce_pressure=yes"

if [ -r /proc/vmstat ]; then
    echo "vmstat_scope=system_wide"
    if grep -E '^(pgfault|pgmajfault|pswpin|pswpout) ' /proc/vmstat \
        >build/vmstat-excerpt.txt; then
        cat build/vmstat-excerpt.txt
        echo "PASS vmstat-observation"
        echo "LIMIT: these counters belong to the whole machine, not to this lab."
    else
        echo "SKIP vmstat: expected keys were absent"
    fi
else
    echo "SKIP vmstat: /proc/vmstat unreadable"
fi

if command -v perf >/dev/null 2>&1; then
    if perf stat -e page-faults,minor-faults,major-faults -- \
        ./build/fault_lab demo >build/perf-fault.txt 2>build/perf-fault.err; then
        if grep -E 'page-faults|minor-faults' build/perf-fault.err >/dev/null; then
            echo "PASS perf-stat-observation"
            echo "perf_claim=single_run_not_universal"
            echo "LIMIT: perf counted one fault_lab run. It is not a rate and not a kernel trace."
        else
            echo "SKIP perf: counters missing (build/perf-fault.err)"
        fi
    else
        echo "SKIP perf: unavailable or denied (build/perf-fault.err)"
    fi
else
    echo "SKIP perf: not installed"
fi
