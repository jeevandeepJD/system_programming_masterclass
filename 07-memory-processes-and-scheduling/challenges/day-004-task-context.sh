#!/usr/bin/env bash
# Verify C context layout, switch offsets, stack construction, and task identity.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
FAULT_ELF="$TOYOS/build/week7-context/toyos.elf"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

printf '%s\n' \
  'PURPOSE: test agreement among context fields, assembly offsets, and task startup.' \
  'PREDICT: valid rsp is 8 mod 16; injected +1 is rejected before switching with status 41.' \
  'EVIDENCE: measured layout, normal task markers, context fault profile, and exact fault ELF.'

cat >"$TMP/offsets.c" <<'EOF'
#include <stddef.h>
#include <stdio.h>
#include "toyos.h"
int main(void) {
    printf("r15=%zu rbx=%zu rbp=%zu rsp=%zu rip=%zu size=%zu\n",
        offsetof(struct context, r15), offsetof(struct context, rbx),
        offsetof(struct context, rbp), offsetof(struct context, rsp),
        offsetof(struct context, rip), sizeof(struct context));
    return 0;
}
EOF
cc -std=c11 -Wall -Wextra -Werror -I"$TOYOS/include" \
  -DCONFIG_MILESTONE=7 "$TMP/offsets.c" -o "$TMP/offsets"
offsets=$("$TMP/offsets")
printf '\n[compiler-observed context]\n%s\n' "$offsets"
test "$offsets" = "r15=0 rbx=32 rbp=40 rsp=48 rip=56 size=64"

printf '\n[context validator and task construction]\n'
awk 'NR>=25 && NR<=51 || NR>=98 && NR<=153 {print}' "$TOYOS/src/tasks.c"
printf '\n[switch implementation]\n'
cat "$TOYOS/src/switch.S"

printf '\n[canonical task observations]\n'
make -C "$TOYOS" --no-print-directory week7
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week7/toyos.iso" 33 \
  "KERNEL-TASK 0: preemption probe"
"$TOYOS/scripts/check-qemu.sh" "$TOYOS/build/week7/toyos.iso" 33 \
  "KERNEL-TASK 1: cooperative yield probe"

printf '\n[injected context observation]\n'
make -C "$TOYOS" --no-print-directory expected-fault-week7-context

printf '\n[exact fault artifact]\n'
nm -n "$FAULT_ELF" |
  awk '$3=="schedule" || $3=="tasks_init" || $3=="context_switch" {print}'
objdump -dS --disassemble=schedule "$FAULT_ELF"

printf '%s\n' \
  'PASS: valid task startup and selected corrupted-RSP rejection matched predictions.' \
  'LIMIT: stack overflow, other corruptions, user mode, and extended-register state remain untested.'
