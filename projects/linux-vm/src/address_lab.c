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

static int maps_region(unsigned long needle, unsigned long *start_out,
                       unsigned long *end_out, char *perms_out, size_t perms_cap,
                       char *name_out, size_t name_cap) {
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps == NULL) {
        return 0;
    }
    char line[512];
    int found = 0;
    while (fgets(line, (int)sizeof(line), maps) != NULL) {
        unsigned long start = 0;
        unsigned long end = 0;
        char perms[8];
        char path[256];
        path[0] = '\0';
        int fields = sscanf(line, "%lx-%lx %7s %*s %*s %*s %255[^\n]", &start,
                            &end, perms, path);
        if (fields < 3) {
            continue;
        }
        if (needle < start || needle >= end) {
            continue;
        }
        if (perms_cap > 0) {
            snprintf(perms_out, perms_cap, "%s", perms);
        }
        if (name_cap > 0) {
            if (fields >= 4 && path[0] != '\0') {
                snprintf(name_out, name_cap, "%s", path);
            } else {
                snprintf(name_out, name_cap, "%s", "anonymous");
            }
        }
        *start_out = start;
        *end_out = end;
        found = 1;
        break;
    }
    fclose(maps);
    return found;
}

static int maps_contains(const char *label) {
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps == NULL) {
        return 0;
    }
    char line[512];
    int found = 0;
    while (fgets(line, (int)sizeof(line), maps) != NULL) {
        if (strstr(line, label) != NULL) {
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
        if (!in_region) {
            continue;
        }
        if (strncmp(line, key, key_len) == 0 && line[key_len] == ':') {
            unsigned long kb = 0;
            if (sscanf(line + key_len + 1, "%lu", &kb) == 1) {
                *kb_out = kb;
                found = 1;
                break;
            }
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
    unsigned long needle = (unsigned long)(uintptr_t)mapping;
    unsigned long start = 0;
    unsigned long end = 0;
    char perms[8];
    char name[256];
    if (!maps_region(needle, &start, &end, perms, sizeof(perms), name,
                     sizeof(name))) {
        fail("maps lookup");
    }
    if (end - start != page) {
        fail("map length");
    }
    puts("maps_contains_anon=yes");
    if (strchr(perms, 'w') != NULL) {
        puts("anon_perms_writable=yes");
    }
    printf("anon_region_name=%s\n", name);

    unsigned long size_kb = 0;
    unsigned long rss_kb = 0;
    if (!smaps_kb(start, "Size", &size_kb) || !smaps_kb(start, "Rss", &rss_kb)) {
        fail("smaps");
    }
    if (size_kb != page / 1024U) {
        fail("smaps size");
    }
    puts("smaps_size_matches_page=yes");
    printf("anon_untouched_rss_kb=%lu\n", rss_kb);
    if (rss_kb == 0) {
        puts("anon_untouched_resident=no");
    }

    if (!maps_contains("[stack]")) {
        fail("stack missing");
    }
    puts("maps_has_stack=yes");
    puts(maps_contains("[heap]") ? "maps_heap=present" : "maps_heap=absent");
    puts("heap_presence_not_a_glibc_contract=yes");

    if (munmap(mapping, page) != 0) {
        fail("munmap");
    }
    puts("descendants=none");
    puts("temp_files=none");
}

int main(int argc, char **argv) {
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: address_lab demo\n");
        return EXIT_FAILURE;
    }
    mode_demo();
    return EXIT_SUCCESS;
}
