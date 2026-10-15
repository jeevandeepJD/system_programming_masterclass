#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int maps_has_address(unsigned long needle) {
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps == NULL) {
        return 0;
    }
    char line[512];
    int found = 0;
    while (fgets(line, (int)sizeof(line), maps) != NULL) {
        unsigned long start = 0;
        unsigned long end = 0;
        if (sscanf(line, "%lx-%lx", &start, &end) == 2 && needle >= start &&
            needle < end) {
            found = 1;
            break;
        }
    }
    fclose(maps);
    return found;
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
        if (sscanf(line + key_len + 1, "%lu", kb_out) == 1) {
            found = 1;
            break;
        }
    }
    fclose(smaps);
    return found;
}

static int status_kb(const char *key, unsigned long *kb_out) {
    FILE *status = fopen("/proc/self/status", "r");
    if (status == NULL) {
        return 0;
    }
    char line[256];
    int found = 0;
    size_t key_len = strlen(key);
    while (fgets(line, (int)sizeof(line), status) != NULL) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == ':') {
            if (sscanf(line + key_len + 1, "%lu", kb_out) == 1) {
                found = 1;
                break;
            }
        }
    }
    fclose(status);
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
    unsigned long mapping_addr = (unsigned long)(uintptr_t)mapping;
    if (!maps_has_address(mapping_addr)) {
        fail("maps");
    }
    puts("maps_anon=yes");

    unsigned char value = ((volatile unsigned char *)mapping)[0];
    if (value != 0) {
        fail("zero fill");
    }
    puts("zero_fill=yes");
    ((volatile unsigned char *)mapping)[0] = 0x5A;
    if (((volatile unsigned char *)mapping)[0] != 0x5A) {
        fail("store");
    }

    unsigned long rss_kb = 0;
    if (!smaps_kb(mapping_addr, "Rss", &rss_kb)) {
        fail("smaps");
    }
    if (rss_kb != page / 1024U) {
        fail("rss");
    }
    puts("touched_rss_matches_page=yes");

    unsigned long vm_rss = 0;
    unsigned long vm_size = 0;
    if (!status_kb("VmRSS", &vm_rss) || !status_kb("VmSize", &vm_size)) {
        fail("status");
    }
    printf("vm_rss_kb=%lu\n", vm_rss);
    printf("vm_size_kb=%lu\n", vm_size);
    puts("status_has_vmrss=yes");
    puts("status_has_vmsize=yes");
    puts("counters_are_observations=yes");
    puts("pressure_induced=no");
    puts("oom_induced=no");
    puts("swap_induced=no");
    puts("vm_observation=proc_self_only");

    if (munmap(mapping, page) != 0) {
        fail("munmap");
    }
    puts("descendants=none");
    puts("temp_files=none");
}

int main(int argc, char **argv) {
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: debug_lab demo\n");
        return EXIT_FAILURE;
    }
    mode_demo();
    return EXIT_SUCCESS;
}
