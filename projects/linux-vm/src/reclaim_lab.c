#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAMES 4
#define SWAP_SLOTS 2

struct frame {
    int occupied;
    int page_id;
    int is_file;
    int dirty;
};

static struct frame memory[FRAMES];
static int lru[FRAMES];
static int resident;
static int swap_pages[SWAP_SLOTS];
static int swap_count;
static int writebacks;
static int clean_drops;
static int swapouts;
static int oom_choices;

static void die(const char *what) {
    fprintf(stderr, "reclaim_lab: %s\n", what);
    exit(EXIT_FAILURE);
}

static int find_lru(int page_id) {
    for (int index = 0; index < resident; index++) {
        if (memory[lru[index]].page_id == page_id) {
            return index;
        }
    }
    return -1;
}

static void move_mru(int lru_index) {
    int slot = lru[lru_index];
    for (int index = lru_index; index > 0; index--) {
        lru[index] = lru[index - 1];
    }
    lru[0] = slot;
}

static int free_slot(void) {
    for (int slot = 0; slot < FRAMES; slot++) {
        if (!memory[slot].occupied) {
            return slot;
        }
    }
    return -1;
}

static void install(int slot, int page_id, int is_file, int dirty) {
    memory[slot].occupied = 1;
    memory[slot].page_id = page_id;
    memory[slot].is_file = is_file;
    memory[slot].dirty = dirty;
    for (int index = resident; index > 0; index--) {
        lru[index] = lru[index - 1];
    }
    lru[0] = slot;
    resident++;
}

static int evict_lru(void) {
    if (resident < 1) {
        die("evict empty");
    }
    int slot = lru[resident - 1];
    struct frame victim = memory[slot];
    if (victim.is_file && victim.dirty) {
        writebacks++;
        puts("evict_dirty_file=writeback_then_drop");
    } else if (victim.is_file) {
        clean_drops++;
        puts("evict_clean_file=drop");
    } else if (swap_count >= SWAP_SLOTS) {
        oom_choices++;
        puts("swap_full_anon=simulated_oom_choice");
        puts("simulated_oom_choice=task_b");
        puts("oom_kills_host_process=no");
        return 0;
    } else {
        swap_pages[swap_count] = victim.page_id;
        swap_count++;
        swapouts++;
        puts("evict_anon=swap");
    }
    memory[slot].occupied = 0;
    resident--;
    return 1;
}

static void access_page(int page_id, int is_file, int write) {
    int lru_index = find_lru(page_id);
    if (lru_index >= 0) {
        if (write) {
            memory[lru[lru_index]].dirty = 1;
        }
        move_mru(lru_index);
        return;
    }
    int slot = free_slot();
    if (slot < 0) {
        int oom_before = oom_choices;
        if (!evict_lru()) {
            if (oom_choices == oom_before) {
                die("evict");
            }
            return;
        }
        slot = free_slot();
        if (slot < 0) {
            die("no frame after evict");
        }
    }
    install(slot, page_id, is_file, write);
}

static void mode_demo(void) {
    puts("frames=4");
    puts("swap_slots=2");
    puts("policy=teaching_lru_not_kernel_scan");
    puts("simulator_only=yes");
    puts("reclaim_host=no");
    puts("swap_host=no");
    puts("pressure_induced=no");

    access_page(1, 1, 1);
    access_page(2, 1, 0);
    access_page(3, 0, 0);
    access_page(4, 0, 0);
    access_page(5, 1, 0);
    access_page(6, 1, 0);
    access_page(7, 0, 0);
    access_page(8, 0, 0);
    access_page(5, 1, 0);
    access_page(6, 1, 0);
    access_page(8, 0, 0);
    access_page(10, 1, 0);

    if (writebacks != 1 || clean_drops != 1 || swapouts != 2 || oom_choices != 1) {
        fprintf(stderr,
                "counts writeback=%d clean=%d swap=%d oom=%d resident=%d\n",
                writebacks, clean_drops, swapouts, oom_choices, resident);
        die("scenario");
    }
    if (find_lru(10) >= 0 || find_lru(7) < 0) {
        die("oom left the new page installed or dropped the anon victim");
    }
    printf("writeback_count=%d\n", writebacks);
    printf("clean_drop_count=%d\n", clean_drops);
    printf("swapout_count=%d\n", swapouts);
    printf("simulated_oom_choices=%d\n", oom_choices);
}

int main(int argc, char **argv) {
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: reclaim_lab demo\n");
        return EXIT_FAILURE;
    }
    mode_demo();
    return EXIT_SUCCESS;
}
