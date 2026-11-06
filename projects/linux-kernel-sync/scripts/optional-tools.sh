#!/bin/sh
set -u
for tool in perf trace-cmd bpftrace lockstat; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    printf 'SKIP %s missing; not run automatically\n' "$tool"
  else
    printf 'SKIP %s available but not run automatically; requires explicit learner action and suitable permission\n' "$tool"
  fi
done
