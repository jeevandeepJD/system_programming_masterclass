#!/usr/bin/env bash
# Generate a temporary integrated Week 9 filesystem/device evidence report.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TOYOS="$ROOT/projects/toyos"
REPORT=$(mktemp /tmp/toyos-week9-report.XXXXXX.txt)
NORMAL="$TOYOS/build/week9/toyos.elf"
USER="$TOYOS/build/week9/user.elf"
FS_FAULT="$TOYOS/build/week9-filesystem/toyos.elf"
DEV_FAULT="$TOYOS/build/week9-device/toyos.elf"

printf '%s\n' \
  'PURPOSE: collect reproducible VFS/ramfs, syscall, PCI/MMIO, fault, and comparison evidence.' \
  'PREDICT: normal/filesystem/device host statuses are 33/47/49 with exact checked markers.' \
  "EVIDENCE: temporary report at $REPORT; causal interpretation remains the learner's work."

{
  printf 'ToyOS Week 9 filesystem/device evidence report\n'
  printf 'Generated (UTC): '; date -u '+%Y-%m-%dT%H:%M:%SZ'
  printf 'Canonical project: %s\n\n' "$TOYOS"

  printf '== Tool identity ==\n'
  gcc --version | awk 'NR==1'
  ld --version | awk 'NR==1'
  qemu-system-x86_64 --version | awk 'NR==1'

  printf '\n== Complete canonical Week 9 regression ==\n'
  make -C "$TOYOS" --no-print-directory test-week9

  printf '\n== Exact artifact identities ==\n'
  sha256sum "$NORMAL" "$USER" "$FS_FAULT" "$DEV_FAULT"
  for artifact in "$NORMAL" "$USER" "$FS_FAULT" "$DEV_FAULT"; do
    printf '%s\n' "$artifact"
    readelf -h "$artifact" | awk '/Class:|Machine:|Entry point address:/'
  done

  printf '\n== Ordered boundary symbols ==\n'
  nm -n "$NORMAL" | awk '$3=="copy_from_user" || $3=="syscall_dispatch" ||
    $3=="vfs_open" || $3=="vfs_read" || $3=="vfs_write" ||
    $3=="pci_read" || $3=="edu_checked_write" || $3=="edu_init" {print}'
  nm -n "$USER" | awk '$3=="_user_start" || $3=="ramfs_path" ||
    $3=="ramfs_note_path" || $3=="io_buffer" {print}'

  printf '\n== Filesystem contracts ==\n'
  cat "$TOYOS/src/vfs.c"
  awk 'NR>=169 && NR<=263 {print}' "$TOYOS/src/process.c"

  printf '\n== Device contracts ==\n'
  cat "$TOYOS/src/edu.c"

  printf '\n== Integrated trace ==\n'
  printf '%s\n' \
    'user open: RAX=7 and user path → int80 → bounded kernel string → exact inode lookup → file slot/fd.' \
    'user read/write: fd → open file/offset → staged kernel buffer → checked user copy; inode owns bytes.' \
    'device: x86 config ports → 1234:11e8 → BAR0 physical base → page mapping → ID/liveness MMIO.' \
    'the ramfs data path and EDU operation remain independent.'

  printf '\n== Fault detection boundaries ==\n'
  printf '%s\n' \
    'filesystem: direct kernel /missing lookup returns -2 during vfs_init; no ring-3 path copy.' \
    'device: offset 3 is rejected by edu_checked_write after normal liveness; no invalid MMIO occurs.'

  printf '\n== Comparison landmarks ==\n'
  printf '%s\n' \
    'xv6-riscv: kernel/file.c fs.c bio.c log.c sysfile.c and virtio disk driver; verify revision.' \
    'Linux: fs/ mm/filemap.c block/ drivers/pci/ include/linux/io.h and concrete drivers; inspect revision.'

  printf '\n== Evidence limits ==\n'
  printf '%s\n' \
    'No close/reclaim, directories, permission model, page cache, block I/O, persistence, or journaling.' \
    'No portable MMIO accessor/memory-type API, physical-hardware result, EDU IRQ, or DMA.' \
    'No file-data path connects ramfs to EDU.'
} >"$REPORT" 2>&1

printf '\n[key report evidence]\n'
awk '/^ToyOS Week|^Generated|^Canonical project|^== |WEEK9: PASS|EXPECTED-FAULT|Evidence limits/{print}' \
  "$REPORT"

printf '\nPASS: Week 9 report generated at %s\n' "$REPORT"
printf '%s\n' \
  'LIMIT: generated evidence does not substitute for a captured GDB session or causal explanation.'
