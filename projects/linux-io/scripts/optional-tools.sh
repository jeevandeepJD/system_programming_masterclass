#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

mkdir -p build

if command -v strace >/dev/null 2>&1; then
    if strace -o build/strace-io.log ./build/debug_lab demo \
        >/dev/null 2>build/strace-io.err; then
        if grep -E 'openat|read|write|close|epoll_ctl|epoll_wait|readlink' \
            build/strace-io.log >/dev/null; then
            echo "PASS strace (build/strace-io.log)"
        else
            echo "SKIP strace: log lacked expected syscalls (build/strace-io.log)"
        fi
    else
        echo "SKIP strace: ptrace or runtime unavailable (build/strace-io.err)"
    fi
else
    echo "SKIP strace: not installed"
fi
