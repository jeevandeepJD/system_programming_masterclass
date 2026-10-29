#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
for p in scheduler_sim wakeup_lab switch_lab affinity_lab inversion_lab report_lab; do
  printf 'RUN %s\n' "$p"
  "./build/$p"
done
printf 'PASS test-week15\n'
