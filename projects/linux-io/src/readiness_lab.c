#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <unistd.h>

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void close_pair(int fds[2]) {
    if (close(fds[0]) != 0 || close(fds[1]) != 0) {
        fail("close");
    }
}

static int wait_epoll(int ep, int timeout_ms) {
    struct epoll_event out;
    memset(&out, 0, sizeof(out));
    for (;;) {
        int n = epoll_wait(ep, &out, 1, timeout_ms);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return n;
    }
}

static void observe_ready_short_read(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    if (set_nonblock(fds[0]) != 0) {
        fail("O_NONBLOCK");
    }
    if (write(fds[1], "wxyz", 4) != 4) {
        fail("write");
    }
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fds[0], &rfds);
    struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
    int ready = select(fds[0] + 1, &rfds, NULL, NULL, &tv);
    if (ready != 1) {
        fail("select should be ready");
    }
    char buf[100];
    ssize_t n = read(fds[0], buf, sizeof(buf));
    if (n != 4) {
        fail("ready read returned an unexpected count");
    }
    puts("ready_short_read=4 requested=100");
    puts("limit=readiness_is_not_a_full_transfer");
    close_pair(fds);
}

static void observe_select_poll(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    if (set_nonblock(fds[0]) != 0) {
        fail("O_NONBLOCK");
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fds[0], &rfds);
    struct timeval empty_tv = {.tv_sec = 0, .tv_usec = 0};
    int empty = select(fds[0] + 1, &rfds, NULL, NULL, &empty_tv);
    if (empty != 0) {
        fail("select empty");
    }
    puts("select_empty=0");

    if (write(fds[1], "wxyz", 4) != 4) {
        fail("write");
    }
    FD_ZERO(&rfds);
    FD_SET(fds[0], &rfds);
    struct timeval ready_tv = {.tv_sec = 0, .tv_usec = 0};
    int ready = select(fds[0] + 1, &rfds, NULL, NULL, &ready_tv);
    if (ready != 1 || !FD_ISSET(fds[0], &rfds)) {
        fail("select ready");
    }
    puts("select_ready=1");

    struct pollfd pfd = {.fd = fds[0], .events = (short)POLLIN, .revents = 0};
    int polled = poll(&pfd, 1, 0);
    if (polled != 1 || (pfd.revents & POLLIN) == 0) {
        fail("poll ready");
    }
    puts("poll_ready=1");

    char buf[4];
    if (read(fds[0], buf, sizeof(buf)) != 4) {
        fail("drain");
    }
    pfd.revents = 0;
    polled = poll(&pfd, 1, 0);
    if (polled != 0) {
        fail("poll empty");
    }
    puts("poll_empty=0");
    close_pair(fds);
}

static void observe_epoll(void) {
    int fds[2];
    if (pipe(fds) != 0) {
        fail("pipe");
    }
    if (set_nonblock(fds[0]) != 0) {
        fail("O_NONBLOCK");
    }
    int ep = epoll_create1(EPOLL_CLOEXEC);
    if (ep < 0) {
        fail("epoll_create1");
    }
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = (uint32_t)EPOLLIN;
    ev.data.fd = fds[0];
    if (epoll_ctl(ep, EPOLL_CTL_ADD, fds[0], &ev) != 0) {
        fail("epoll_ctl add");
    }
    if (write(fds[1], "wxyz", 4) != 4) {
        fail("write");
    }
    if (wait_epoll(ep, 0) != 1) {
        fail("epoll LT first");
    }
    char byte = 0;
    if (read(fds[0], &byte, 1) != 1) {
        fail("partial read");
    }
    if (wait_epoll(ep, 0) != 1) {
        fail("epoll LT after partial read");
    }
    puts("epoll_lt_partial=ready");

    char rest[8];
    ssize_t n = read(fds[0], rest, sizeof(rest));
    if (n != 3) {
        fail("lt drain");
    }
    n = read(fds[0], rest, sizeof(rest));
    if (n >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
        fail("lt EAGAIN");
    }
    if (wait_epoll(ep, 0) != 0) {
        fail("epoll LT empty");
    }

    ev.events = (uint32_t)EPOLLIN | (uint32_t)EPOLLET;
    if (epoll_ctl(ep, EPOLL_CTL_MOD, fds[0], &ev) != 0) {
        fail("epoll_ctl mod");
    }
    if (write(fds[1], "wxyz", 4) != 4) {
        fail("write edge");
    }
    if (wait_epoll(ep, 0) != 1) {
        fail("epoll ET edge");
    }
    if (read(fds[0], &byte, 1) != 1) {
        fail("et partial");
    }
    if (wait_epoll(ep, 0) != 0) {
        fail("epoll ET repeated without a new edge");
    }
    puts("epoll_et_partial=not_ready");

    n = read(fds[0], rest, sizeof(rest));
    if (n != 3) {
        fail("et drain bytes");
    }
    n = read(fds[0], rest, sizeof(rest));
    if (n >= 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
        fail("et EAGAIN");
    }
    puts("epoll_et_drain=EAGAIN");

    if (write(fds[1], "Q", 1) != 1) {
        fail("new edge");
    }
    if (wait_epoll(ep, 0) != 1) {
        fail("epoll ET new edge");
    }
    puts("epoll_et_new_edge=ready");

    if (close(ep) != 0) {
        fail("close epoll");
    }
    close_pair(fds);
}

static void mode_demo(void) {
    observe_ready_short_read();
    observe_select_poll();
    observe_epoll();
    puts("descendants=none");
    puts("temp_files=none");
}

int main(int argc, char **argv) {
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: readiness_lab demo\n");
        return 2;
    }
    mode_demo();
    return 0;
}
