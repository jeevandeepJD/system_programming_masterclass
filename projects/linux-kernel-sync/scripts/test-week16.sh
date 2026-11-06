#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
for program in primitive_select message_order spinlock_lab mutex_pi_lab refseq_lab epoch_rcu_lab debug_report; do
  printf 'RUN %s\n' "$program"
  "./build/$program"
done
printf 'PASS test-week16\n'
