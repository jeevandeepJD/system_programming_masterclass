#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
enum { ROUNDS = 2000 };
static atomic_int payload;
static atomic_int ready;
static atomic_int seen;
static void timeout_handler(int sig) {
  (void)sig;
  _Exit(124);
}
static void *writer(void *arg) {
  int i;
  (void)arg;
  for (i = 1; i <= ROUNDS; ++i) {
    while (atomic_load_explicit(&ready, memory_order_acquire) != 0) {
    }
    atomic_store_explicit(&payload, i, memory_order_relaxed);
    atomic_store_explicit(&ready, 1, memory_order_release);
  }
  return NULL;
}
static void *reader(void *arg) {
  int i;
  (void)arg;
  for (i = 1; i <= ROUNDS; ++i) {
    int value;
    while (atomic_load_explicit(&ready, memory_order_acquire) != 1) {
    }
    value = atomic_load_explicit(&payload, memory_order_relaxed);
    if (value == i)
      atomic_fetch_add_explicit(&seen, 1, memory_order_relaxed);
    atomic_store_explicit(&ready, 0, memory_order_release);
  }
  return NULL;
}
int main(void) {
  struct sigaction sa;
  pthread_t a, b;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = timeout_handler;
  if (sigaction(SIGALRM, &sa, NULL) != 0) {
    perror("sigaction");
    return EXIT_FAILURE;
  }
  alarm(5U);
  atomic_init(&payload, 0);
  atomic_init(&ready, 0);
  atomic_init(&seen, 0);
  if (pthread_create(&a, NULL, writer, NULL) != 0 ||
      pthread_create(&b, NULL, reader, NULL) != 0)
    return EXIT_FAILURE;
  if (pthread_join(a, NULL) != 0 || pthread_join(b, NULL) != 0)
    return EXIT_FAILURE;
  alarm(0U);
  if (atomic_load_explicit(&seen, memory_order_relaxed) != ROUNDS)
    return EXIT_FAILURE;
  printf("rounds=%d publication_failures=0 "
         "forbidden_outcome_probe=not_claimed\nPASS message_order\n",
         ROUNDS);
  return EXIT_SUCCESS;
}
