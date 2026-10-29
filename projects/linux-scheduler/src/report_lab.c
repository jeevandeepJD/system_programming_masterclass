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
  printf("pid=%ld\n", (long)getpid());
  FILE *f = fopen("/proc/self/status", "r");
  if (!f) {
    puts("proc_status=SKIP");
    return 0;
  }
  char b[256];
  while (fgets(b, sizeof(b), f))
    if (strncmp(b, "voluntary_ctxt_switches", 23) == 0 ||
        strncmp(b, "nonvoluntary_ctxt_switches", 26) == 0)
      fputs(b, stdout);
  if (fclose(f) != 0)
    die("fclose");
  puts("report_scope=self_proc_read_only\nPASS report_lab");
  return 0;
}
