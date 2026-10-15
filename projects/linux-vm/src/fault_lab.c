#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int smaps_kb(unsigned long start, const char *key, unsigned long *kb_out) {
    FILE *smaps = fopen("/proc/self/smaps", "r");
    if (smaps == NULL) {
        return 0;
    }
    char line[512];
    int in_region = 0;
    int found = 0;
    size_t key_len = strlen(key);
    while (fgets(line, (int)sizeof(line), smaps) != NULL) {
        unsigned long region_start = 0;
        unsigned long region_end = 0;
        if (sscanf(line, "%lx-%lx", &region_start, &region_end) == 2) {
            in_region = region_start == start;
            continue;
        }
        if (!in_region || strncmp(line, key, key_len) != 0 || line[key_len] != ':') {
            continue;
        }
        unsigned long kb = 0;
        if (sscanf(line + key_len + 1, "%lu", &kb) == 1) {
            *kb_out = kb;
            found = 1;
            break;
        }
    }
    fclose(smaps);
    return found;
}

static void mode_demo(void) {
    long page_long = sysconf(_SC_PAGESIZE);
    if (page_long < 1024 || (page_long % 1024L) != 0) {
        fail("pagesize");
    }
    size_t page = (size_t)page_long;
    printf("page_size=%zu\n", page);

    void *mapping = mmap(NULL, page, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) {
        fail("mmap");
    }
    unsigned char resident = 1;
    if (mincore(mapping, page, &resident) != 0) {
        fail("mincore");
    }
    if ((resident & 1U) != 0) {
        fail("page resident before touch");
    }
    puts("anon_untouched_resident=no");

    unsigned long rss_before = 1;
    if (!smaps_kb((unsigned long)(uintptr_t)mapping, "Rss", &rss_before)) {
        fail("smaps before");
    }
    if (rss_before != 0) {
        fail("rss before touch");
    }
    puts("anon_untouched_rss_kb=0");

    struct rusage before;
    struct rusage after;
    if (getrusage(RUSAGE_SELF, &before) != 0) {
        fail("getrusage");
    }
    unsigned char value = ((volatile unsigned char *)mapping)[0];
    if (getrusage(RUSAGE_SELF, &after) != 0) {
        fail("getrusage");
    }
    if (value != 0) {
        fail("zero fill");
    }
    puts("zero_fill=yes");

    long minor_delta = after.ru_minflt - before.ru_minflt;
    long major_delta = after.ru_majflt - before.ru_majflt;
    printf("minor_fault_delta=%ld\n", minor_delta);
    printf("major_fault_delta=%ld\n", major_delta);
    if (minor_delta < 1) {
        fail("minor fault delta");
    }
    puts("minor_fault_delta_ge_1=yes");
    puts("fault_count_caveat=other_activity_can_raise_the_count");
    puts("anon_fault_is_not_a_disk_claim=yes");

    unsigned long rss_after_read = 0;
    if (!smaps_kb((unsigned long)(uintptr_t)mapping, "Rss", &rss_after_read)) {
        fail("smaps after read");
    }
    printf("rss_after_read_kb=%lu\n", rss_after_read);
    puts("read_rss_not_asserted=shared_zero_page_or_private");

    ((volatile unsigned char *)mapping)[0] = 0xA5;
    if (((volatile unsigned char *)mapping)[0] != 0xA5) {
        fail("store");
    }
    puts("anon_store_visible=yes");

    resident = 0;
    if (mincore(mapping, page, &resident) != 0) {
        fail("mincore after");
    }
    if ((resident & 1U) == 0) {
        fail("not resident after touch");
    }
    puts("anon_touched_resident=yes");

    unsigned long rss_after = 0;
    if (!smaps_kb((unsigned long)(uintptr_t)mapping, "Rss", &rss_after)) {
        fail("smaps after");
    }
    if (rss_after != page / 1024U) {
        fail("rss after touch");
    }
    puts("touched_rss_matches_page=yes");

    if (munmap(mapping, page) != 0) {
        fail("munmap");
    }
    puts("descendants=none");
    puts("temp_files=none");
}

int main(int argc, char **argv) {
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: fault_lab demo\n");
        return EXIT_FAILURE;
    }
    mode_demo();
    return EXIT_SUCCESS;
}
