#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static char workdir[] = "/tmp/week12-mmap.XXXXXX";
static int workdir_live;

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void cleanup(void) {
    if (!workdir_live) {
        return;
    }
    char path[128];
    int wrote = snprintf(path, sizeof(path), "%s/mapped", workdir);
    if (wrote > 0 && (size_t)wrote < sizeof(path)) {
        unlink(path);
    }
    rmdir(workdir);
    workdir_live = 0;
}

static long long elapsed_ns(const struct timespec *start, const struct timespec *end) {
    long long sec = (long long)end->tv_sec - (long long)start->tv_sec;
    long long nsec = (long long)end->tv_nsec - (long long)start->tv_nsec;
    return sec * 1000000000LL + nsec;
}

static unsigned char checksum(const unsigned char *p, size_t n) {
    unsigned char sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum = (unsigned char)(sum + p[i]);
    }
    return sum;
}

static void mode_demo(void) {
    if (mkdtemp(workdir) == NULL) {
        fail("mkdtemp");
    }
    workdir_live = 1;

    long page = sysconf(_SC_PAGESIZE);
    if (page < 1) {
        fail("pagesize");
    }
    size_t len = (size_t)page * 64U;

    char path[128];
    int wrote = snprintf(path, sizeof(path), "%s/mapped", workdir);
    if (wrote < 0 || (size_t)wrote >= sizeof(path)) {
        fail("path");
    }
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, (mode_t)0600);
    if (fd < 0) {
        fail("open");
    }
    if (ftruncate(fd, (off_t)len) != 0) {
        fail("ftruncate");
    }
    void *mapped = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        fail("mmap");
    }
    unsigned char *map = mapped;

    memcpy(map, "mmap-store", 10);
    char got[10];
    if (pread(fd, got, sizeof(got), 0) != (ssize_t)sizeof(got) ||
        memcmp(got, "mmap-store", sizeof(got)) != 0) {
        fail("store not visible to pread");
    }
    puts("mmap_store_visible_to_pread=yes");

    if (msync(map, len, MS_SYNC) != 0) {
        fail("msync");
    }
    puts("msync_completed=yes");
    puts("msync_is_not_crash_proof=yes");

    if (pwrite(fd, "write-path", 10, (off_t)32) != 10) {
        fail("pwrite");
    }
    if (memcmp(map + 32, "write-path", 10) != 0) {
        fail("pwrite not visible in mapping");
    }
    puts("pwrite_visible_to_mapping=yes");

    for (size_t i = 0; i < len; i += (size_t)page) {
        map[i] = (unsigned char)(map[i] + 1U);
    }
    unsigned char *buf = calloc(len, 1U);
    if (buf == NULL) {
        fail("calloc");
    }
    struct timespec t0;
    struct timespec t1;
    struct timespec t2;
    if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) {
        fail("clock");
    }
    if (pread(fd, buf, len, 0) != (ssize_t)len) {
        fail("pread copy");
    }
    unsigned char read_sum = checksum(buf, len);
    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
        fail("clock");
    }
    memcpy(buf, map, len);
    unsigned char map_sum = checksum(buf, len);
    if (clock_gettime(CLOCK_MONOTONIC, &t2) != 0) {
        fail("clock");
    }
    printf("read_copy_ns=%lld mmap_copy_ns=%lld checksum=%u/%u\n",
           elapsed_ns(&t0, &t1), elapsed_ns(&t1, &t2), (unsigned)read_sum,
           (unsigned)map_sum);
    puts("performance_claim=none");
    puts("timing=single_run_not_universal");
    free(buf);

    if (munmap(mapped, len) != 0) {
        fail("munmap");
    }
    if (close(fd) != 0) {
        fail("close");
    }
    puts("descendants=none");
    cleanup();
    struct stat st;
    if (stat(workdir, &st) == 0) {
        fail("temp directory remains");
    }
    puts("temp_cleanup=done");
}

int main(int argc, char **argv) {
    if (atexit(cleanup) != 0) {
        fail("atexit");
    }
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: mmap_lab demo\n");
        return 2;
    }
    mode_demo();
    return 0;
}
