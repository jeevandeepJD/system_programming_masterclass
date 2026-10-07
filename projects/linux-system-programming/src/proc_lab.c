#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void fail(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void show_first_line(const char *path, const char *label) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) fail(path);
    char line[512];
    if (fgets(line, sizeof(line), fp) == NULL) fail("fgets");
    if (fclose(fp) != 0) fail("fclose");
    line[strcspn(line, "\n")] = '\0';
    printf("%s=%s\n", label, line);
}

int main(void) {
    char path[64];
    int n = snprintf(path, sizeof(path), "/proc/%ld/status", (long)getpid());
    if (n < 0 || (size_t)n >= sizeof(path)) fail("snprintf");
    FILE *fp = fopen(path, "r");
    if (fp == NULL) fail(path);
    char line[512];
    int saw_pid = 0;
    int saw_ppid = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "Pid:", 4) == 0) {
            printf("%s", line);
            saw_pid = 1;
        } else if (strncmp(line, "PPid:", 5) == 0) {
            printf("%s", line);
            saw_ppid = 1;
        }
    }
    if (ferror(fp) != 0 || fclose(fp) != 0) fail("read status");
    if (saw_pid == 0 || saw_ppid == 0) fail("status fields");
    show_first_line("/proc/self/stat", "stat");
    char target[512];
    ssize_t len = readlink("/proc/self/exe", target, sizeof(target) - 1U);
    if (len < 0) fail("readlink");
    target[(size_t)len] = '\0';
    printf("exe=%s\nproc_inspection=ok\n", target);
    return EXIT_SUCCESS;
}
