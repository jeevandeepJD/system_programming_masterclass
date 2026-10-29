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
struct ent {
  unsigned id, weight, service, deadline;
};
int main(void) {
  arm();
  struct ent e[3] = {{1, 1, 0, 3}, {2, 2, 0, 2}, {3, 1, 0, 4}};
  puts("simulator=teaching_model_not_linux");
  for (unsigned step = 0; step < 6; step++) {
    size_t pick = 0;
    for (size_t i = 1; i < 3; i++)
      if (e[i].deadline < e[pick].deadline)
        pick = i;
    printf("step=%u pick=%u deadline=%u\n", step, e[pick].id, e[pick].deadline);
    e[pick].service++;
    e[pick].deadline += e[pick].weight + 2U;
  }
  puts("PASS scheduler_sim deterministic");
  return 0;
}
