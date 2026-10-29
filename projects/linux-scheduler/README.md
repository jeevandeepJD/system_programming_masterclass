# Week 15 Linux scheduler lab

This bounded userspace project accompanies Week 15. `scheduler_sim` is a
deterministic teaching simulator, not Linux EEVDF. Other programs observe a
pipe wakeup, pipe ping-pong, inherited affinity, a normal-pthread inversion
scenario, and read-only `/proc/self` report fields.

Run `make test-week15`. Core tests never request RT policy, change cgroups,
mount tracefs, offline CPUs, use the network, or write outside `build/`.
Every child is reaped and every blocking path has an alarm. `make tools`
performs optional read-only checks and prints `SKIP` honestly.
