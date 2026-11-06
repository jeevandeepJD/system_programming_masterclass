#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
enum { THREADS = 4, ITERATIONS = 10000 };
static atomic_flag lock = ATOMIC_FLAG_INIT;
static unsigned long counter;
static void timeout_handler(int sig) {
  (void)sig;
  _Exit(124);
}
static void lock_flag(void) {
  while (atomic_flag_test_and_set_explicit(&lock, memory_order_acquire)) {
  }
}
static void unlock_flag(void) {
  atomic_flag_clear_explicit(&lock, memory_order_release);
}
static void *worker(void *arg) {
  int i;
  (void)arg;
  for (i = 0; i < ITERATIONS; ++i) {
    lock_flag();
    ++counter;
    unlock_flag();
  }
  return NULL;
}
int main(void) {
  struct sigaction sa;
  pthread_t threads[THREADS];
  int i;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = timeout_handler;
  if (sigaction(SIGALRM, &sa, NULL) != 0)
    return EXIT_FAILURE;
  alarm(5U);
  for (i = 0; i < THREADS; ++i)
    if (pthread_create(&threads[i], NULL, worker, NULL) != 0)
      return EXIT_FAILURE;
  for (i = 0; i < THREADS; ++i)
    if (pthread_join(threads[i], NULL) != 0)
      return EXIT_FAILURE;
  alarm(0U);
  if (counter != (unsigned long)THREADS * (unsigned long)ITERATIONS)
    return EXIT_FAILURE;
  printf("threads=%d bounded_iterations=%d counter=%lu\nPASS spinlock_lab\n",
         THREADS, ITERATIONS, counter);
  return EXIT_SUCCESS;
}
