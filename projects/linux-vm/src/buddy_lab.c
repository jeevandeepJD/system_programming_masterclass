#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ORDER 4
#define MAX_PAGES (1 << MAX_ORDER)

enum { ZONE_DMA = 0, ZONE_NORMAL = 1, ZONE_COUNT = 2 };
enum { NODE_COUNT = 2 };
enum { GFP_KERNEL = 0, GFP_DMA = 1 };

struct zone {
    int pages;
    int max_order;
    int free_count[MAX_ORDER + 1];
    int free_start[MAX_ORDER + 1][MAX_PAGES];
};

struct machine {
    struct zone zones[ZONE_COUNT];
};

static struct machine nodes[NODE_COUNT];

static void die(const char *what) {
    fprintf(stderr, "buddy_lab: %s\n", what);
    exit(EXIT_FAILURE);
}

static void zone_init(struct zone *zone, int pages) {
    memset(zone, 0, sizeof(*zone));
    zone->pages = pages;
    zone->max_order = 0;
    int remaining = pages;
    while (remaining > 1) {
        remaining >>= 1;
        zone->max_order++;
    }
    if ((1 << zone->max_order) != pages || zone->max_order > MAX_ORDER) {
        die("zone size");
    }
    zone->free_start[zone->max_order][0] = 0;
    zone->free_count[zone->max_order] = 1;
}

static int take_block(struct zone *zone, int order, int *start_out) {
    int found = -1;
    for (int candidate = order; candidate <= zone->max_order; candidate++) {
        if (zone->free_count[candidate] > 0) {
            found = candidate;
            break;
        }
    }
    if (found < 0) {
        return 0;
    }
    int start = zone->free_start[found][zone->free_count[found] - 1];
    zone->free_count[found]--;
    while (found > order) {
        found--;
        int half = 1 << found;
        zone->free_start[found][zone->free_count[found]] = start + half;
        zone->free_count[found]++;
        puts("buddy_split=yes");
    }
    *start_out = start;
    return 1;
}

static int free_block(struct zone *zone, int start, int order) {
    int original = order;
    while (order < zone->max_order) {
        int buddy = start ^ (1 << order);
        int index = -1;
        for (int slot = 0; slot < zone->free_count[order]; slot++) {
            if (zone->free_start[order][slot] == buddy) {
                index = slot;
                break;
            }
        }
        if (index < 0) {
            break;
        }
        int last = zone->free_count[order] - 1;
        zone->free_start[order][index] = zone->free_start[order][last];
        zone->free_count[order] = last;
        if (buddy < start) {
            start = buddy;
        }
        order++;
    }
    zone->free_start[order][zone->free_count[order]] = start;
    zone->free_count[order]++;
    if (order > original) {
        puts("buddy_coalesce=yes");
    }
    return order;
}

static int zone_is_whole(const struct zone *zone) {
    if (zone->free_count[zone->max_order] != 1) {
        return 0;
    }
    if (zone->free_start[zone->max_order][0] != 0) {
        return 0;
    }
    for (int order = 0; order < zone->max_order; order++) {
        if (zone->free_count[order] != 0) {
            return 0;
        }
    }
    return 1;
}

static int alloc_pages(int prefer_node, int gfp, int order, int *node_out,
                       int *zone_out, int *start_out) {
    int preferred_zone = gfp == GFP_DMA ? ZONE_DMA : ZONE_NORMAL;
    int try_node[2];
    try_node[0] = prefer_node;
    try_node[1] = prefer_node == 0 ? 1 : 0;
    for (int step = 0; step < 2; step++) {
        int node = try_node[step];
        struct zone *zone = &nodes[node].zones[preferred_zone];
        if (order > zone->max_order) {
            continue;
        }
        if (!take_block(zone, order, start_out)) {
            continue;
        }
        *node_out = node;
        *zone_out = preferred_zone;
        if (node == prefer_node) {
            puts("numa_local_hit=yes");
        } else {
            puts("numa_fallback=yes");
        }
        return 1;
    }
    return 0;
}

static void mode_demo(void) {
    for (int node = 0; node < NODE_COUNT; node++) {
        zone_init(&nodes[node].zones[ZONE_DMA], 8);
        zone_init(&nodes[node].zones[ZONE_NORMAL], 16);
    }
    puts("policy=teaching_buddy_not_kernel_fallback");
    puts("simulator_only=yes");
    puts("host_pages_untouched=yes");

    int node = -1;
    int zone = -1;
    int kernel_a = -1;
    int kernel_b = -1;
    int kernel_c = -1;
    int dma_a = -1;
    int dma_remote = -1;
    int fallback = -1;

    if (!alloc_pages(0, GFP_KERNEL, 2, &node, &zone, &kernel_a) || node != 0 ||
        zone != ZONE_NORMAL) {
        die("kernel alloc a");
    }
    if (!alloc_pages(0, GFP_KERNEL, 2, &node, &zone, &kernel_b) || node != 0 ||
        zone != ZONE_NORMAL) {
        die("kernel alloc b");
    }
    if (!alloc_pages(0, GFP_KERNEL, 3, &node, &zone, &kernel_c) || node != 0 ||
        zone != ZONE_NORMAL) {
        die("kernel alloc c");
    }
    if (!alloc_pages(0, GFP_DMA, 1, &node, &zone, &dma_a) || node != 0 ||
        zone != ZONE_DMA) {
        die("dma alloc");
    }
    puts("dma_request_uses_dma=yes");
    puts("kernel_request_skips_dma=yes");

    if (!alloc_pages(1, GFP_DMA, 3, &node, &zone, &dma_remote) || node != 1 ||
        zone != ZONE_DMA) {
        die("remote dma alloc");
    }
    if (!alloc_pages(0, GFP_KERNEL, 0, &node, &zone, &fallback) || node != 1 ||
        zone != ZONE_NORMAL) {
        die("fallback alloc");
    }
    if (alloc_pages(0, GFP_DMA, 3, &node, &zone, &dma_a)) {
        die("exhausted dma alloc succeeded");
    }
    puts("exhausted_alloc=fail");

    if (free_block(&nodes[0].zones[ZONE_NORMAL], kernel_a, 2) < 2) {
        die("free a");
    }
    if (free_block(&nodes[0].zones[ZONE_NORMAL], kernel_b, 2) != 3) {
        die("free b");
    }
    if (free_block(&nodes[0].zones[ZONE_NORMAL], kernel_c, 3) != 4) {
        die("free c");
    }
    if (free_block(&nodes[1].zones[ZONE_NORMAL], fallback, 0) != 4) {
        die("free fallback");
    }
    if (free_block(&nodes[1].zones[ZONE_DMA], dma_remote, 3) != 3) {
        die("free remote dma");
    }
    if (free_block(&nodes[0].zones[ZONE_DMA], dma_a, 1) != 3) {
        die("free dma");
    }

    for (int check_node = 0; check_node < NODE_COUNT; check_node++) {
        if (!zone_is_whole(&nodes[check_node].zones[ZONE_DMA]) ||
            !zone_is_whole(&nodes[check_node].zones[ZONE_NORMAL])) {
            die("unbalanced");
        }
    }
    puts("simulator_balanced=yes");
}

int main(int argc, char **argv) {
    if (argc != 2 || strcmp(argv[1], "demo") != 0) {
        fprintf(stderr, "usage: buddy_lab demo\n");
        return EXIT_FAILURE;
    }
    mode_demo();
    return EXIT_SUCCESS;
}
