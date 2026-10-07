#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-system-programming"
printf '%s\n' 'PURPOSE: inventory process identity, executable, ELF, kernel, and checked /proc observations.' 'PREDICT: proc_lab reports PID/PPID, stat, exe, and proc_inspection=ok; numeric identities vary.' 'EVIDENCE: canonical source/binary, self-only procfs output, ELF metadata, hash, and kernel release.'
make -C "$PROJECT" --no-print-directory build
uname -srmo
sha256sum "$PROJECT/build/proc_lab"
readelf -h "$PROJECT/build/proc_lab" | awk '/Class:|Type:|Machine:|Entry point address:/'
printf '\n[checked self observation]\n'
"$PROJECT/build/proc_lab"
printf '\n[source error/lifetime boundaries]\n'
awk 'NR<=55 {print}' "$PROJECT/src/proc_lab.c"
printf '%s\n' 'PASS: canonical self-inspection completed.' 'LIMIT: no arbitrary PID, namespace traversal, maps, credentials transition, or syscall-entry capture.'
