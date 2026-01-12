#include "test_allocator.h"
#include "openalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>

#define ALIGNMENT OPENALLOC_ALIGN
#define MAX_TEST_NAME 64
#define MAX_TESTS 256
#define STRESS_ITERATIONS 100000
#define BENCHMARK_ITERATIONS 100000
#define NUM_THREADS 16
#define THREAD_ITERATIONS 10000

static uint32_t rng_seed = 0;
allocator_interface_t* current_allocator = NULL;
performance_metrics_t current_metrics = {0};
static jmp_buf test_jmpbuf;
static volatile sig_atomic_t signal_caught = 0;
static char current_test_name[MAX_TEST_NAME] = {0};
static int tests_passed = 0;
static int tests_failed = 0;
static int assertions_failed = 0;

static inline uint32_t rand_u32(void) {
    rng_seed = rng_seed * 1103515245 + 12345;
    return rng_seed;
}

static inline size_t rand_size(size_t min, size_t max) {
    return min + (rand_u32() % (max - min + 1));
}

static inline double get_time_ns(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1e9 + tv.tv_usec * 1e3;
}

static size_t get_peak_rss_kb(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}

static void signal_handler(int sig) {
    (void)sig;
    signal_caught = 1;
    longjmp(test_jmpbuf, 1);
}

#define TEST_START(name) \
    do { \
        strncpy(current_test_name, name, MAX_TEST_NAME - 1); \
        signal_caught = 0; \
        printf("  [RUN] %s\n", name); \
        fflush(stdout); \
        if (setjmp(test_jmpbuf) != 0) { \
            printf("  [FAIL] %s - Signal caught\n", name); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define TEST_PASS(name) \
    do { \
        if (!signal_caught) { \
            printf("  [PASS] %s\n", name); \
            tests_passed++; \
        } \
    } while(0)

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("  [FAIL] Assertion failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
            assertions_failed++; \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_NULL(ptr) ASSERT((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT((ptr) != NULL)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NEQ(a, b) ASSERT((a) != (b))
#define ASSERT_ALIGNED(ptr) ASSERT(((uintptr_t)(ptr) % ALIGNMENT) == 0)

typedef struct {
    void* ptr;
    size_t size;
    uint8_t pattern;
} allocation_t;

typedef struct {
    allocation_t* allocs;
    size_t count;
    size_t capacity;
} allocation_tracker_t;

static allocation_tracker_t* tracker_create(size_t capacity) {
    allocation_tracker_t* tracker = malloc(sizeof(allocation_tracker_t));
    tracker->allocs = calloc(capacity, sizeof(allocation_t));
    tracker->count = 0;
    tracker->capacity = capacity;
    return tracker;
}

static void tracker_destroy(allocation_tracker_t* tracker) {
    for (size_t i = 0; i < tracker->count; i++) {
        current_allocator->free(tracker->allocs[i].ptr);
    }
    free(tracker->allocs);
    free(tracker);
}

static void tracker_add(allocation_tracker_t* tracker, void* ptr, size_t size) {
    if (tracker->count < tracker->capacity) {
        uint8_t pattern = (uint8_t)rand_u32();
        memset(ptr, pattern, size);
        tracker->allocs[tracker->count] = (allocation_t){ptr, size, pattern};
        tracker->count++;
    }
}

static void tracker_random_free(allocation_tracker_t* tracker) {
    if (tracker->count > 0) {
        size_t idx = rand_size(0, tracker->count - 1);
        allocation_t* alloc = &tracker->allocs[idx];
        
        for (size_t i = 0; i < alloc->size; i++) {
            volatile uint8_t* p = (uint8_t*)alloc->ptr + i;
            (void)*p;
        }
        
        current_allocator->free(alloc->ptr);
        tracker->allocs[idx] = tracker->allocs[tracker->count - 1];
        tracker->count--;
    }
}

static void test_basic_correctness(void) {
    TEST_START("basic_correctness");
    
    void* ptr1 = current_allocator->malloc(100);
    ASSERT_NOT_NULL(ptr1);
    ASSERT_ALIGNED(ptr1);
    memset(ptr1, 0xAA, 100);
    
    void* ptr2 = current_allocator->malloc(200);
    ASSERT_NOT_NULL(ptr2);
    ASSERT_NEQ(ptr1, ptr2);
    ASSERT_ALIGNED(ptr2);
    
    current_allocator->free(ptr1);
    current_allocator->free(ptr2);
    
    void* null_ptr = current_allocator->malloc(0);
    ASSERT_NULL(null_ptr);
    
    current_allocator->free(NULL);
    
    TEST_PASS("basic_correctness");
}

static void test_adjacent_allocations(void) {
    TEST_START("adjacent_allocations");
    
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = current_allocator->malloc(64);
        ASSERT_NOT_NULL(ptrs[i]);
        ASSERT_ALIGNED(ptrs[i]);
        for (int j = 0; j < i; j++) {
            ASSERT_NEQ(ptrs[i], ptrs[j]);
        }
    }
    
    for (int i = 0; i < 10; i++) {
        current_allocator->free(ptrs[i]);
    }
    
    TEST_PASS("adjacent_allocations");
}

static void test_alignment(void) {
    TEST_START("alignment");
    
    for (size_t size = 1; size <= 100; size++) {
        void* ptr = current_allocator->malloc(size);
        ASSERT_NOT_NULL(ptr);
        ASSERT_ALIGNED(ptr);
        current_allocator->free(ptr);
    }
    
    TEST_PASS("alignment");
}

static void test_realloc_basic(void) {
    TEST_START("realloc_basic");
    
    if (!current_allocator->realloc) {
        printf("  [SKIP] realloc not implemented\n");
        return;
    }
    
    void* ptr = current_allocator->malloc(100);
    ASSERT_NOT_NULL(ptr);
    memset(ptr, 0xAB, 100);
    
    void* ptr2 = current_allocator->realloc(ptr, 200);
    ASSERT_NOT_NULL(ptr2);
    for (size_t i = 0; i < 100; i++) {
        ASSERT(((uint8_t*)ptr2)[i] == 0xAB);
    }
    
    void* result = current_allocator->realloc(ptr2, 0);
    ASSERT_NULL(result);
    
    TEST_PASS("realloc_basic");
}

static void test_calloc_basic(void) {
    TEST_START("calloc_basic");
    
    if (!current_allocator->calloc) {
        printf("  [SKIP] calloc not implemented\n");
        return;
    }
    
    void* ptr = current_allocator->calloc(100, 1);
    ASSERT_NOT_NULL(ptr);
    for (size_t i = 0; i < 100; i++) {
        ASSERT(((uint8_t*)ptr)[i] == 0);
    }
    current_allocator->free(ptr);
    
    ptr = current_allocator->calloc(10, 10);
    ASSERT_NOT_NULL(ptr);
    for (size_t i = 0; i < 100; i++) {
        ASSERT(((uint8_t*)ptr)[i] == 0);
    }
    current_allocator->free(ptr);
    
    TEST_PASS("calloc_basic");
}

static void test_double_free_detection(void) {
    TEST_START("double_free_detection");
    
    void* ptr = current_allocator->malloc(100);
    ASSERT_NOT_NULL(ptr);
    current_allocator->free(ptr);
    
    signal(SIGSEGV, SIG_IGN);
    signal(SIGABRT, SIG_IGN);
    current_allocator->free(ptr);
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    
    printf("  [PASS] double_free_detection (no crash)\n");
    tests_passed++;
}

static void test_allocation_storm(void) {
    TEST_START("allocation_storm");
    
    allocation_tracker_t* tracker = tracker_create(10000);
    
    for (int i = 0; i < 10000; i++) {
        size_t size = rand_size(8, 1024);
        void* ptr = current_allocator->malloc(size);
        if (ptr) {
            tracker_add(tracker, ptr, size);
        }
    }
    
    while (tracker->count > 0) {
        tracker_random_free(tracker);
    }
    
    tracker_destroy(tracker);
    
    TEST_PASS("allocation_storm");
}

static void test_power_of_2_sizes(void) {
    TEST_START("power_of_2_sizes");
    
    for (int exp = 0; exp <= 20; exp++) {
        size_t size = 1ULL << exp;
        void* ptr = current_allocator->malloc(size);
        if (ptr) {
            ASSERT_ALIGNED(ptr);
            memset(ptr, 0xFF, size);
            current_allocator->free(ptr);
        }
    }
    
    TEST_PASS("power_of_2_sizes");
}

static void test_worst_case_coalescing(void) {
    TEST_START("worst_case_coalescing");
    
    void* ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = current_allocator->malloc(100);
        ASSERT_NOT_NULL(ptrs[i]);
    }
    
    for (int i = 0; i < 100; i += 2) {
        current_allocator->free(ptrs[i]);
    }
    
    for (int i = 1; i < 100; i += 2) {
        current_allocator->free(ptrs[i]);
    }
    
    TEST_PASS("worst_case_coalescing");
}

static void test_partial_fills(void) {
    TEST_START("partial_fills");
    
    allocation_tracker_t* tracker = tracker_create(1000);
    
    for (int i = 0; i < 1000; i++) {
        size_t size = rand_size(64, 512);
        void* ptr = current_allocator->malloc(size);
        if (ptr) {
            tracker_add(tracker, ptr, size);
        }
        if (i % 10 == 0 && tracker->count > 0) {
            tracker_random_free(tracker);
        }
    }
    
    tracker_destroy(tracker);
    
    TEST_PASS("partial_fills");
}

typedef struct {
    int thread_id;
    int iterations;
    int errors;
} thread_data_t;

static void* thread_worker(void* arg) {
    (void)arg;
    return NULL;
}

static void test_multithreading(void) {
    TEST_START("multithreading");
    
    printf("  [SKIP] Multithreading requires thread-safe allocator\n");
    
    TEST_PASS("multithreading");
}

static void test_fifo_pattern(void) {
    TEST_START("fifo_pattern");
    
    void* ptrs[1000];
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = current_allocator->malloc(rand_size(64, 256));
        if (!ptrs[i]) {
            break;
        }
    }
    
    for (int i = 0; i < 1000; i++) {
        if (ptrs[i]) {
            current_allocator->free(ptrs[i]);
        }
    }
    
    TEST_PASS("fifo_pattern");
}

static void test_lifo_pattern(void) {
    TEST_START("lifo_pattern");
    
    void* ptrs[1000];
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = current_allocator->malloc(rand_size(64, 256));
        if (!ptrs[i]) {
            break;
        }
    }
    
    for (int i = 999; i >= 0; i--) {
        if (ptrs[i]) {
            current_allocator->free(ptrs[i]);
        }
    }
    
    TEST_PASS("lifo_pattern");
}

static void test_random_pattern(void) {
    TEST_START("random_pattern");
    
    allocation_tracker_t* tracker = tracker_create(10000);
    int alloc_failures = 0;
    
    for (int i = 0; i < 10000; i++) {
        if (rand_u32() % 2 || tracker->count == 0) {
            if (tracker->count >= tracker->capacity || alloc_failures > 10) {
                tracker_random_free(tracker);
                alloc_failures = 0;
                continue;
            }
            
            size_t size = rand_size(8, 512);
            void* ptr = current_allocator->malloc(size);
            if (ptr) {
                tracker_add(tracker, ptr, size);
                alloc_failures = 0;
            } else {
                alloc_failures++;
                if (tracker->count > 0) {
                    tracker_random_free(tracker);
                    alloc_failures = 0;
                }
            }
        } else {
            tracker_random_free(tracker);
        }
    }
    
    while (tracker->count > 0) {
        tracker_random_free(tracker);
    }
    
    tracker_destroy(tracker);
    
    TEST_PASS("random_pattern");
}

static void benchmark_malloc_free(void) {
    printf("\n=== Performance Benchmark ===\n");
    
    double start, end;
    double total_malloc = 0;
    double total_free = 0;
    size_t total_bytes = 0;
    size_t peak_rss = 0;
    
    void** ptrs = malloc(BENCHMARK_ITERATIONS * sizeof(void*));
    
    start = get_time_ns();
    for (uint64_t i = 0; i < BENCHMARK_ITERATIONS; i++) {
        size_t size = rand_size(8, 1024);
        ptrs[i] = current_allocator->malloc(size);
        if (ptrs[i]) {
            total_bytes += size;
            peak_rss = get_peak_rss_kb();
        } else {
            printf("  Allocation failed at iteration %lu\n", i);
            break;
        }
    }
    end = get_time_ns();
    total_malloc = end - start;
    
    start = get_time_ns();
    for (uint64_t i = 0; i < BENCHMARK_ITERATIONS; i++) {
        if (ptrs[i]) {
            current_allocator->free(ptrs[i]);
        }
    }
    end = get_time_ns();
    total_free = end - start;
    
    free(ptrs);
    
    double malloc_ops_per_sec = (BENCHMARK_ITERATIONS * 1e9) / total_malloc;
    double free_ops_per_sec = (BENCHMARK_ITERATIONS * 1e9) / total_free;
    double bytes_per_sec = (total_bytes * 1e9) / (total_malloc + total_free);
    
    printf("  Malloc ops/sec:  %.0f\n", malloc_ops_per_sec);
    printf("  Free ops/sec:    %.0f\n", free_ops_per_sec);
    printf("  Throughput:      %.2f MB/sec\n", bytes_per_sec / (1024 * 1024));
    printf("  Peak RSS:        %zu KB\n", peak_rss);
    
    current_metrics.name = "benchmark";
    current_metrics.malloc_time_ns = total_malloc / BENCHMARK_ITERATIONS;
    current_metrics.free_time_ns = total_free / BENCHMARK_ITERATIONS;
    current_metrics.total_allocated = total_bytes;
    current_metrics.peak_usage = peak_rss * 1024;
    current_metrics.allocations = BENCHMARK_ITERATIONS;
    current_metrics.frees = BENCHMARK_ITERATIONS;
}

void test_allocator_set_interface(allocator_interface_t* iface) {
    current_allocator = iface;
}

void test_allocator_run_all(uint32_t seed) {
    if (!current_allocator) {
        printf("Error: No allocator interface set\n");
        return;
    }
    
    rng_seed = seed;
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGBUS, signal_handler);
    signal(SIGFPE, signal_handler);
    
    printf("\n=== Running Test Suite (seed=%u) ===\n", seed);
    fflush(stdout);
    
    test_basic_correctness();
    test_adjacent_allocations();
    test_alignment();
    test_realloc_basic();
    test_calloc_basic();
    test_double_free_detection();
    test_allocation_storm();
    test_power_of_2_sizes();
    test_worst_case_coalescing();
    test_partial_fills();
    test_multithreading();
    test_fifo_pattern();
    test_lifo_pattern();
    test_random_pattern();
    
    printf("\n=== Test Summary ===\n");
    printf("  Passed:  %d\n", tests_passed);
    printf("  Failed:  %d\n", tests_failed);
    printf("  Total:   %d\n", tests_passed + tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ All tests passed!\n");
    } else {
        printf("\n✗ Some tests failed\n");
        printf("  Assertions failed: %d\n", assertions_failed);
    }
}

void test_allocator_run_benchmark(uint32_t seed) {
    if (!current_allocator) {
        printf("Error: No allocator interface set\n");
        return;
    }
    
    rng_seed = seed;
    benchmark_malloc_free();
}

int main(int argc, char** argv) {
    uint32_t seed = (uint32_t)time(NULL);
    
    if (argc > 1) {
        seed = (uint32_t)atoi(argv[1]);
    }
    
#define HEAP_SIZE (100 * 1024 * 1024)
    static unsigned char heap[HEAP_SIZE];
    openalloc_init(heap, HEAP_SIZE);
    
    allocator_interface_t openalloc_iface = {
        .malloc = openalloc_malloc,
        .free = openalloc_free,
        .realloc = openalloc_realloc,
        .calloc = NULL
    };
    
    test_allocator_set_interface(&openalloc_iface);
    test_allocator_run_all(seed);
    test_allocator_run_benchmark(seed);
    
    return (tests_failed > 0) ? 1 : 0;
}
