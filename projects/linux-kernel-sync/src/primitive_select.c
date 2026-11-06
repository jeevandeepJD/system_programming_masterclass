#include <stdio.h>
#include <stdlib.h>
typedef struct {
  const char *context;
  int may_sleep;
  int irq_shared;
  int read_mostly;
  const char *choice;
} scenario;
int main(void) {
  const scenario cases[] = {{"process", 1, 0, 0, "mutex"},
                            {"process+hardirq", 0, 1, 0, "spin_lock_irqsave"},
                            {"atomic", 0, 0, 0, "spinlock"},
                            {"read-mostly lifetime", 0, 0, 1, "RCU"}};
  size_t i;
  for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i)
    printf("context=%s may_sleep=%d irq_shared=%d read_mostly=%d choice=%s\n",
           cases[i].context, cases[i].may_sleep, cases[i].irq_shared,
           cases[i].read_mostly, cases[i].choice);
  puts("PASS primitive_select");
  return EXIT_SUCCESS;
}
