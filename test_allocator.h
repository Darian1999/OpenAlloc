#ifndef TEST_ALLOCATOR_H
#define TEST_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void* (*malloc)(size_t size);
    void (*free)(void* ptr);
    void* (*realloc)(void* ptr, size_t size);
    void* (*calloc)(size_t nmemb, size_t size);
} allocator_interface_t;

typedef struct {
    const char* name;
    double malloc_time_ns;
    double free_time_ns;
    double realloc_time_ns;
    size_t total_allocated;
    size_t peak_usage;
    double fragmentation_ratio;
    uint64_t allocations;
    uint64_t frees;
} performance_metrics_t;

extern allocator_interface_t* current_allocator;
extern performance_metrics_t current_metrics;

void test_allocator_set_interface(allocator_interface_t* iface);
void test_allocator_run_all(uint32_t seed);
void test_allocator_run_benchmark(uint32_t seed);

#ifdef __cplusplus
}
#endif

#endif
