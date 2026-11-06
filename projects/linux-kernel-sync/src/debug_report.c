#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int report_field(const char *name) {
  FILE *file = fopen("/proc/self/status", "r");
  char line[256];
  size_t n = strlen(name);
  int found = 0;
  if (file == NULL)
    return 0;
  while (fgets(line, (int)sizeof(line), file) != NULL)
    if (strncmp(line, name, n) == 0 && line[n] == ':') {
      printf("proc_field=%s present\n", name);
      found = 1;
      break;
    }
  if (fclose(file) != 0)
    return 0;
  return found;
}
int main(void) {
  int ok = report_field("State") && report_field("Threads") &&
           report_field("voluntary_ctxt_switches") &&
           report_field("nonvoluntary_ctxt_switches");
  puts("lockdep=kernel-config-dependent\nkcsan=kernel-config-and-runtime-"
       "dependent\nrcu_stall=no_claim_from_userspace");
  if (!ok)
    return EXIT_FAILURE;
  puts("PASS debug_report");
  return EXIT_SUCCESS;
}
