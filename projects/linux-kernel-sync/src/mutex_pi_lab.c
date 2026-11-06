#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static pthread_mutex_t mutex;
static void timeout_handler(int sig) {
  (void)sig;
  _Exit(124);
}
static void fail(int error, const char *what) {
  if (error != 0) {
    errno = error;
    perror(what);
    exit(EXIT_FAILURE);
  }
}
static void *waiter(void *arg) {
  (void)arg;
  fail(pthread_mutex_lock(&mutex), "lock");
  fail(pthread_mutex_unlock(&mutex), "unlock");
  return NULL;
}
int main(void) {
  struct sigaction sa;
  pthread_mutexattr_t attr;
  pthread_t thread;
  struct timespec pause_time = {0, 5000000L};
  int pi;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = timeout_handler;
  if (sigaction(SIGALRM, &sa, NULL) != 0)
    return EXIT_FAILURE;
  alarm(5U);
  fail(pthread_mutexattr_init(&attr), "attr_init");
  pi = pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
  if (pi == 0)
    puts("pi_attribute=supported");
  else
    printf("pi_attribute=SKIP error=%d\n", pi);
  fail(pthread_mutex_init(&mutex, &attr), "mutex_init");
  fail(pthread_mutexattr_destroy(&attr), "attr_destroy");
  fail(pthread_mutex_lock(&mutex), "main_lock");
  fail(pthread_create(&thread, NULL, waiter, NULL), "create");
  (void)nanosleep(&pause_time, NULL);
  fail(pthread_mutex_unlock(&mutex), "main_unlock");
  fail(pthread_join(thread, NULL), "join");
  fail(pthread_mutex_destroy(&mutex), "destroy");
  alarm(0U);
  puts("rt_policy_requested=no\nPASS mutex_pi_lab");
  return EXIT_SUCCESS;
}
