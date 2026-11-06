#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
enum { THREADS = 4, ITERATIONS = 5000 };
typedef struct {
  atomic_uint refs;
  atomic_uint sequence;
  atomic_int x;
  atomic_int y;
} object;
static object item;
static unsigned long per_thread[THREADS];
static void timeout_handler(int sig) {
  (void)sig;
  _Exit(124);
}
static int get_unless_zero(void) {
  unsigned current = atomic_load_explicit(&item.refs, memory_order_relaxed);
  while (current != 0U)
    if (atomic_compare_exchange_weak_explicit(
            &item.refs, &current, current + 1U, memory_order_acquire,
            memory_order_relaxed))
      return 1;
  return 0;
}
static void put_ref(void) {
  (void)atomic_fetch_sub_explicit(&item.refs, 1U, memory_order_release);
}
static void *worker(void *arg) {
  size_t id = *(size_t *)arg;
  int i;
  for (i = 0; i < ITERATIONS; ++i)
    if (get_unless_zero()) {
      ++per_thread[id];
      put_ref();
    }
  return NULL;
}
int main(void) {
  struct sigaction sa;
  pthread_t threads[THREADS];
  size_t ids[THREADS], i;
  unsigned before, after;
  int x, y;
  unsigned long total = 0UL;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = timeout_handler;
  if (sigaction(SIGALRM, &sa, NULL) != 0)
    return EXIT_FAILURE;
  alarm(5U);
  atomic_init(&item.refs, 1U);
  atomic_init(&item.sequence, 0U);
  atomic_init(&item.x, 0);
  atomic_init(&item.y, 0);
  (void)atomic_fetch_add_explicit(&item.sequence, 1U, memory_order_release);
  atomic_store_explicit(&item.x, 21, memory_order_relaxed);
  atomic_store_explicit(&item.y, 42, memory_order_relaxed);
  (void)atomic_fetch_add_explicit(&item.sequence, 1U, memory_order_release);
  do {
    before = atomic_load_explicit(&item.sequence, memory_order_acquire);
    x = atomic_load_explicit(&item.x, memory_order_relaxed);
    y = atomic_load_explicit(&item.y, memory_order_relaxed);
    after = atomic_load_explicit(&item.sequence, memory_order_acquire);
  } while ((before & 1U) != 0U || before != after);
  for (i = 0U; i < THREADS; ++i) {
    ids[i] = i;
    if (pthread_create(&threads[i], NULL, worker, &ids[i]) != 0)
      return EXIT_FAILURE;
  }
  for (i = 0U; i < THREADS; ++i) {
    if (pthread_join(threads[i], NULL) != 0)
      return EXIT_FAILURE;
    total += per_thread[i];
  }
  put_ref();
  alarm(0U);
  if (x != 21 || y != 42 ||
      total != (unsigned long)THREADS * (unsigned long)ITERATIONS ||
      atomic_load_explicit(&item.refs, memory_order_relaxed) != 0U)
    return EXIT_FAILURE;
  printf("stable_snapshot=%d,%d per_thread_total=%lu terminal_refs=0\nPASS "
         "refseq_lab\n",
         x, y, total);
  return EXIT_SUCCESS;
}
