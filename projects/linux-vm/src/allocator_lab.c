#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

enum { LARGE_BYTES = 2 * 1024 * 1024 };

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int maps_name(unsigned long needle, char *name_out, size_t name_cap) {
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps == NULL) {
        return 0;
    }
    char line[512];
    int found = 0;
    while (fgets(line, (int)sizeof(line), maps) != NULL) {
        unsigned long start = 0;
        unsigned long end = 0;
        char path[256];
        path[0] = '\0';
        int fields = sscanf(line, "%lx-%lx %*s %*s %*s %*s %255[^\n]", &start,
                            &end, path);
        if (fields < 2 || needle < start || needle >= end) {
            continue;
        }
        if (fields >= 3 && path[0] != '\0') {
            snprintf(name_out, name_cap, "%s", path);
        } else {
            snprintf(name_out, name_cap, "%s", "anonymous");
        }
        found = 1;
        break;
    }
    fclose(maps);
    return found;
}

static void mode_demo(void) {
    void *brk_before = sbrk(0);
    if (brk_before == (void *)-1) {
        fail("sbrk");
    }
    void *small = malloc(32);
    void *large = malloc(LARGE_BYTES);
    if (small == NULL || large == NULL) {
        fail("malloc");
    }
    void *brk_after = sbrk(0);
    if (brk_after == (void *)-1) {
        fail("sbrk");
    }
    ((volatile unsigned char *)small)[0] = 1;
    ((volatile unsigned char *)large)[0] = 2;
    printf("brk_changed=%s\n", brk_before == brk_after ? "no" : "yes");
    printf("bounded_alloc_bytes=%d\n", LARGE_BYTES);
    puts("glibc_mmap_threshold_asserted=no");

    char small_name[256];
    char large_name[256];
    if (!maps_name((unsigned long)(uintptr_t)small, small_name, sizeof(small_name)) ||
        !maps_name((unsigned long)(uintptr_t)large, large_name, sizeof(large_name))) {
        fail("maps malloc");
    }
    printf("small_region=%s\n", small_name);
    printf("large_region=%s\n", large_name);
    puts("regions_are_observations=yes");

    free(small);
    free(large);
    puts("malloc_lifetime_released=yes");

    long page_long = sysconf(_SC_PAGESIZE);
    if (page_long < 1) {
        fail("pagesize");
    }
    size_t page = (size_t)page_long;
    void *mapping = mmap(NULL, page, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) {
        fail("mmap");
    }
    char mapped_name[256];
    if (!maps_name((unsigned long)(uintptr_t)mapping, mapped_name, sizeof(mapped_name))) {
        fail("maps mmap");
    }
    puts("explicit_mmap_in_maps=yes");
    if (munmap(mapping, page) != 0) {
        fail("munmap");
    }
    if (maps_name((unsigned long)(uintptr_t)mapping, mapped_name, sizeof(mapped_name))) {
        fail("mapping still present");
    }
    puts("munmap_absent_from_maps=yes");
    puts("descendants=none");
    puts("temp_files=none");
}

int main(int argc, char **argv) {
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: allocator_lab demo\n");
        return EXIT_FAILURE;
    }
    mode_demo();
    return EXIT_SUCCESS;
}
