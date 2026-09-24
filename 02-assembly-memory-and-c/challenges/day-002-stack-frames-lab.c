/*
 * Day 2 challenge B — watch frames appear, watch the ABI hold, watch it
 * break, and watch the stack run out.
 *
 * Build with frame pointers so the walk in part 4 has a chain to follow:
 *
 *   gcc -std=c17 -Wall -Wextra -O0 -g -fno-omit-frame-pointer \
 *     day-002-abi-probe.s day-002-stack-frames-lab.c \
 *     -o /tmp/day-002-stack-frames-lab
 *
 *   /tmp/day-002-stack-frames-lab
 *   /tmp/day-002-stack-frames-lab --overflow
 *
 * The --overflow run drives a *child process* into stack exhaustion on
 * purpose. The parent survives and reports what killed the child.
 *
 * Useful GDB session:
 *
 *   gdb -q /tmp/day-002-stack-frames-lab
 *   (gdb) break descend
 *   (gdb) run
 *   (gdb) continue 3
 *   (gdb) backtrace
 *   (gdb) info frame
 *   (gdb) x/6gx $rbp
 *   (gdb) frame 2
 *   (gdb) info args
 */

#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

/* Implemented in day-002-abi-probe.s. */
unsigned long asm_read_rsp(void);
long asm_seven(long a1, long a2, long a3, long a4, long a5, long a6, long a7);
long asm_sum_squares(long n);
long asm_bad_sum_squares(long n);
long asm_probe_rbx(long (*fn)(long), long arg);

#define RBX_MARKER UINT64_C(0x5a5a5a5a5a5a5a5a)

/* ---------------------------------------------------------------- */
/* Part 1 — one frame per recursive activation                       */
/* ---------------------------------------------------------------- */

static unsigned long previous_frame;

__attribute__((noinline)) static long descend(long depth)
{
    long locals[4];
    unsigned long frame = (unsigned long)__builtin_frame_address(0);
    unsigned long ret = (unsigned long)__builtin_return_address(0);

    locals[0] = depth;
    locals[1] = depth * depth;
    locals[2] = locals[0] + locals[1];
    locals[3] = (long)frame;

    if (previous_frame == 0) {
        printf("  depth %2ld  frame 0x%012lx  return 0x%012lx  delta    -\n",
               depth, frame, ret);
    } else {
        printf("  depth %2ld  frame 0x%012lx  return 0x%012lx  delta %4ld\n",
               depth, frame, ret, (long)previous_frame - (long)frame);
    }
    previous_frame = frame;

    if (depth <= 1) {
        return locals[2];
    }
    return locals[2] + descend(depth - 1);
}

/* ---------------------------------------------------------------- */
/* Part 4 — follow the saved-rbp chain by hand                       */
/* ---------------------------------------------------------------- */

struct frame_link {
    struct frame_link *saved_rbp;
    void *return_address;
};

__attribute__((noinline)) static void walk_frames(const char *who, int limit)
{
    struct frame_link *link = __builtin_frame_address(0);

    printf("  manual walk from %s:\n", who);
    for (int level = 0; level < limit && link != NULL; ++level) {
        printf("    level %d  rbp 0x%012lx  return 0x%012lx\n",
               level,
               (unsigned long)link,
               (unsigned long)link->return_address);

        struct frame_link *next = link->saved_rbp;

        /*
         * Stop before reading nonsense. A valid chain climbs toward higher
         * addresses in modest steps; anything else means we have left the
         * frame-pointer world (a leaf compiled with -fomit-frame-pointer,
         * a signal frame, or the bottom of the stack).
         */
        if (next <= link || (unsigned long)next - (unsigned long)link > (1UL << 20)) {
            puts("    chain ends here");
            break;
        }
        link = next;
    }
}

__attribute__((noinline)) static void walk_level_two(void)
{
    walk_frames("walk_level_two", 6);
}

__attribute__((noinline)) static void walk_level_one(void)
{
    walk_level_two();
}

/* ---------------------------------------------------------------- */
/* Part 5 — exhaust a child's stack on purpose                       */
/* ---------------------------------------------------------------- */

struct shared_counter {
    volatile unsigned long depth;
    volatile unsigned long lowest_frame;
    volatile unsigned long highest_frame;
};

__attribute__((noinline)) static void run_away(struct shared_counter *counter)
{
    volatile char padding[256];
    unsigned long frame = (unsigned long)__builtin_frame_address(0);

    padding[0] = (char)counter->depth;
    padding[255] = padding[0];

    if (counter->depth == 0) {
        counter->highest_frame = frame;
    }
    counter->lowest_frame = frame;
    counter->depth += 1;

    /*
     * The guard is never false in practice. It exists so the compiler can
     * see a path that returns, which keeps -Winfinite-recursion quiet and
     * stops it from rewriting this into a loop.
     */
    if (counter->depth < ULONG_MAX) {
        run_away(counter);
    }

    counter->depth += (unsigned long)padding[0];
}

static int overflow_demo(void)
{
    struct rlimit limit;
    struct shared_counter *counter;
    pid_t child;
    int status;

    if (getrlimit(RLIMIT_STACK, &limit) == 0) {
        if (limit.rlim_cur == RLIM_INFINITY) {
            puts("  RLIMIT_STACK soft limit: unlimited");
        } else {
            printf("  RLIMIT_STACK soft limit: %llu bytes (%llu KiB)\n",
                   (unsigned long long)limit.rlim_cur,
                   (unsigned long long)limit.rlim_cur / 1024ULL);
        }
    }

    counter = mmap(NULL, sizeof(*counter), PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (counter == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    counter->depth = 0;
    counter->lowest_frame = 0;
    counter->highest_frame = 0;

    if (fflush(NULL) == EOF) {
        perror("fflush");
        return 1;
    }

    child = fork();
    if (child == -1) {
        perror("fork");
        return 1;
    }

    if (child == 0) {
        run_away(counter);
        _exit(EXIT_FAILURE);
    }

    if (waitpid(child, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }

    printf("  child reached depth %lu\n", counter->depth);
    printf("  first frame 0x%012lx, last frame 0x%012lx\n",
           counter->highest_frame, counter->lowest_frame);
    if (counter->depth > 0) {
        printf("  stack consumed %lu bytes, about %lu bytes per frame\n",
               counter->highest_frame - counter->lowest_frame,
               (counter->highest_frame - counter->lowest_frame) / counter->depth);
    }

    if (WIFSIGNALED(status)) {
        printf("  child terminated by signal %d (%s)\n",
               WTERMSIG(status), strsignal(WTERMSIG(status)));
    } else if (WIFEXITED(status)) {
        printf("  child exited normally with status %d\n", WEXITSTATUS(status));
    }

    puts("  Check the kernel's view of the same event:  dmesg | tail");
    (void)munmap(counter, sizeof(*counter));
    return 0;
}

/* ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--overflow") == 0) {
        puts("Day 2 stack-frame lab — stack exhaustion");
        puts("=========================================");
        return overflow_demo();
    }

    puts("Day 2 stack-frame lab");
    puts("======================");

    puts("\n1. one frame per recursive activation");
    printf("  descend(5) returned %ld\n", descend(5));

    puts("\n2. rsp on entry to a callee");
    {
        unsigned long rsp = asm_read_rsp();

        printf("  rsp inside asm_read_rsp: 0x%012lx\n", rsp);
        printf("  rsp %% 16 = %lu\n", rsp % 16UL);
        puts("  The ABI promises rsp % 16 == 0 *before* the call instruction.");
        puts("  The pushed return address is what makes the callee see 8.");
    }

    puts("\n3. the seventh integer argument");
    {
        long sum = asm_seven(1, 2, 3, 4, 5, 6, 7);

        printf("  asm_seven(1..7) = %ld %s\n", sum, sum == 28 ? "(PASS)" : "(FAIL)");
        puts("  Six values came from rdi, rsi, rdx, rcx, r8, r9.");
        puts("  The seventh was read from 8(%rsp): the caller's stack.");
    }

    puts("\n4. following the saved-rbp chain by hand");
    walk_level_one();
    puts("  Compare the return addresses above with GDB's backtrace.");

    puts("\n5. callee-saved registers are a promise, not a suggestion");
    {
        long good = asm_probe_rbx(asm_sum_squares, 5);
        long bad = asm_probe_rbx(asm_bad_sum_squares, 5);

        printf("  asm_sum_squares(5)     = %ld\n", asm_sum_squares(5));
        printf("  asm_bad_sum_squares(5) = %ld\n", asm_bad_sum_squares(5));
        printf("  rbx after the well-behaved callee: 0x%016lx %s\n",
               (unsigned long)good,
               (unsigned long)good == RBX_MARKER ? "(marker intact)" : "(CLOBBERED)");
        printf("  rbx after the careless callee:     0x%016lx %s\n",
               (unsigned long)bad,
               (unsigned long)bad == RBX_MARKER ? "(marker intact)" : "(CLOBBERED)");
        puts("  Both routines returned the correct sum. Only one of them is");
        puts("  safe to call from code that keeps a value in rbx.");
    }

    puts("\n6. now run the stack-exhaustion demonstration");
    puts("  /tmp/day-002-stack-frames-lab --overflow");

    return 0;
}
