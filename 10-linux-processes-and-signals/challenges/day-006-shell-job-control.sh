#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); PROJECT="$ROOT/projects/linux-system-programming"
printf '%s\n' 'PURPOSE: relate fixed shell-style launch to PID, PGID, SID, and bounded child-group state.' 'PREDICT: fixed command exits 7; helper child has PID equal to PGID; terminal availability may vary.' 'EVIDENCE: self-only IDs, canonical status mode, and one reaped helper process group.'
make -C "$PROJECT" --no-print-directory build
python3 - <<'PY2'
import os
print(f"observer pid={os.getpid()} ppid={os.getppid()} pgid={os.getpgrp()} sid={os.getsid(0)} stdin_tty={os.isatty(0)}")
r,w=os.pipe(); pid=os.fork()
if pid==0:
    os.close(r); os.setpgid(0,0)
    os.write(w, f"child pid={os.getpid()} ppid={os.getppid()} pgid={os.getpgrp()} sid={os.getsid(0)}\n".encode())
    os.close(w); os._exit(0)
os.close(w); data=os.read(r,512); os.close(r); got,status=os.waitpid(pid,0)
if got != pid or not os.WIFEXITED(status) or os.WEXITSTATUS(status)!=0: raise SystemExit('child-group verification failed')
text=data.decode().strip(); print(text)
if f"pid={pid}" not in text or f"pgid={pid}" not in text: raise SystemExit('PGID invariant failed')
print('child_group_reaped=yes')
PY2
"$PROJECT/build/process_lab" status
printf '%s\n' 'PASS: bounded launch/group observations completed with child cleanup.' 'LIMIT: no full shell, pipeline, setsid, tcsetpgrp, stop/continue, terminal mutation, or broad signal target.'
