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
  int a[2], b[2];
  arm();
  if (pipe(a) || pipe(b))
    die("pipe");
  pid_t c = fork();
  if (c < 0)
    die("fork");
  if (c == 0) {
    close(a[1]);
    close(b[0]);
    char x;
    for (unsigned i = 0; i < 32; i++) {
      if (read(a[0], &x, 1) != 1)
        _Exit(2);
      if (write(b[1], "y", 1) != 1)
        _Exit(3);
    }
    _Exit(0);
  }
  close(a[0]);
  close(b[1]);
  char x;
  for (unsigned i = 0; i < 32; i++) {
    if (write(a[1], "x", 1) != 1)
      die("write");
    if (read(b[0], &x, 1) != 1)
      die("read");
  }
  int st;
  if (waitpid(c, &st, 0) < 0)
    die("wait");
  puts("round_trips=32\nexact_kernel_switch_claim=none\nPASS switch_lab");
  return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : 1;
}
