#define _GNU_SOURCE
#include <elf.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <unistd.h>

static int constructor_ran;

static void normal_exit_handler(void)
{
    static const char message[] = "[atexit] runs during normal libc exit\n";
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
}

__attribute__((constructor))
static void before_main(void)
{
    constructor_ran = 1;
    static const char message[] = "[constructor] runs before main\n";
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
}

static void show_maps(void)
{
    FILE *stream = fopen("/proc/self/maps", "r");
    if (!stream) {
        fprintf(stderr, "fopen /proc/self/maps: %s\n", strerror(errno));
        return;
    }

    puts("\n[first eight /proc/self/maps entries]");
    char line[512];
    for (int count = 0; count < 8 && fgets(line, sizeof(line), stream); ++count)
        fputs(line, stdout);
    fclose(stream);
}

int main(int argc, char **argv, char **envp)
{
    if (atexit(normal_exit_handler) != 0) {
        fputs("atexit registration failed\n", stderr);
        return EXIT_FAILURE;
    }

    printf("[main] constructor_ran=%d pid=%ld\n", constructor_ran, (long)getpid());
    printf("argc=%d argv=%p envp=%p\n", argc, (void *)argv, (void *)envp);
    for (int index = 0; index < argc; ++index)
        printf("argv[%d] @ %p -> %s\n", index, (void *)argv[index], argv[index]);

    int environment_count = 0;
    while (envp[environment_count])
        ++environment_count;
    printf("environment entries=%d\n", environment_count);
    printf("auxv: AT_PAGESZ=%lu AT_ENTRY=%#lx AT_PHDR=%#lx\n",
           getauxval(AT_PAGESZ), getauxval(AT_ENTRY), getauxval(AT_PHDR));

    show_maps();
    puts("\n[main] returning 23; atexit should run before the shell observes status 23");
    fflush(stdout);
    return 23;
}
