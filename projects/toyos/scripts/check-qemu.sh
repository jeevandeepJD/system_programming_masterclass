#!/usr/bin/env bash
set -u

if [[ $# -ne 3 ]]; then
    echo "usage: $0 ISO EXPECTED-QEMU-STATUS EXPECTED-LOG" >&2
    exit 2
fi

iso=$1
expected_status=$2
expected_log=$3
log=$(mktemp)
trap 'rm -f "$log"' EXIT

set +e
timeout 12s qemu-system-x86_64 \
    -machine accel=tcg \
    -m 128M \
    -display none \
    -serial stdio \
    -monitor none \
    -no-reboot \
    -device isa-debug-exit \
    -cdrom "$iso" >"$log" 2>&1
status=$?
set -e

cat "$log"
if [[ $status -ne $expected_status ]]; then
    echo "FAIL: QEMU status $status, expected $expected_status" >&2
    exit 1
fi
if ! grep -Fq "$expected_log" "$log"; then
    echo "FAIL: missing serial marker: $expected_log" >&2
    exit 1
fi
echo "LOG-CHECK: $expected_log"
