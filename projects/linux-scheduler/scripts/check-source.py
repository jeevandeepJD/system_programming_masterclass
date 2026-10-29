#!/usr/bin/env python3
from pathlib import Path
import hashlib,os,subprocess,re,sys
root=Path(os.environ.get("LINUX_SRC","/home/jd/Documents/linux-6.12"))
expect="adc218676eef25575469234709c2d87185ca223a"
mf=root/"Makefile"
before=hashlib.sha256(mf.read_bytes()).hexdigest()
got=subprocess.check_output(["git","-C",str(root),"rev-parse","HEAD"],text=True).strip()
if got!=expect: raise SystemExit(f"FAIL commit={got}")
checks={
"kernel/sched/core.c":[
"void enqueue_task(struct rq *rq, struct task_struct *p, int flags)",
"int try_to_wake_up(struct task_struct *p, unsigned int state, int wake_flags)",
"cpu = select_task_rq(p, p->wake_cpu, &wake_flags);",
"ttwu_queue(p, cpu, wake_flags);",
"context_switch(struct rq *rq, struct task_struct *prev,",
"switch_to(prev, next, prev);",
"static void __sched notrace __schedule(int sched_mode)",
"next = pick_next_task(rq, prev, &rf);",
"trace_sched_switch(preempt, prev, next, prev_state);",
"void __sched schedule(void)",
"BUG_ON(!sched_class_above(&stop_sched_class, &dl_sched_class));",
"BUG_ON(!sched_class_above(&dl_sched_class, &rt_sched_class));",
"BUG_ON(!sched_class_above(&rt_sched_class, &fair_sched_class));"],
"kernel/sched/fair.c":[
"static int vruntime_eligible(struct cfs_rq *cfs_rq, u64 vruntime)",
"int entity_eligible(struct cfs_rq *cfs_rq, struct sched_entity *se)",
"static struct sched_entity *pick_eevdf(struct cfs_rq *cfs_rq)",
"select_task_rq_fair(struct task_struct *p, int prev_cpu, int wake_flags)",
"cpu = find_energy_efficient_cpu(p, prev_cpu);",
"new_cpu = select_idle_sibling(p, prev_cpu, new_cpu);"]}
norm=lambda s:re.sub(r"\s+"," ",s.strip())
for rel,need in checks.items():
 lines=[norm(x) for x in (root/rel).read_text(errors="replace").splitlines()]
 pos=-1
 for fragment in need:
  target=norm(fragment)
  pos=next((i for i in range(pos+1,len(lines)) if target in lines[i]),-1)
  if pos<0: raise SystemExit(f"FAIL ordered excerpt {rel}: {fragment}")
  print(f"excerpt={rel}:{fragment}")
after=hashlib.sha256(mf.read_bytes()).hexdigest()
if before!=after: raise SystemExit("FAIL source Makefile changed")
print(f"commit={got}\nsource_makefile_unchanged=yes\nPASS source-check")
