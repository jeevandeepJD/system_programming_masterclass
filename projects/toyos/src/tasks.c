#include "toyos.h"

#if CONFIG_MILESTONE >= 7

enum task_state {
    TASK_RUNNABLE,
    TASK_RUNNING,
    TASK_BLOCKED
};

struct kernel_task {
    struct context context;
    enum task_state state;
    unsigned id;
    void (*entry)(void);
    uint8_t *stack;
};

static struct context boot_context;
static struct kernel_task *tasks[2];
static struct kernel_task *current;
static volatile bool task_done[2];

static void schedule(void);

static bool context_is_valid(const struct kernel_task *task)
{
    if (task == NULL || task->stack == NULL || task->state != TASK_RUNNABLE) {
        return false;
    }
    const uint64_t stack_start = (uint64_t)(uintptr_t)task->stack;
    const uint64_t stack_end = stack_start + 8192U;
    const uint64_t kernel_start = (uint64_t)(uintptr_t)__kernel_start;
    const uint64_t kernel_end = (uint64_t)(uintptr_t)__kernel_end;
    return task->context.rsp >= stack_start
        && task->context.rsp < stack_end
        && (task->context.rsp & 0xfU) == 8U
        && task->context.rip >= kernel_start
        && task->context.rip < kernel_end;
}

static void validate_context(const struct kernel_task *task)
{
    if (!context_is_valid(task)) {
#if EXPECT_FAULT == 5
        serial_write("EXPECTED-FAULT: context PASS\n");
        qemu_exit(0x14);
#else
        PANIC("corrupted kernel-task context");
#endif
    }
}

static void task_finish(void)
{
    ASSERT(current != NULL);
    task_done[current->id] = true;
    current->state = TASK_BLOCKED;
    serial_write("KERNEL-TASK ");
    serial_dec(current->id);
    serial_write(": done\n");
    if (task_done[0] && task_done[1]) {
        ASSERT(timer_ticks() >= 3U);
        serial_write("SELFTEST: cooperative switch PASS\n");
        serial_write("SELFTEST: PIT/PIC preemption PASS\n");
        serial_write("WEEK7: PASS\n");
        qemu_exit(0x10);
    }
    schedule();
    PANIC("blocked kernel task resumed");
}

static void task_trampoline(void)
{
    interrupts_enable();
    ASSERT(current != NULL);
    current->entry();
    task_finish();
}

static void preempted_worker(void)
{
    serial_write("KERNEL-TASK 0: preemption probe\n");
    const uint64_t start = timer_ticks();
    while (timer_ticks() - start < 3U) {
        __asm__ volatile ("pause");
    }
}

static void yielding_worker(void)
{
    serial_write("KERNEL-TASK 1: cooperative yield probe\n");
    for (unsigned i = 0; i < 4U; ++i) {
        schedule();
    }
}

static struct kernel_task *create_task(unsigned id, void (*entry)(void))
{
    struct kernel_task *const task = kalloc(sizeof(*task));
    uint8_t *const stack = kalloc(8192U);
    ASSERT(task != NULL);
    ASSERT(stack != NULL);
    task->state = TASK_RUNNABLE;
    task->id = id;
    task->entry = entry;
    task->stack = stack;
    uint64_t stack_top = ((uint64_t)(uintptr_t)(stack + 8192U)) & ~0xfULL;
    stack_top -= 8U;
    *(uint64_t *)(uintptr_t)stack_top = 0;
    task->context = (struct context){
        .rsp = stack_top,
        .rip = (uint64_t)(uintptr_t)task_trampoline
    };
    return task;
}

void tasks_init(void)
{
    tasks[0] = create_task(0, preempted_worker);
    tasks[1] = create_task(1, yielding_worker);
    serial_write("TASKS: two ring-0 kernel tasks with dedicated stacks\n");
    serial_write("TASKS: no user processes, isolation, or syscalls yet\n");
#if EXPECT_FAULT == 5
    serial_write("INJECT: corrupt kernel-task stack alignment\n");
    tasks[0]->context.rsp += 1U;
#endif
}

static void schedule(void)
{
    struct kernel_task *next;
    if (current == NULL) {
        next = tasks[0];
    } else {
        next = tasks[(current->id + 1U) % ARRAY_LEN(tasks)];
        if (next->state == TASK_BLOCKED) {
            next = tasks[current->id];
        }
        if (next == current || next->state == TASK_BLOCKED) {
            return;
        }
    }

    validate_context(next);
    struct context *old_context = &boot_context;
    if (current != NULL) {
        current->state = TASK_RUNNABLE;
        old_context = &current->context;
    }
    next->state = TASK_RUNNING;
    current = next;
    context_switch(old_context, &next->context);
}

void scheduler_tick(void)
{
    if (current != NULL) {
        schedule();
    }
}

void tasks_start(void)
{
    schedule();
    PANIC("scheduler returned to bootstrap context");
}

#endif
