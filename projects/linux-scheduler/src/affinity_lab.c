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
  arm();
  cpu_set_t set;
  if (sched_getaffinity(0, sizeof(set), &set) != 0)
    die("sched_getaffinity");
  unsigned n = 0;
  size_t first = CPU_SETSIZE;
  for (size_t i = 0; i < (size_t)CPU_SETSIZE; i++)
    if (CPU_ISSET(i, &set)) {
      n++;
      if (first == (size_t)CPU_SETSIZE)
        first = i;
    }
  printf("allowed_cpus=%u first_allowed=%zu\n", n, first);
  if (first < (size_t)CPU_SETSIZE) {
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(first, &one);
    if (sched_setaffinity(0, sizeof(one), &one) != 0)
      die("sched_setaffinity");
    puts("scope=self_only");
  }
  puts("PASS affinity_lab");
  return 0;
}
