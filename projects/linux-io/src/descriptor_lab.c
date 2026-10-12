#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char workdir[] = "/tmp/week12-desc.XXXXXX";
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
    int wrote = snprintf(path, sizeof(path), "%s/bytes", workdir);
    if (wrote > 0 && (size_t)wrote < sizeof(path)) {
        unlink(path);
    }
    (void)rmdir(workdir);
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

static int read_exact(int fd, void *data, size_t size) {
    unsigned char *p = data;
    while (size > 0U) {
        ssize_t n = read(fd, p, size);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return -1;
        }
        p += (size_t)n;
        size -= (size_t)n;
    }
    return 0;
}

static void expect_bytes(int fd, const char *want) {
    size_t n = strlen(want);
    char got[16];
    if (n >= sizeof(got)) {
        fail("expect size");
    }
    if (read_exact(fd, got, n) != 0) {
        fail("read");
    }
    if (memcmp(got, want, n) != 0) {
        fail("unexpected bytes");
    }
}

static void mode_demo(void) {
    if (mkdtemp(workdir) == NULL) {
        fail("mkdtemp");
    }
    workdir_live = 1;

    char path[128];
    int wrote = snprintf(path, sizeof(path), "%s/bytes", workdir);
    if (wrote < 0 || (size_t)wrote >= sizeof(path)) {
        fail("path");
    }
    int fd_a = open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, (mode_t)0600);
    if (fd_a < 0) {
        fail("open");
    }
    const char payload[] = "0123456789ABCDEF";
    if (write_all(fd_a, payload, sizeof(payload) - 1U) != 0) {
        fail("write");
    }
    if (lseek(fd_a, 0, SEEK_SET) < 0) {
        fail("lseek");
    }

    expect_bytes(fd_a, "0123");
    int fd_b = dup(fd_a);
    if (fd_b < 0) {
        fail("dup");
    }
    expect_bytes(fd_b, "4567");
    puts("dup_shares_offset=yes");

    if (lseek(fd_b, 0, SEEK_SET) != 0) {
        fail("lseek dup");
    }
    expect_bytes(fd_a, "0123");
    puts("lseek_shared=yes");

    int fd_c = open(path, O_RDONLY | O_CLOEXEC);
    if (fd_c < 0) {
        fail("reopen");
    }
    expect_bytes(fd_c, "0123");
    expect_bytes(fd_a, "4567");
    puts("separate_open_independent=yes");
    printf("fd_original=%d fd_dup=%d fd_reopen=%d\n", fd_a, fd_b, fd_c);

    if (close(fd_a) != 0 || close(fd_b) != 0 || close(fd_c) != 0) {
        fail("close");
    }
    puts("limit=descriptor_is_not_a_kernel_address");
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
        fprintf(stderr, "usage: descriptor_lab demo\n");
        return 2;
    }
    mode_demo();
    return 0;
}
