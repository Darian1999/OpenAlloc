/*
 * Example: Using the test harness with your custom allocator
 *
 * This file demonstrates how to integrate the test_allocator system
 * with your custom memory allocator implementation.
 */

#include "test_allocator.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * Example 1: Test with glibc malloc (reference implementation)
 */
int test_with_glibc(void) {
    // Define the interface
    allocator_interface_t glibc_iface = {
        .malloc = malloc,
        .free = free,
        .realloc = realloc,
        .calloc = calloc
    };
    
    // Set and run
    test_allocator_set_interface(&glibc_iface);
    test_allocator_run_all(12345);
    test_allocator_run_benchmark(12345);
    
    return 0;
}

/*
 * Example 2: Test with OpenAlloc
 */
int test_with_openalloc(void) {
    // Initialize OpenAlloc with a heap
    #define HEAP_SIZE (100 * 1024 * 1024)
    static unsigned char heap[HEAP_SIZE];
    openalloc_init(heap, HEAP_SIZE);
    
    // Define the interface
    allocator_interface_t openalloc_iface = {
        .malloc = openalloc_malloc,
        .free = openalloc_free,
        .realloc = openalloc_realloc,
        .calloc = NULL  // Not implemented
    };
    
    // Set and run
    test_allocator_set_interface(&openalloc_iface);
    test_allocator_run_all(12345);
    test_allocator_run_benchmark(12345);
    
    return 0;
}

/*
 * Example 3: Test with your custom allocator
 *
 * Your allocator must implement these functions:
 *   void* my_malloc(size_t size);
 *   void my_free(void* ptr);
 *   void* my_realloc(void* ptr, size_t size);  // optional
 *   void* my_calloc(size_t nmemb, size_t size);  // optional
 */
int test_with_custom_allocator(void) {
    // Define the interface
    allocator_interface_t custom_iface = {
        .malloc = my_malloc,
        .free = my_free,
        .realloc = my_realloc,
        .calloc = my_calloc
    };
    
    // Set and run
    test_allocator_set_interface(&custom_iface);
    test_allocator_run_all(54321);  // Use different seed
    test_allocator_run_benchmark(54321);
    
    return 0;
}

/*
 * Example 4: Performance comparison
 */
int compare_allocators(void) {
    printf("\n=== Comparing glibc vs OpenAlloc ===\n");
    
    // Test glibc
    printf("\n--- Testing glibc malloc ---\n");
    test_with_glibc();
    
    // Test OpenAlloc
    printf("\n--- Testing OpenAlloc ---\n");
    test_with_openalloc();
    
    return 0;
}

int main(int argc, char** argv) {
    uint32_t seed = 12345;
    
    if (argc > 1) {
        seed = (uint32_t)atoi(argv[1]);
    }
    
    // Run comparison
    compare_allocators();
    
    return 0;
}
