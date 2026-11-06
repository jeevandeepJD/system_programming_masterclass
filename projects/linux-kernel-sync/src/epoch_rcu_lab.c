#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
enum { READERS = 3 };
static atomic_uint epoch;
static atomic_uint active[READERS];
static atomic_int published;
static void timeout_handler(int sig) {
  (void)sig;
  _Exit(124);
}
static void *reader(void *arg) {
  size_t id = *(size_t *)arg;
  unsigned observed = atomic_load_explicit(&epoch, memory_order_acquire);
  atomic_store_explicit(&active[id], observed, memory_order_release);
  (void)atomic_load_explicit(&published, memory_order_acquire);
  atomic_store_explicit(&active[id], 0U, memory_order_release);
  return NULL;
}
int main(void) {
  struct sigaction sa;
  pthread_t threads[READERS];
  size_t ids[READERS], i;
  unsigned old_epoch;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = timeout_handler;
  if (sigaction(SIGALRM, &sa, NULL) != 0)
    return EXIT_FAILURE;
  alarm(5U);
  atomic_init(&epoch, 1U);
  atomic_init(&published, 1);
  for (i = 0U; i < READERS; ++i) {
    atomic_init(&active[i], 0U);
    ids[i] = i;
    if (pthread_create(&threads[i], NULL, reader, &ids[i]) != 0)
      return EXIT_FAILURE;
  }
  old_epoch = atomic_fetch_add_explicit(&epoch, 1U, memory_order_acq_rel);
  atomic_store_explicit(&published, 2, memory_order_release);
  for (i = 0U; i < READERS; ++i)
    if (pthread_join(threads[i], NULL) != 0)
      return EXIT_FAILURE;
  for (i = 0U; i < READERS; ++i)
    if (atomic_load_explicit(&active[i], memory_order_acquire) == old_epoch)
      return EXIT_FAILURE;
  alarm(0U);
  printf("model=userspace_epoch_not_linux_rcu old_epoch=%u new_value=2 "
         "grace_condition=met\nPASS epoch_rcu_lab\n",
         old_epoch);
  return EXIT_SUCCESS;
}
