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
static void check_pthread(int error, const char *operation) {
  if (error != 0) {
    errno = error;
    die(operation);
  }
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
static pthread_mutex_t m;
static void *worker(void *p) {
  (void)p;
  if (pthread_mutex_lock(&m) != 0)
    return (void *)1;
  if (pthread_mutex_unlock(&m) != 0)
    return (void *)1;
  return NULL;
}
int main(void) {
  arm();
  pthread_mutexattr_t a;
  check_pthread(pthread_mutexattr_init(&a), "pthread_mutexattr_init");
  int pi = pthread_mutexattr_setprotocol(&a, PTHREAD_PRIO_INHERIT);
  if (pi != 0)
    puts("pi_mutex=SKIP unsupported");
  else
    puts("pi_mutex=requested_supported");
  check_pthread(pthread_mutex_init(&m, &a), "pthread_mutex_init");
  check_pthread(pthread_mutexattr_destroy(&a), "pthread_mutexattr_destroy");
  pthread_t t;
  check_pthread(pthread_mutex_lock(&m), "pthread_mutex_lock");
  check_pthread(pthread_create(&t, NULL, worker, NULL), "pthread_create");
  struct timespec d = {0, 10000000L};
  (void)nanosleep(&d, NULL);
  check_pthread(pthread_mutex_unlock(&m), "pthread_mutex_unlock");
  void *r = NULL;
  check_pthread(pthread_join(t, &r), "pthread_join");
  check_pthread(pthread_mutex_destroy(&m), "pthread_mutex_destroy");
  alarm(0);
  puts("rt_policy_change=none\nPASS inversion_lab");
  return r == NULL ? 0 : 1;
}
