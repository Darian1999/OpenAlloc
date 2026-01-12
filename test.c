#include "openalloc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define HEAP_SIZE (1024 * 1024)
static unsigned char heap[HEAP_SIZE];

static void test_init(void) {
    int result = openalloc_init(heap, HEAP_SIZE);
    assert(result == 0);
    
    openalloc_stats_t stats;
    openalloc_get_stats(&stats);
    assert(stats.heap_size == HEAP_SIZE);
    assert(stats.free_blocks == 1);
    assert(stats.allocated_blocks == 0);
    
    printf("✓ Init test passed\n");
}

static void test_basic_malloc_free(void) {
    openalloc_init(heap, HEAP_SIZE);
    
    void* ptr1 = openalloc_malloc(100);
    assert(ptr1 != NULL);
    
    void* ptr2 = openalloc_malloc(200);
    assert(ptr2 != NULL);
    assert(ptr2 != ptr1);
    
    openalloc_free(ptr1);
    openalloc_free(ptr2);
    
    printf("✓ Basic malloc/free test passed\n");
}

static void test_null_pointer(void) {
    openalloc_init(heap, HEAP_SIZE);
    
    void* ptr = openalloc_malloc(0);
    assert(ptr == NULL);
    
    openalloc_free(NULL);
    
    printf("✓ Null pointer test passed\n");
}

static void test_alignment(void) {
    openalloc_init(heap, HEAP_SIZE);
    
    for (size_t size = 1; size <= 100; size++) {
        void* ptr = openalloc_malloc(size);
        assert(ptr != NULL);
        assert((uintptr_t)ptr % OPENALLOC_ALIGN == 0);
        openalloc_free(ptr);
    }
    
    printf("✓ Alignment test passed\n");
}

static void test_realloc(void) {
    openalloc_init(heap, HEAP_SIZE);
    
    void* ptr = openalloc_malloc(100);
    assert(ptr != NULL);
    
    void* ptr2 = openalloc_realloc(ptr, 200);
    assert(ptr2 != NULL);
    
    void* result = openalloc_realloc(ptr2, 0);
    assert(result == NULL);
    
    void* ptr3 = openalloc_realloc(NULL, 100);
    assert(ptr3 != NULL);
    openalloc_free(ptr3);
    
    printf("✓ Realloc test passed\n");
}

static void test_coalescing(void) {
#ifdef OPENALLOC_NO_SEG
    openalloc_init(heap, HEAP_SIZE);
    
    void* ptr1 = openalloc_malloc(100);
    void* ptr2 = openalloc_malloc(100);
    void* ptr3 = openalloc_malloc(100);
    
    openalloc_free(ptr1);
    openalloc_free(ptr3);
    openalloc_free(ptr2);
    
    openalloc_stats_t stats;
    openalloc_get_stats(&stats);
    assert(stats.free_blocks == 1);
    
    printf("✓ Coalescing test passed\n");
#else
    printf("✓ Coalescing test skipped (segregated allocator)\n");
#endif
}

static void test_fragmentation(void) {
    openalloc_init(heap, HEAP_SIZE);
    
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = openalloc_malloc(100);
        assert(ptrs[i] != NULL);
    }
    
    for (int i = 0; i < 10; i += 2) {
        openalloc_free(ptrs[i]);
    }
    
    void* ptr = openalloc_malloc(500);
    assert(ptr != NULL);
    
    printf("✓ Fragmentation test passed\n");
}

static void test_usable_size(void) {
    openalloc_init(heap, HEAP_SIZE);
    
    void* ptr = openalloc_malloc(100);
    size_t usable = openalloc_usable_size(ptr);
    assert(usable >= 100);
    openalloc_free(ptr);
    
    printf("✓ Usable size test passed\n");
}

static void test_large_allocations(void) {
    openalloc_init(heap, HEAP_SIZE);
    
    void* ptr = openalloc_malloc(HEAP_SIZE / 2);
    assert(ptr != NULL);
    
    void* ptr2 = openalloc_malloc(HEAP_SIZE / 4);
    assert(ptr2 != NULL);
    
    openalloc_free(ptr);
    openalloc_free(ptr2);
    
    printf("✓ Large allocations test passed\n");
}

static void test_stress(void) {
    openalloc_init(heap, HEAP_SIZE);
    
    void* ptrs[100];
    int count = 0;
    
    for (int i = 0; i < 1000; i++) {
        if (count < 100) {
            size_t size = (i % 10 + 1) * 50;
            void* ptr = openalloc_malloc(size);
            if (ptr) {
                ptrs[count++] = ptr;
            }
        } else if (count > 0) {
            int idx = i % count;
            openalloc_free(ptrs[idx]);
            ptrs[idx] = ptrs[--count];
        }
    }
    
    for (int i = 0; i < count; i++) {
        openalloc_free(ptrs[i]);
    }
    
    printf("✓ Stress test passed\n");
}

static void test_oom(void) {
    openalloc_init(heap, HEAP_SIZE);
    
    void* ptr = openalloc_malloc(HEAP_SIZE);
    assert(ptr == NULL);
    
    printf("✓ OOM test passed\n");
}

int main(void) {
    printf("Running Openalloc tests...\n\n");
    
    test_init();
    test_basic_malloc_free();
    test_null_pointer();
    test_alignment();
    test_realloc();
    test_coalescing();
    test_fragmentation();
    test_usable_size();
    test_large_allocations();
    test_stress();
    test_oom();
    
    printf("\n✓ All tests passed!\n");
    return 0;
}
