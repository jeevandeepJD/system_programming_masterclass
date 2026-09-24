#!/usr/bin/env bash
#
# Day 6: read-only CPU, cache, and NUMA topology probe.
# Missing tools and restricted virtual/container environments are expected.

set -u

section()
{
    printf '\n== %s ==\n' "$1"
}

run_if_available()
{
    local command_name=$1
    shift

    if command -v "$command_name" >/dev/null 2>&1; then
        "$@" 2>&1 || printf '[%s failed or is restricted]\n' "$command_name"
    else
        printf '[%s not installed]\n' "$command_name"
    fi
}

section "Environment"
printf 'kernel: '
uname -srmo 2>/dev/null || printf 'unavailable\n'

if command -v systemd-detect-virt >/dev/null 2>&1; then
    printf 'virtualization: '
    systemd-detect-virt 2>/dev/null || printf 'none detected\n'
else
    printf 'virtualization: [systemd-detect-virt not installed]\n'
fi

if [[ -r /proc/self/status ]]; then
    awk '/^(Cpus_allowed_list|Mems_allowed_list):/ { print }' /proc/self/status
else
    printf 'allowed CPU/memory lists: [/proc/self/status unavailable]\n'
fi

section "lscpu summary"
run_if_available lscpu lscpu

section "lscpu CPU-to-node map"
if command -v lscpu >/dev/null 2>&1; then
    lscpu -e=CPU,NODE,SOCKET,CORE,ONLINE 2>&1 ||
        printf '[this lscpu does not support the requested columns]\n'
else
    printf '[lscpu not installed]\n'
fi

section "lscpu cache view"
if command -v lscpu >/dev/null 2>&1; then
    lscpu --caches 2>&1 ||
        printf '[this lscpu does not provide --caches]\n'
else
    printf '[lscpu not installed]\n'
fi

section "numactl hardware"
run_if_available numactl numactl --hardware

section "numactl current policy"
run_if_available numactl numactl --show

section "hwloc topology"
if command -v lstopo-no-graphics >/dev/null 2>&1; then
    lstopo-no-graphics 2>&1 ||
        printf '[lstopo-no-graphics failed or is restricted]\n'
elif command -v lstopo >/dev/null 2>&1; then
    lstopo --of console 2>&1 ||
        printf '[lstopo failed or is restricted]\n'
else
    printf '[hwloc/lstopo not installed]\n'
fi

section "sysfs online topology"
for path in /sys/devices/system/cpu/online \
            /sys/devices/system/node/online \
            /sys/devices/system/node/possible; do
    if [[ -r "$path" ]]; then
        printf '%s: ' "$path"
        cat "$path"
    else
        printf '%s: [unavailable]\n' "$path"
    fi
done

section "sysfs NUMA nodes"
shopt -s nullglob
nodes=(/sys/devices/system/node/node[0-9]*)
if ((${#nodes[@]} == 0)); then
    printf '[no NUMA node directories exposed]\n'
else
    for node in "${nodes[@]}"; do
        printf '%s cpulist=' "${node##*/}"
        if [[ -r "$node/cpulist" ]]; then
            cat "$node/cpulist"
        else
            printf '[unavailable]\n'
        fi

        printf '%s distance=' "${node##*/}"
        if [[ -r "$node/distance" ]]; then
            cat "$node/distance"
        else
            printf '[unavailable]\n'
        fi
    done
fi

section "Interpretation reminders"
cat <<'EOF'
- A logical CPU is not necessarily a core; SMT siblings may share one core.
- A package/socket is not necessarily one NUMA node.
- NUMA distance values are relative costs, not nanoseconds.
- Memory-channel count cannot be inferred from NUMA-node count.
- VM and container views may be synthetic or restricted.
EOF
