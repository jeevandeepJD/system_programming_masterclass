/*
 * Week 3, Day 2 — structures, tagged unions, callbacks, and intrusive links.
 *
 * Build:
 *   gcc -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g \
 *     day-002-layout-callback-lab.c -o /tmp/week3-day2
 *
 * Predict layout values and callback results before selecting a mode.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct record {
    char tag;
    uint32_t count;
    uint16_t flags;
};

struct compact {
    uint32_t count;
    uint16_t flags;
    char tag;
};

uint32_t get_count(const struct record *record)
{
    return record->count;
}

static void run_layout(void)
{
    struct record records[2] = {
        { 'A', UINT32_C(100), UINT16_C(3) },
        { 'B', UINT32_C(200), UINT16_C(5) }
    };

    puts("Predict first: each offset, total size, alignment, and array stride.");
    puts("\nstruct record");
    printf("  offsetof(tag)   = %zu\n", offsetof(struct record, tag));
    printf("  offsetof(count) = %zu\n", offsetof(struct record, count));
    printf("  offsetof(flags) = %zu\n", offsetof(struct record, flags));
    printf("  sizeof          = %zu\n", sizeof(struct record));
    printf("  _Alignof        = %zu\n", _Alignof(struct record));
    printf("  array stride    = %td bytes\n",
           (const unsigned char *)&records[1] -
           (const unsigned char *)&records[0]);
    printf("  get_count       = %u\n", get_count(&records[0]));

    puts("\nstruct compact (same fields, different declaration order)");
    printf("  offsetof(count) = %zu\n", offsetof(struct compact, count));
    printf("  offsetof(flags) = %zu\n", offsetof(struct compact, flags));
    printf("  offsetof(tag)   = %zu\n", offsetof(struct compact, tag));
    printf("  sizeof          = %zu\n", sizeof(struct compact));
    printf("  _Alignof        = %zu\n", _Alignof(struct compact));

    puts("\nTODO prediction: identify every padding byte in both structures.");
    puts("Do not infer a wire or disk format from these ABI observations.");
}

enum value_kind {
    VALUE_NUMBER,
    VALUE_REAL
    /* TODO: Add a borrowed immutable string alternative. */
};

struct value {
    enum value_kind kind;
    union {
        uint32_t number;
        float real;
        /* TODO: Add `const char *text` with an explicit borrowed lifetime. */
    } as;
};

static void print_value(const struct value *value)
{
    switch (value->kind) {
    case VALUE_NUMBER:
        printf("number: %u\n", value->as.number);
        break;
    case VALUE_REAL:
        printf("real:   %.3f\n", (double)value->as.real);
        break;
    default:
        puts("invalid tag");
        break;
    }
}

static void run_union(void)
{
    struct value first = {
        .kind = VALUE_NUMBER,
        .as.number = UINT32_C(0x11223344)
    };
    struct value second = {
        .kind = VALUE_REAL,
        .as.real = 1.0f
    };
    uint32_t real_bits = 0;

    puts("Predict first: union size/alignment and which member each tag permits.");
    printf("sizeof value union = %zu, _Alignof = %zu\n",
           sizeof first.as, _Alignof(union {
               uint32_t number;
               float real;
           }));
    print_value(&first);
    print_value(&second);

    _Static_assert(sizeof real_bits == sizeof second.as.real,
                   "this observation expects 32-bit float and uint32_t");
    memcpy(&real_bits, &second.as.real, sizeof real_bits);
    printf("1.0f object representation copied with memcpy: 0x%08x\n",
           real_bits);
    puts("The tag is software policy; the union does not update it for us.");
}

typedef int (*binary_operation)(int left, int right);

struct operation {
    const char *name;
    binary_operation function;
};

static int add(int left, int right)
{
    return left + right;
}

static int multiply(int left, int right)
{
    return left * right;
}

int apply_operation(const struct operation *operation, int left, int right)
{
    return operation->function(left, right);
}

typedef void (*visitor_fn)(int value, void *context);

static void visit(const int *items, size_t count, visitor_fn visitor,
                  void *context)
{
    for (size_t index = 0; index < count; ++index)
        visitor(items[index], context);
}

struct sum_context {
    int total;
    size_t calls;
};

static void accumulate(int value, void *context)
{
    struct sum_context *sum = context;

    sum->total += value;
    ++sum->calls;
}

enum registry_result {
    REGISTRY_OK,
    REGISTRY_DUPLICATE,
    REGISTRY_FULL,
    REGISTRY_TODO
};

enum { REGISTRY_CAPACITY = 3 };

struct callback_registry {
    struct operation entries[REGISTRY_CAPACITY];
    size_t count;
};

static enum registry_result registry_add(struct callback_registry *registry,
                                         const char *name,
                                         binary_operation function)
{
    (void)registry;
    (void)name;
    (void)function;
    /*
     * TODO: Reject duplicate names, reject a full registry, then append the
     * name/function pair and increment count.  Do not call NULL callbacks.
     */
    return REGISTRY_TODO;
}

static const struct operation *
registry_find(const struct callback_registry *registry, const char *name)
{
    (void)registry;
    (void)name;
    /* TODO: Return the matching entry, or NULL for an unknown name. */
    return NULL;
}

struct list_node {
    struct list_node *previous;
    struct list_node *next;
};

struct task {
    int id;
    struct list_node link;
};

static void list_initialize(struct list_node *head)
{
    head->previous = head;
    head->next = head;
}

static void list_append(struct list_node *head, struct list_node *node)
{
    node->previous = head->previous;
    node->next = head;
    head->previous->next = node;
    head->previous = node;
}

static size_t list_traverse(const struct list_node *head)
{
    (void)head;
    /*
     * TODO: Follow next links until reaching head again.  Recover each task
     * from offsetof(struct task, link), print its id, and return the count.
     */
    return 0;
}

static void list_unlink(struct list_node *node)
{
    (void)node;
    /*
     * TODO: Join the neighboring nodes, then make node self-linked so a
     * repeated accidental unlink does not corrupt this small lab's list.
     */
}

static const char *registry_result_name(enum registry_result result)
{
    switch (result) {
    case REGISTRY_OK:
        return "ok";
    case REGISTRY_DUPLICATE:
        return "duplicate";
    case REGISTRY_FULL:
        return "full";
    case REGISTRY_TODO:
        return "TODO";
    }
    return "invalid";
}

static void run_callbacks(void)
{
    const struct operation operations[] = {
        { "add", add },
        { "multiply", multiply }
    };
    const int items[] = { 2, 3, 5, 7 };
    struct sum_context sum = { 0, 0 };
    struct callback_registry registry = { { { NULL, NULL } }, 0 };
    struct list_node head;
    struct task first = { 101, { NULL, NULL } };
    struct task second = { 202, { NULL, NULL } };
    enum registry_result status;

    puts("Predict first: operation results, callback count, and final sum.");
    printf("add(6, 7)      = %d\n",
           apply_operation(&operations[0], 6, 7));
    printf("multiply(6, 7) = %d\n",
           apply_operation(&operations[1], 6, 7));

    visit(items, sizeof items / sizeof items[0], accumulate, &sum);
    printf("visitor context: calls=%zu total=%d\n", sum.calls, sum.total);

    puts("\nRegistry TODO checks");
    status = registry_add(&registry, "add", add);
    printf("  valid registration: %s (expected ok after TODO)\n",
           registry_result_name(status));
    status = registry_add(&registry, "add", add);
    printf("  duplicate name:     %s (expected duplicate after TODO)\n",
           registry_result_name(status));
    printf("  unknown lookup:     %s\n",
           registry_find(&registry, "missing") == NULL ? "not found" :
           "unexpected match");
    puts("  TODO: fill all slots, then verify one more add returns full.");

    list_initialize(&head);
    list_append(&head, &first.link);
    list_append(&head, &second.link);
    printf("\nIntrusive traversal count: %zu (expected 2 after TODO)\n",
           list_traverse(&head));
    list_unlink(&first.link);
    printf("After unlink count:          %zu (expected 1 after TODO)\n",
           list_traverse(&head));
}

static void print_usage(const char *program)
{
    fprintf(stderr, "usage: %s layout|union|callbacks\n", program);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        print_usage(argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "layout") == 0)
        run_layout();
    else if (strcmp(argv[1], "union") == 0)
        run_union();
    else if (strcmp(argv[1], "callbacks") == 0)
        run_callbacks();
    else {
        print_usage(argv[0]);
        return 2;
    }

    return 0;
}
