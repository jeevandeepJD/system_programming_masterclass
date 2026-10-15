#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

static int field_present(const char *path, const char *prefix) {
    FILE *file = fopen(path, "r");
    char line[512];
    size_t length = strlen(prefix);
    int found = 0;

    if (file == NULL) {
        return 0;
    }
    while (!found && fgets(line, (int)sizeof line, file) != NULL) {
        if (strncmp(line, prefix, length) == 0) {
            found = 1;
        }
    }
    if (fclose(file) != 0) {
        return 0;
    }
    return found;
}

static int interrupts_readable(void) {
    FILE *file = fopen("/proc/interrupts", "r");
    char line[256];
    int header = 0;

    if (file == NULL) {
        return 0;
    }
    if (fgets(line, (int)sizeof line, file) != NULL && strstr(line, "CPU") != NULL) {
        header = 1;
    }
    if (fclose(file) != 0) {
        return 0;
    }
    return header;
}

int main(int argc, char **argv) {
    struct utsname host;

    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: %s demo\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (uname(&host) != 0) {
        perror("uname");
        return EXIT_FAILURE;
    }

    printf("sysname=%s\n", host.sysname);
    printf("release=%s\n", host.release);
    puts("host_observed=yes");

    if (interrupts_readable()) {
        puts("proc_interrupts=readable");
    } else {
        puts("proc_interrupts=unreadable");
    }
    if (field_present("/proc/self/status", "voluntary_ctxt_switches:")) {
        puts("context_switch_field=present");
    } else {
        puts("context_switch_field=absent");
    }

    puts("tracefs_written=no");
    puts("modules_loaded=no");
    puts("network_used=no");
    puts("kernel_source_modified=no");
    puts("scheduler_dive=deferred_to_week_15");
    puts("mastery_claim=none");
    return EXIT_SUCCESS;
}
