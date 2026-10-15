#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-vm"
printf '%s\n' \
  'PURPOSE: separate a virtual address, a VMA, and a page-table entry.' \
  'PREDICT: one anonymous mapping is in maps, its smaps Size matches the page, and Rss is 0 before a touch.' \
  'EVIDENCE: checked markers, compiler identity, and the binary hash.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
cc --version | awk 'NR==1'
sha256sum "$PROJECT/build/address_lab"
printf '\n[address space evidence]\n'
"$PROJECT/build/address_lab" demo
printf '\n[source landmark]\n'
grep -n 'maps_contains_anon\|smaps_size_matches_page\|heap_presence_not_a_glibc_contract' \
  "$PROJECT/src/address_lab.c"
printf '%s\n' \
  'PASS: maps shows the anonymous VMA; untouched Rss is not a resident frame.' \
  'LIMIT: maps is this process export. It is not a page-table dump and not a physical address.'
