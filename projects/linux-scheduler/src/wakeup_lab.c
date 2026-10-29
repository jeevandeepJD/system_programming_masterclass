#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static void die(const char *s) {
  perror(s);
  exit(EXIT_FAILURE);
}
static void timeout_handler(int sig) {
  (void)sig;
  _Exit(124);
}
static void arm(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = timeout_handler;
  if (sigaction(SIGALRM, &sa, NULL) != 0)
    die("sigaction");
  alarm(4U);
}
int main(void) {
  int p[2];
  arm();
  if (pipe(p) != 0)
    die("pipe");
  struct timespec a, b;
  if (clock_gettime(CLOCK_MONOTONIC, &a) != 0)
    die("clock");
  pid_t c = fork();
  if (c < 0)
    die("fork");
  if (c == 0) {
    close(p[0]);
    struct timespec d = {0, 20000000L};
    (void)nanosleep(&d, NULL);
    if (write(p[1], "x", 1) != 1)
      _Exit(2);
    close(p[1]);
    _Exit(0);
  }
  close(p[1]);
  char x;
  if (read(p[0], &x, 1) != 1)
    die("read");
  if (clock_gettime(CLOCK_MONOTONIC, &b) != 0)
    die("clock");
  int st;
  if (waitpid(c, &st, 0) < 0)
    die("waitpid");
  long ms = (b.tv_sec - a.tv_sec) * 1000L + (b.tv_nsec - a.tv_nsec) / 1000000L;
  printf("sample_wait_ms=%ld\nclaim=userspace_pipe_observation_only\n", ms);
  return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : 1;
}
