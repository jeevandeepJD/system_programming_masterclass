#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static char workdir[] = "/tmp/week13-map.XXXXXX";
static int workdir_live;
static int in_child;
static pid_t kids[4];
static int n_kids;

static void reap_live(void) {
    for (int index = 0; index < n_kids; index++) {
        if (kids[index] > 0) {
            kill(kids[index], SIGKILL);
            int status = 0;
            waitpid(kids[index], &status, 0);
            kids[index] = -1;
        }
    }
}

static void cleanup(void) {
    if (in_child || !workdir_live) {
        return;
    }
    reap_live();
    char path[128];
    const char *names[] = {"shared", "private"};
    for (size_t index = 0; index < 2; index++) {
        int wrote = snprintf(path, sizeof(path), "%s/%s", workdir, names[index]);
        if (wrote > 0 && (size_t)wrote < sizeof(path)) {
            unlink(path);
        }
    }
    rmdir(workdir);
    workdir_live = 0;
}

static void fail(const char *what) {
    perror(what);
    if (in_child) {
        _exit(2);
    }
    exit(EXIT_FAILURE);
}

static void on_alarm(int signo) {
    (void)signo;
}

static void remember(pid_t pid) {
    if (n_kids < 4) {
        kids[n_kids] = pid;
        n_kids++;
    }
}

static void forget(pid_t pid) {
    for (int index = 0; index < n_kids; index++) {
        if (kids[index] == pid) {
            kids[index] = -1;
        }
    }
}

static int wait_child(pid_t pid, int *status_out) {
    alarm(4);
    pid_t got = waitpid(pid, status_out, 0);
    if (got == pid) {
        alarm(0);
        forget(pid);
        return 0;
    }
    alarm(0);
    kill(pid, SIGKILL);
    waitpid(pid, status_out, 0);
    forget(pid);
    return -1;
}

static int open_filled(const char *name, unsigned char fill, size_t page) {
    char path[128];
    int wrote = snprintf(path, sizeof(path), "%s/%s", workdir, name);
    if (wrote < 0 || (size_t)wrote >= sizeof(path)) {
        fail("path");
    }
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, (mode_t)0600);
    if (fd < 0) {
        fail("open");
    }
    if (ftruncate(fd, (off_t)page) != 0) {
        fail("ftruncate");
    }
    if (pwrite(fd, &fill, 1, 0) != 1) {
        fail("pwrite");
    }
    return fd;
}

static void expect_exit_ok(pid_t pid) {
    int status = 0;
    if (wait_child(pid, &status) != 0 || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        fail("child exit");
    }
}

static void mode_demo(void) {
    long page_long = sysconf(_SC_PAGESIZE);
    if (page_long < 1) {
        fail("pagesize");
    }
    size_t page = (size_t)page_long;
    if (mkdtemp(workdir) == NULL) {
        fail("mkdtemp");
    }
    workdir_live = 1;

    int shared_fd = open_filled("shared", (unsigned char)'S', page);
    volatile unsigned char *shared = mmap(NULL, page, PROT_READ | PROT_WRITE,
                                          MAP_SHARED, shared_fd, 0);
    if (shared == MAP_FAILED) {
        fail("mmap shared");
    }
    if (shared[0] != (unsigned char)'S') {
        fail("shared initial");
    }
    pid_t shared_child = fork();
    if (shared_child < 0) {
        fail("fork shared");
    }
    if (shared_child == 0) {
        in_child = 1;
        shared[0] = (unsigned char)'C';
        _exit(shared[0] == (unsigned char)'C' ? 0 : 3);
    }
    remember(shared_child);
    expect_exit_ok(shared_child);
    if (shared[0] != (unsigned char)'C') {
        fail("shared visibility");
    }
    puts("shared_child_store_visible=yes");

    int private_fd = open_filled("private", (unsigned char)'P', page);
    volatile unsigned char *private_map =
        mmap(NULL, page, PROT_READ | PROT_WRITE, MAP_PRIVATE, private_fd, 0);
    if (private_map == MAP_FAILED) {
        fail("mmap private");
    }
    if (private_map[0] != (unsigned char)'P') {
        fail("private initial");
    }
    int pipe_fd[2];
    if (pipe(pipe_fd) != 0) {
        fail("pipe");
    }
    pid_t private_child = fork();
    if (private_child < 0) {
        fail("fork private");
    }
    if (private_child == 0) {
        in_child = 1;
        close(pipe_fd[0]);
        private_map[0] = (unsigned char)'C';
        const char note[] = "private_child_store=yes\n";
        size_t left = sizeof(note) - 1U;
        const char *cursor = note;
        while (left > 0) {
            ssize_t wrote = write(pipe_fd[1], cursor, left);
            if (wrote < 0) {
                _exit(4);
            }
            cursor += wrote;
            left -= (size_t)wrote;
        }
        _exit(private_map[0] == (unsigned char)'C' ? 0 : 3);
    }
    remember(private_child);
    close(pipe_fd[1]);
    char note[64];
    size_t got = 0;
    while (got + 1U < sizeof(note)) {
        ssize_t count = read(pipe_fd[0], note + got, sizeof(note) - 1U - got);
        if (count == 0) {
            break;
        }
        if (count < 0) {
            fail("read pipe");
        }
        got += (size_t)count;
    }
    note[got] = '\0';
    close(pipe_fd[0]);
    expect_exit_ok(private_child);
    if (private_map[0] != (unsigned char)'P') {
        fail("private parent changed");
    }
    if (strcmp(note, "private_child_store=yes\n") != 0) {
        fail("private child note");
    }
    puts("private_parent_unchanged=yes");
    puts("private_child_store=yes");

    volatile unsigned char *guarded =
        mmap(NULL, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (guarded == MAP_FAILED) {
        fail("mmap guard");
    }
    guarded[0] = 1;
    if (mprotect((void *)guarded, page, PROT_READ) != 0) {
        fail("mprotect");
    }
    pid_t faulting = fork();
    if (faulting < 0) {
        fail("fork mprotect");
    }
    if (faulting == 0) {
        in_child = 1;
        guarded[0] = 2;
        _exit(0);
    }
    remember(faulting);
    int status = 0;
    if (wait_child(faulting, &status) != 0) {
        fail("wait mprotect");
    }
    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGSEGV) {
        fail("mprotect status");
    }
    printf("child_status_decoded=signaled:%d\n", WTERMSIG(status));
    puts("mprotect_child_sigsegv=yes");
    if (guarded[0] != 1) {
        fail("parent store changed");
    }
    puts("parent_read_survived=yes");
    puts("mprotect_fault_in_child_only=yes");

    if (munmap((void *)shared, page) != 0 || munmap((void *)private_map, page) != 0 ||
        munmap((void *)guarded, page) != 0) {
        fail("munmap");
    }
    if (close(shared_fd) != 0 || close(private_fd) != 0) {
        fail("close");
    }
    puts("children_reaped=yes");
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
    if (signal(SIGALRM, on_alarm) == SIG_ERR) {
        fail("signal");
    }
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: mapping_lab demo\n");
        return EXIT_FAILURE;
    }
    mode_demo();
    return EXIT_SUCCESS;
}
