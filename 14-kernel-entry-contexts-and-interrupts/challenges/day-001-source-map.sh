#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PROJECT="$ROOT/projects/linux-kernel-contexts"
export LINUX_SRC="${LINUX_SRC:-/home/jd/Documents/linux-6.12}"
printf '%s\n' \
  'PURPOSE: pin the Linux 6.12 tree and name the file that owns each context.' \
  'PREDICT: VERSION 6, PATCHLEVEL 12, SUBLEVEL 0, and commit adc218676eef.' \
  'EVIDENCE: exact version lines, Kconfig and kbuild text, and source_modified=no.'
uname -srmo
"$PROJECT/scripts/check-source.sh" 1
printf '%s\n' \
  'PASS: the pinned tree identifies v6.12 and the entry files are present.' \
  'LIMIT: a grep hit is not the running kernel config and not a boot.'
