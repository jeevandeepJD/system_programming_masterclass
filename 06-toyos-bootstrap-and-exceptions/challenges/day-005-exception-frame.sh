#!/usr/bin/env bash
# Check C frame offsets and exercise breakpoint and divide exception paths.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

printf '%s\n' \
  'PURPOSE: test agreement between assembly push order and the C frame layout.' \
  'PREDICT: r15=0, rax=112, vector=120, error=128, rip=136 bytes.' \
  'EVIDENCE: measured offsetof values, breakpoint return, and divide fault marker.'

printf '\n[frame declaration]\n'
awk '/struct interrupt_frame/{show=1} show{print} /^};/{if(show){exit}}' \
  "$TOYOS/include/toyos.h"

printf '\n[assembly common entry]\n'
awk '/^isr_common:/{show=1} show{print} /^    iretq/{exit}' "$TOYOS/src/isr.S"

cat >"$TMP/offsets.c" <<'EOF'
#include <stddef.h>
#include <stdio.h>
#include "toyos.h"
int main(void) {
    printf("r15=%zu rax=%zu vector=%zu error=%zu rip=%zu size=%zu\n",
        offsetof(struct interrupt_frame, r15),
        offsetof(struct interrupt_frame, rax),
        offsetof(struct interrupt_frame, vector),
        offsetof(struct interrupt_frame, error),
        offsetof(struct interrupt_frame, rip),
        sizeof(struct interrupt_frame));
    return 0;
}
EOF
cc -std=c11 -Wall -Wextra -Werror -I"$TOYOS/include" \
  -DCONFIG_MILESTONE=6 "$TMP/offsets.c" -o "$TMP/offsets"
offsets=$("$TMP/offsets")
printf '\n[compiler-observed offsets]\n%s\n' "$offsets"
test "$offsets" = "r15=0 rax=112 vector=120 error=128 rip=136 size=160"

printf '\n[breakpoint return path]\n'
make -C "$TOYOS" --no-print-directory week6
"$TOYOS/scripts/check-qemu.sh" \
  "$TOYOS/build/week6/toyos.iso" 33 "SELFTEST: IDT breakpoint frame PASS"

printf '\n[divide-error path]\n'
make -C "$TOYOS" --no-print-directory expected-fault-week6-divide

printf '%s\n' \
  'PASS: selected static layout and dynamic exception paths agree.' \
  'LIMIT: two vectors do not validate every stub or privilege-transition frame.'
