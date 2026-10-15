#!/bin/sh
# Read-only checks against the pinned Linux v6.12 tree. This script never
# writes under LINUX_SRC.
set -eu

LINUX_SRC=${LINUX_SRC:-/home/jd/Documents/linux-6.12}
day=${1:-all}

if [ ! -d "$LINUX_SRC" ]; then
    printf 'FAIL LINUX_SRC is not a directory: %s\n' "$LINUX_SRC" >&2
    exit 1
fi

need_file() {
    if [ ! -f "$LINUX_SRC/$1" ]; then
        printf 'FAIL missing %s\n' "$1" >&2
        exit 1
    fi
}

need_exact() {
    need_file "$1"
    if ! grep -F -x -q -- "$2" "$LINUX_SRC/$1"; then
        printf 'FAIL %s lacks exact line: %s\n' "$1" "$2" >&2
        exit 1
    fi
    printf 'exact=%s\n' "$2"
}

need_text() {
    need_file "$1"
    if ! grep -F -q -- "$2" "$LINUX_SRC/$1"; then
        printf 'FAIL %s lacks text: %s\n' "$1" "$2" >&2
        exit 1
    fi
    printf 'text=%s:%s\n' "$1" "$2"
}

version_gate() {
    need_exact Makefile "VERSION = 6"
    need_exact Makefile "PATCHLEVEL = 12"
    need_exact Makefile "SUBLEVEL = 0"
    need_exact Makefile "NAME = Baby Opossum Posse"
    if [ ! -d "$LINUX_SRC/.git" ]; then
        printf 'FAIL pinned tree has no git metadata\n' >&2
        exit 1
    fi
    commit=$(git -C "$LINUX_SRC" rev-parse HEAD)
    expect=adc218676eef25575469234709c2d87185ca223a
    if [ "$commit" != "$expect" ]; then
        printf 'FAIL commit %s\n' "$commit" >&2
        exit 1
    fi
    printf 'commit=%s\n' "$commit"
    printf 'tree=v6.12\n'
}

day1() {
    version_gate
    need_text Kconfig 'mainmenu "Linux/$(ARCH) $(KERNELVERSION) Kernel Configuration"'
    need_text scripts/Makefile.build "Init all relevant variables used in kbuild files so"
    need_text Makefile "KBUILD_CFLAGS += -std=gnu11"
    need_text kernel/Kconfig.preempt "config PREEMPT_NONE"
    need_text kernel/Kconfig.preempt "config PREEMPT_VOLUNTARY"
    need_text kernel/Kconfig.preempt 'prompt "Preemption Model"'
    need_text include/linux/preempt.h "preempt_count (used for kernel preemption, interrupt count, etc.)"
    need_file arch/x86/entry/entry_64.S
    need_file kernel/entry/common.c
    need_file arch/x86/mm/fault.c
    need_file kernel/irq/irqdesc.c
    need_file kernel/softirq.c
    need_file kernel/workqueue.c
    need_text kernel/entry/Makefile 'obj-$(CONFIG_GENERIC_ENTRY)'
    need_text include/linux/preempt.h "# define in_task()"
    need_text arch/x86/kernel/idt.c "static __init void set_intr_gate(unsigned int n, const void *addr)"
    printf 'source_modified=no\n'
    printf 'day=1\n'
}

day2() {
    version_gate
    need_text arch/x86/entry/entry_64.S "SYM_CODE_START(entry_SYSCALL_64)"
    need_text arch/x86/entry/entry_64.S "syscall_return_via_sysret:"
    need_text arch/x86/entry/entry_64.S "sysretq"
    need_text arch/x86/entry/common.c "__visible noinstr bool do_syscall_64(struct pt_regs *regs, int nr)"
    need_text arch/x86/entry/common.c "regs->ax = x64_sys_call(regs, unr);"
    need_text arch/x86/entry/syscall_64.c "The sys_call_table[] is no longer used for system calls, but"
    need_text arch/x86/entry/syscall_64.c "long x64_sys_call(const struct pt_regs *regs, unsigned int nr)"
    need_text include/linux/entry-common.h "syscall_enter_from_user_mode - Establish state and check and handle work"
    need_text kernel/entry/common.c "__visible noinstr void syscall_exit_to_user_mode(struct pt_regs *regs)"
    printf 'source_modified=no\n'
    printf 'day=2\n'
}

day3() {
    version_gate
    need_text arch/x86/mm/fault.c "DEFINE_IDTENTRY_RAW_ERRORCODE(exc_page_fault)"
    need_text arch/x86/mm/fault.c "void do_user_addr_fault(struct pt_regs *regs,"
    need_text arch/x86/mm/fault.c "address = cpu_feature_enabled(X86_FEATURE_FRED) ? fred_event_data(regs) : read_cr2();"
    need_text kernel/entry/common.c "noinstr irqentry_state_t irqentry_enter(struct pt_regs *regs)"
    need_text kernel/entry/common.c "noinstr void irqentry_enter_from_user_mode(struct pt_regs *regs)"
    need_text kernel/entry/common.c "noinstr void irqentry_exit_to_user_mode(struct pt_regs *regs)"
    printf 'source_modified=no\n'
    printf 'day=3\n'
}

day4() {
    version_gate
    need_text include/linux/irqdesc.h "struct irq_desc {"
    need_text include/linux/irqdesc.h "irq_flow_handler_t	handle_irq;"
    need_text include/linux/interrupt.h "typedef irqreturn_t (*irq_handler_t)(int, void *);"
    need_text include/linux/interrupt.h "struct irqaction - per interrupt action descriptor"
    need_text kernel/irq/handle.c "irqreturn_t __handle_irq_event_percpu(struct irq_desc *desc)"
    need_text kernel/irq/handle.c "trace_irq_handler_entry(irq, action);"
    need_text include/linux/interrupt.h "#define IRQF_SHARED"
    need_text kernel/irq/chip.c "void handle_level_irq(struct irq_desc *desc)"
    need_text drivers/irqchip/irq-gic-v3.c "static void gic_eoi_irq(struct irq_data *d)"
    need_text drivers/irqchip/irq-sifive-plic.c "Handling an interrupt is a two-step process: first you claim the interrupt"
    need_text kernel/irq/handle.c "void __irq_wake_thread(struct irq_desc *desc, struct irqaction *action)"
    printf 'source_modified=no\n'
    printf 'day=4\n'
}

day5() {
    version_gate
    need_text include/linux/interrupt.h "HI_SOFTIRQ=0,"
    need_text include/linux/interrupt.h "RCU_SOFTIRQ,"
    need_text include/linux/interrupt.h "NR_SOFTIRQS"
    need_text include/linux/interrupt.h "request_threaded_irq(unsigned int irq, irq_handler_t handler,"
    need_text kernel/softirq.c "asmlinkage __visible void __softirq_entry __do_softirq(void)"
    need_text kernel/softirq.c "void raise_softirq(unsigned int nr)"
    need_text kernel/softirq.c "void open_softirq(int nr, void (*action)(void))"
    need_text kernel/workqueue.c "bool queue_work_on(int cpu, struct workqueue_struct *wq,"
    need_text include/linux/workqueue.h "WQ_BH"
    need_text kernel/workqueue.c "static int worker_thread(void *__worker)"
    printf 'source_modified=no\n'
    printf 'day=5\n'
}

day6() {
    version_gate
    need_text include/linux/preempt.h "#define PREEMPT_BITS	8"
    need_text include/linux/preempt.h "#define HARDIRQ_BITS	4"
    need_text include/linux/preempt.h "#define in_hardirq()		(hardirq_count())"
    need_text include/linux/preempt.h "Do not use in_atomic() in driver code."
    need_text include/linux/preempt.h "#define in_atomic()	(preempt_count() != 0)"
    need_text include/linux/hardirq.h "preempt_count_add(HARDIRQ_OFFSET);	\\"
    need_text include/linux/kernel.h "# define might_sleep() \\"
    need_text kernel/Kconfig.preempt "config PREEMPT_NONE"
    printf 'source_modified=no\n'
    printf 'day=6\n'
}

day7() {
    version_gate
    need_text kernel/irq/handle.c "trace_irq_handler_entry(irq, action);"
    need_text kernel/irq/handle.c "trace_irq_handler_exit(irq, action, res);"
    need_text kernel/trace/trace_irqsoff.c "static struct trace_array		*irqsoff_trace __read_mostly;"
    need_text Documentation/trace/ftrace.rst "ftrace - Function Tracer"
    need_text arch/x86/entry/entry_64.S "SYM_CODE_START(entry_SYSCALL_64)"
    need_text arch/x86/mm/fault.c "DEFINE_IDTENTRY_RAW_ERRORCODE(exc_page_fault)"
    printf 'source_modified=no\n'
    printf 'day=7\n'
}

run_day() {
    case "$1" in
        1) day1 ;;
        2) day2 ;;
        3) day3 ;;
        4) day4 ;;
        5) day5 ;;
        6) day6 ;;
        7) day7 ;;
        *)
            printf 'FAIL unknown day %s\n' "$1" >&2
            exit 1
            ;;
    esac
}

case "$day" in
    all)
        run_day 1
        run_day 2
        run_day 3
        run_day 4
        run_day 5
        run_day 6
        run_day 7
        ;;
    1|2|3|4|5|6|7)
        run_day "$day"
        ;;
    *)
        printf 'FAIL usage: check-source.sh [1-7|all]\n' >&2
        exit 1
        ;;
esac
