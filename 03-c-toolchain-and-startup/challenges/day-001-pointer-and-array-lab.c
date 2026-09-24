/*
 * Week 3, Day 1 — pointer and array observation lab.
 *
 * Predict each result before running this program.  All observations in the
 * default execution are within the bounds and lifetimes required by C17.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct sample {
    char tag;
    int value;
    char *name;
};

static int static_initialized = 17;
static int static_zero_initialized;
static const char read_only_text[] = "read-only text";

static void show_parameter_decay(int values[5])
{
    int *adjusted_parameter = values;

    printf("  inside callee: sizeof adjusted array parameter = %zu\n",
           sizeof adjusted_parameter);
    printf("  inside callee: values points at              %p\n",
           (void *)values);
}

static void show_pointer_sizes(void)
{
    puts("\n[1] Pointer representations");
    printf("  sizeof(char *)   = %zu\n", sizeof(char *));
    printf("  sizeof(double *) = %zu\n", sizeof(double *));
}

static void show_pointer_arithmetic(void)
{
    int integers[2] = { 10, 20 };
    char characters[2] = { 'a', 'b' };
    struct sample samples[2] = {
        { 'A', 1, NULL },
        { 'B', 2, NULL }
    };

    puts("\n[2] Pointer arithmetic strides");
    printf("  int * stride           = %td bytes\n",
           (const unsigned char *)&integers[1] -
           (const unsigned char *)&integers[0]);
    printf("  char * stride          = %td byte\n",
           (const unsigned char *)&characters[1] -
           (const unsigned char *)&characters[0]);
    printf("  struct sample * stride = %td bytes (sizeof = %zu)\n",
           (const unsigned char *)&samples[1] -
           (const unsigned char *)&samples[0],
           sizeof(struct sample));
}

static void show_index_equivalence(void)
{
    int values[] = { 10, 20, 30, 40 };

    puts("\n[3] Indexing is pointer arithmetic");
    printf("  values[2]       = %d\n", values[2]);
    printf("  *(values + 2)   = %d\n", *(values + 2));
    printf("  *(2 + values)   = %d\n", *(2 + values));
    printf("  2[values]       = %d  (valid C; do not write code this way)\n",
           2[values]);
}

static void show_array_identity(void)
{
    int values[5] = { 10, 20, 30, 40, 50 };

    puts("\n[4] One address, different types and strides");
    printf("  values     = %p; sizeof values     = %zu\n",
           (void *)values, sizeof values);
    printf("  &values[0] = %p; sizeof values[0]  = %zu\n",
           (void *)&values[0], sizeof values[0]);
    printf("  &values    = %p; sizeof &values    = %zu\n",
           (void *)&values, sizeof &values);
    printf("  values + 1 = %p  (one int later)\n", (void *)(values + 1));
    printf("  &values + 1= %p  (one whole array later)\n",
           (void *)(&values + 1));

    puts("\n[5] Array parameter adjustment");
    printf("  inside caller: sizeof values = %zu, count = %zu\n",
           sizeof values, sizeof values / sizeof values[0]);
    show_parameter_decay(values);
}

static void show_grid_layout(void)
{
    int grid[3][4];
    int next = 0;

    for (size_t row = 0; row < 3; ++row) {
        for (size_t column = 0; column < 4; ++column)
            grid[row][column] = next++;
    }

    puts("\n[6] Two-dimensional row-major layout");
    printf("  sizeof grid       = %zu\n", sizeof grid);
    printf("  sizeof grid[0]    = %zu\n", sizeof grid[0]);
    printf("  sizeof grid[0][0] = %zu\n", sizeof grid[0][0]);
    printf("  &grid[0][0]       = %p\n", (void *)&grid[0][0]);
    printf("  &grid[1][0]       = %p (row stride = %td bytes)\n",
           (void *)&grid[1][0],
           (const unsigned char *)&grid[1][0] -
           (const unsigned char *)&grid[0][0]);

    fputs("  flat walk: ", stdout);
    for (size_t row = 0; row < 3; ++row) {
        for (size_t column = 0; column < 4; ++column)
            printf("%d%s", grid[row][column],
                   row == 2 && column == 3 ? "\n" : " ");
    }
}

static void show_storage_regions(void)
{
    int automatic = 23;
    int *allocated = malloc(sizeof *allocated);
    char writable[] = "string literal";
    const char *literal = "string literal";

    puts("\n[7] Addresses from several storage regions");
    printf("  function-local stack object  %p\n", (void *)&automatic);
    printf("  heap allocation              %p\n", (void *)allocated);
    printf("  initialized static object    %p\n", (void *)&static_initialized);
    printf("  zero-initialized static      %p\n",
           (void *)&static_zero_initialized);
    printf("  named const array            %p -> \"%s\"\n",
           (const void *)read_only_text, read_only_text);
    printf("  string literal               %p -> \"%s\"\n",
           (const void *)literal, literal);
    printf("  writable stack array         %p -> \"%s\"\n",
           (void *)writable, writable);

    writable[0] = 'S';
    printf("  after writable[0] = 'S':      \"%s\"\n", writable);
    printf("  literal remains:              \"%s\"\n", literal);

    free(allocated);
}

int main(void)
{
    puts("Predict first: sizes, strides, equal addresses, and which addresses");
    puts("will change when this program is run a second time.");

    show_pointer_sizes();
    show_pointer_arithmetic();
    show_index_equivalence();
    show_array_identity();
    show_grid_layout();
    show_storage_regions();

    puts("\nNo one-past pointer was dereferenced.");
    return 0;
}
