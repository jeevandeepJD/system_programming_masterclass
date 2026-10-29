#!/bin/sh
set -u
for tool in ps taskset perf trace-cmd bpftrace; do
  if ! command -v "$tool" >/dev/null 2>&1; then printf 'SKIP %s missing\n' "$tool"; continue; fi
  case "$tool" in
    ps) ps -o pid,stat,ni,psr,comm -p $$ || printf 'SKIP ps failed\n' ;;
    taskset) taskset -pc $$ || printf 'SKIP taskset failed\n' ;;
    *) printf 'SKIP %s not run automatically; optional tracing needs host permission and explicit learner action\n' "$tool" ;;
  esac
done
