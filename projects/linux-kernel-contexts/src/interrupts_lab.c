#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int readable_interrupts(void) {
    FILE *file = fopen("/proc/interrupts", "r");
    char line[1024];
    int rows = 0;
    int header = 0;

    if (file == NULL) {
        return 0;
    }
    while (rows < 4096 && fgets(line, (int)sizeof line, file) != NULL) {
        rows++;
        if (rows == 1 && strstr(line, "CPU") != NULL) {
            header = 1;
        }
    }
    if (fclose(file) != 0) {
        return 0;
    }
    return header && rows > 1;
}

int main(int argc, char **argv) {
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: %s demo\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (readable_interrupts()) {
        puts("proc_interrupts=readable");
        puts("cpu_header=yes");
        puts("irq_rows_observed=yes");
    } else {
        puts("proc_interrupts=unreadable");
        puts("cpu_header=no");
        puts("irq_rows_observed=no");
    }
    puts("counts_not_asserted=yes");
    puts("proc_read_is_not_irq_handler=yes");
    puts("kernel_source_modified=no");
    return EXIT_SUCCESS;
}
