#include "openalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define HEAP_SIZE (1024 * 1024)
static unsigned char heap[HEAP_SIZE];

static double get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void benchmark_small_allocations(void) {
    printf("Benchmark: Small allocations (16 bytes)...\n");
    
    const int iterations = 100000;
    void* ptrs[1000];
    
    double start = get_time_seconds();
    
    for (int i = 0; i < iterations; i++) {
        ptrs[i % 1000] = openalloc_malloc(16);
    }
    
    double end = get_time_seconds();
    
    for (int i = 0; i < 1000; i++) {
        openalloc_free(ptrs[i]);
    }
    
    printf("  %.2f ns per allocation\n", (end - start) * 1e9 / iterations);
}

static void benchmark_medium_allocations(void) {
    printf("Benchmark: Medium allocations (1KB)...\n");
    
    const int iterations = 10000;
    void* ptrs[100];
    
    double start = get_time_seconds();
    
    for (int i = 0; i < iterations; i++) {
        ptrs[i % 100] = openalloc_malloc(1024);
    }
    
    double end = get_time_seconds();
    
    for (int i = 0; i < 100; i++) {
        openalloc_free(ptrs[i]);
    }
    
    printf("  %.2f ns per allocation\n", (end - start) * 1e9 / iterations);
}

static void benchmark_free(void) {
    printf("Benchmark: Free operations...\n");
    
    const int iterations = 100000;
    void* ptrs[iterations];
    
    for (int i = 0; i < iterations; i++) {
        ptrs[i] = openalloc_malloc(100);
    }
    
    double start = get_time_seconds();
    
    for (int i = 0; i < iterations; i++) {
        openalloc_free(ptrs[i]);
    }
    
    double end = get_time_seconds();
    
    printf("  %.2f ns per free\n", (end - start) * 1e9 / iterations);
}

static void benchmark_mixed(void) {
    printf("Benchmark: Mixed alloc/free pattern...\n");
    
    const int iterations = 50000;
    void* ptrs[500];
    int count = 0;
    
    double start = get_time_seconds();
    
    for (int i = 0; i < iterations; i++) {
        if (i % 2 == 0 || count == 0) {
            if (count < 500) {
                ptrs[count++] = openalloc_malloc((i % 10 + 1) * 100);
            }
        } else {
            if (count > 0) {
                openalloc_free(ptrs[--count]);
            }
        }
    }
    
    for (int i = 0; i < count; i++) {
        openalloc_free(ptrs[i]);
    }
    
    double end = get_time_seconds();
    
    printf("  %.2f ns per operation\n", (end - start) * 1e9 / iterations);
}

static void benchmark_fragmentation(void) {
    printf("Benchmark: Fragmentation resistance...\n");
    
    void* ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = openalloc_malloc(100);
    }
    
    for (int i = 0; i < 50; i++) {
        openalloc_free(ptrs[i * 2]);
    }
    
    const int iterations = 1000;
    double start = get_time_seconds();
    
    for (int i = 0; i < iterations; i++) {
        void* ptr = openalloc_malloc(500);
        openalloc_free(ptr);
    }
    
    double end = get_time_seconds();
    
    for (int i = 0; i < 50; i++) {
        if (ptrs[i * 2]) {
            openalloc_free(ptrs[i * 2 + 1]);
        }
    }
    
    printf("  %.2f ns per alloc/free\n", (end - start) * 1e9 / iterations);
}

int main(void) {
    printf("Openalloc Benchmark Suite\n");
    printf("==========================\n\n");
    
    openalloc_init(heap, HEAP_SIZE);
    
    benchmark_small_allocations();
    printf("\n");
    
    openalloc_init(heap, HEAP_SIZE);
    benchmark_medium_allocations();
    printf("\n");
    
    openalloc_init(heap, HEAP_SIZE);
    benchmark_free();
    printf("\n");
    
    openalloc_init(heap, HEAP_SIZE);
    benchmark_mixed();
    printf("\n");
    
    openalloc_init(heap, HEAP_SIZE);
    benchmark_fragmentation();
    printf("\n");
    
    openalloc_stats_t stats;
    openalloc_get_stats(&stats);
    printf("Final Stats:\n");
    printf("  Heap size: %zu bytes\n", stats.heap_size);
    printf("  Free blocks: %zu\n", stats.free_blocks);
    printf("  Allocated blocks: %zu\n", stats.allocated_blocks);
    
    return 0;
}
