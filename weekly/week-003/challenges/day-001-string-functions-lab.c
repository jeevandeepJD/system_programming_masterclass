/*
 * Week 3, Day 1 — string primitives and explicit sanitizer experiments.
 *
 * The five TODO functions have safe starter behavior, so the default test
 * run never invokes undefined behavior.  Do not edit the tests.  The bug1,
 * bug2, and bug3 command-line modes are deliberately invalid experiments;
 * they run only when explicitly selected.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t my_strlen(const char *text)
{
    (void)text;
    /* TODO: Count characters up to, but not including, the first '\0'. */
    return 0;
}

static char *my_strchr(const char *text, int character)
{
    (void)text;
    (void)character;
    /*
     * TODO: Return a pointer to the first matching character, or NULL.
     * Remember that searching for '\0' must find the terminator.
     */
    return NULL;
}

static size_t my_strlcpy(char *destination, const char *source,
                         size_t capacity)
{
    /*
     * Safe starter behavior: produce an empty string whenever storage exists.
     * TODO: Copy at most capacity - 1 characters, terminate when capacity is
     * nonzero, and return the full source length even when truncation occurs.
     */
    if (capacity != 0)
        destination[0] = '\0';
    (void)source;
    return 0;
}

static void my_strrev(char *text)
{
    (void)text;
    /* TODO: Reverse the string in place without moving its terminator. */
}

static size_t count_occurrences(const char *text, int character)
{
    (void)text;
    (void)character;
    /* TODO: Count matches before the terminator. */
    return 0;
}

static int failures;

static void expect_size(const char *name, size_t actual, size_t expected)
{
    if (actual == expected) {
        printf("PASS  %-34s %zu\n", name, actual);
    } else {
        printf("FAIL  %-34s got %zu, expected %zu\n",
               name, actual, expected);
        ++failures;
    }
}

static void expect_string(const char *name, const char *actual,
                          const char *expected)
{
    if (strcmp(actual, expected) == 0) {
        printf("PASS  %-34s \"%s\"\n", name, actual);
    } else {
        printf("FAIL  %-34s got \"%s\", expected \"%s\"\n",
               name, actual, expected);
        ++failures;
    }
}

static void expect_pointer(const char *name, const char *actual,
                           const char *expected)
{
    if (actual == expected) {
        printf("PASS  %s\n", name);
    } else {
        printf("FAIL  %s\n", name);
        ++failures;
    }
}

static int run_tests(void)
{
    char copy[6] = { 'X', 'X', 'X', 'X', 'X', 'X' };
    char zero_capacity_guard[] = "safe";
    char reversible[] = "systems";
    const char search[] = "abc";

    puts("Complete the five TODO functions; keep these tests unchanged.\n");

    expect_size("my_strlen(\"\")", my_strlen(""), 0);
    expect_size("my_strlen(\"pointer\")", my_strlen("pointer"), 7);

    expect_pointer("my_strchr finds an ordinary character",
                   my_strchr(search, 'b'), search + 1);
    expect_pointer("my_strchr returns NULL when absent",
                   my_strchr(search, 'z'), NULL);
    expect_pointer("my_strchr finds the terminating NUL",
                   my_strchr(search, '\0'), search + 3);

    expect_size("my_strlcpy reports full source length",
                my_strlcpy(copy, "abcdefgh", sizeof copy), 8);
    expect_string("my_strlcpy truncates and terminates", copy, "abcde");
    expect_size("my_strlcpy accepts zero capacity",
                my_strlcpy(zero_capacity_guard, "changed", 0), 7);
    expect_string("zero capacity writes no bytes",
                  zero_capacity_guard, "safe");

    my_strrev(reversible);
    expect_string("my_strrev reverses in place", reversible, "smetsys");
    expect_size("count_occurrences finds repeats",
                count_occurrences("mississippi", 's'), 4);
    expect_size("count_occurrences handles no match",
                count_occurrences("mississippi", 'z'), 0);

    if (failures == 0) {
        puts("\nAll tests passed.");
        return 0;
    }

    printf("\n%d test(s) still fail; this is expected before the TODOs are done.\n",
           failures);
    return 1;
}

static void bug1_read_past_end(void)
{
    char *unterminated = malloc(5);
    if (unterminated == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    memcpy(unterminated, "abcde", 5);
    printf("bug1 length = %zu\n", strlen(unterminated));
    free(unterminated);
}

static void unsafe_copy(char *destination, const char *source)
{
    size_t index = 0;

    do {
        destination[index] = source[index];
    } while (source[index++] != '\0');
}

static void bug2_stack_overflow(void)
{
    char destination[8];
    void (*volatile copy_function)(char *, const char *) = unsafe_copy;

    copy_function(destination, "this string is too long");
    printf("bug2 copied \"%s\"\n", destination);
}

static volatile uintptr_t escaped_address;

static const char *bug3_dangling(void)
{
    char local[] = "expired stack object";

    /*
     * The integer round trip keeps this deliberately bad experiment from
     * producing a compile-time dangling-pointer diagnostic.  On this lab's
     * target, uintptr_t can represent an object pointer; ASan still tracks
     * the pointed-to stack storage and diagnoses the explicit bad access.
     */
    escaped_address = (uintptr_t)(void *)local;
    return (const char *)(uintptr_t)escaped_address;
}

static void bug3_use_after_return(void)
{
    const char *(*volatile make_dangling)(void) = bug3_dangling;
    const char *dangling = make_dangling();

    printf("bug3 text = \"%s\"\n", dangling);
}

static void print_usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [bug1|bug2|bug3]\n"
            "  no argument: run only the safe TODO test suite\n"
            "  bug modes: deliberately execute undefined behavior for ASan\n",
            program);
}

int main(int argc, char **argv)
{
    if (argc == 1)
        return run_tests();

    if (argc != 2) {
        print_usage(argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "bug1") == 0)
        bug1_read_past_end();
    else if (strcmp(argv[1], "bug2") == 0)
        bug2_stack_overflow();
    else if (strcmp(argv[1], "bug3") == 0)
        bug3_use_after_return();
    else {
        print_usage(argv[0]);
        return 2;
    }

    return 0;
}
