#include "openalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEAP_SIZE (1024 * 1024)
static unsigned char heap[HEAP_SIZE];

int main(void) {
    printf("=== OpenAlloc Security Feature Tests ===\n\n");
    
    openalloc_init(heap, HEAP_SIZE);
    
    printf("Test 1: Normal allocation and free\n");
    void* ptr1 = openalloc_malloc(100);
    printf("  Allocated: %p\n", ptr1);
    openalloc_free(ptr1);
    printf("  Freed successfully\n");
    printf("  ✓ PASSED\n\n");
    
    printf("Test 2: Double-free detection (should abort)\n");
    void* ptr2 = openalloc_malloc(100);
    printf("  Allocated: %p\n", ptr2);
    openalloc_free(ptr2);
    printf("  First free succeeded\n");
    printf("  Attempting second free...\n");
    openalloc_free(ptr2);
    printf("  ✗ FAILED - Double-free not detected!\n");
    return 1;
}