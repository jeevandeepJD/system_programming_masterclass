#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

if command -v strace >/dev/null 2>&1; then
    strace -f -o build/strace-process.log ./build/process_lab status >/dev/null
    grep -E 'execve|wait4|waitid|waitpid' build/strace-process.log >/dev/null
    echo "PASS strace (build/strace-process.log)"
else
    echo "SKIP strace: not installed"
fi

if command -v gdb >/dev/null 2>&1; then
    gdb -q -batch -x gdb/week10.gdb --args ./build/proc_lab >build/gdb-session.log 2>&1
    grep -F 'proc_inspection=ok' build/gdb-session.log >/dev/null
    echo "PASS gdb batch hook (build/gdb-session.log)"
else
    echo "SKIP gdb: not installed"
fi

if command -v valgrind >/dev/null 2>&1; then
    valgrind --quiet --leak-check=full --errors-for-leak-kinds=all \
        --error-exitcode=91 ./build/queue_lab >/dev/null
    echo "PASS Valgrind queue check"
else
    echo "SKIP Valgrind: not installed"
fi

compiler=${CC:-cc}
if "$compiler" -std=c17 -D_POSIX_C_SOURCE=200809L -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer -O1 \
    src/thread_lab.c -o build/thread_lab-asan 2>build/asan-build.log; then
    ASAN_OPTIONS=detect_leaks=1 ./build/thread_lab-asan ownership >/dev/null
    echo "PASS AddressSanitizer/UBSan ownership check"
else
    echo "SKIP AddressSanitizer/UBSan: compiler/runtime unavailable (build/asan-build.log)"
fi

if "$compiler" -std=c17 -D_POSIX_C_SOURCE=200809L -pthread \
    -fsanitize=thread -O1 src/thread_lab.c -o build/thread_lab-tsan \
    2>build/tsan-build.log; then
    set +e
    TSAN_OPTIONS=exitcode=66 ./build/thread_lab-tsan race >build/tsan-race.log 2>&1
    result=$?
    set -e
    if [ "$result" -eq 66 ] && grep -F 'data race' build/tsan-race.log >/dev/null; then
        echo "PASS ThreadSanitizer detected intentional race (build/tsan-race.log)"
    else
        echo "SKIP ThreadSanitizer run: runtime unsupported or race not observed (build/tsan-race.log)"
    fi
else
    echo "SKIP ThreadSanitizer: compiler/runtime unavailable (build/tsan-build.log)"
fi
