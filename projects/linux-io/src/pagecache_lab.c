#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char workdir[] = "/tmp/week12-page.XXXXXX";
static int workdir_live;

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void cleanup(void) {
    if (!workdir_live) {
        return;
    }
    const char *names[] = {"cached", "sparse"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char path[128];
        int wrote = snprintf(path, sizeof(path), "%s/%s", workdir, names[i]);
        if (wrote > 0 && (size_t)wrote < sizeof(path)) {
            unlink(path);
        }
    }
    rmdir(workdir);
    workdir_live = 0;
}

static int write_all(int fd, const void *data, size_t size) {
    const unsigned char *p = data;
    while (size > 0U) {
        ssize_t n = write(fd, p, size);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        p += (size_t)n;
        size -= (size_t)n;
    }
    return 0;
}

static void path_for(char *out, size_t out_len, const char *name) {
    int wrote = snprintf(out, out_len, "%s/%s", workdir, name);
    if (wrote < 0 || (size_t)wrote >= out_len) {
        fail("path");
    }
}

static void observe_buffered(void) {
    char path[128];
    path_for(path, sizeof(path), "cached");
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, (mode_t)0600);
    if (fd < 0) {
        fail("open");
    }
    const char payload[] = "cached-bytes";
    if (write_all(fd, payload, sizeof(payload) - 1U) != 0) {
        fail("write");
    }

    int second = open(path, O_RDONLY | O_CLOEXEC);
    if (second < 0) {
        fail("second open");
    }
    char got[sizeof(payload)];
    memset(got, 0, sizeof(got));
    ssize_t n = pread(second, got, sizeof(payload) - 1U, 0);
    if (n != (ssize_t)(sizeof(payload) - 1U) ||
        memcmp(got, payload, sizeof(payload) - 1U) != 0) {
        fail("buffered readback");
    }
    puts("buffered_readback=match");
    puts("visibility=page_cache_not_crash_durability");

    if (fsync(fd) != 0) {
        fail("fsync");
    }
    puts("fsync_completed=yes");
    puts("fsync_is_not_crash_proof=yes");

    struct stat from_fd;
    struct stat from_path;
    if (fstat(fd, &from_fd) != 0 || stat(path, &from_path) != 0) {
        fail("stat");
    }
    if (from_fd.st_size != from_path.st_size ||
        from_fd.st_size != (off_t)(sizeof(payload) - 1U)) {
        fail("stat size");
    }
    printf("stat_size=%lld\n", (long long)from_path.st_size);
    puts("stat_matches_fstat=yes");

    if (close(fd) != 0 || close(second) != 0) {
        fail("close");
    }
}

static void observe_sparse(void) {
    char path[128];
    path_for(path, sizeof(path), "sparse");
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, (mode_t)0600);
    if (fd < 0) {
        fail("open sparse");
    }
    const off_t hole = (off_t)1048576;
    if (lseek(fd, hole, SEEK_SET) != hole) {
        fail("lseek hole");
    }
    if (write_all(fd, "S", 1U) != 0) {
        fail("write sparse tail");
    }

    unsigned char mid = 0xff;
    ssize_t n = pread(fd, &mid, 1, (off_t)100);
    if (n != 1 || mid != 0) {
        fail("hole read");
    }
    printf("hole_byte=%u\n", (unsigned)mid);

    struct stat st;
    if (fstat(fd, &st) != 0) {
        fail("fstat sparse");
    }
    if (st.st_size != hole + (off_t)1) {
        fail("logical size");
    }
    printf("sparse_logical_size=%lld\n", (long long)st.st_size);
    printf("sparse_blocks=%lld\n", (long long)st.st_blocks);
    unsigned long long logical = (unsigned long long)st.st_size;
    unsigned long long allocated = (unsigned long long)st.st_blocks * 512ULL;
    if (allocated >= logical) {
        fail("expected a sparse allocation");
    }
    puts("allocated_blocks_lt_logical=yes");
    if (close(fd) != 0) {
        fail("close");
    }
}

static void mode_demo(void) {
    if (mkdtemp(workdir) == NULL) {
        fail("mkdtemp");
    }
    workdir_live = 1;
    observe_buffered();
    observe_sparse();
    puts("durability_claim=none");
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
        fprintf(stderr, "usage: pagecache_lab demo\n");
        return 2;
    }
    mode_demo();
    return 0;
}
