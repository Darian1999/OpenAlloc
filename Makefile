CC = gcc
CFLAGS = -Wall -Wextra -O3 -march=native -std=c11 -D_POSIX_C_SOURCE=199309L
LDFLAGS = -lpthread
NO_SEG_FLAG :=

# Check for --no-seg in command line
ifneq ($(filter --no-seg,$(MAKECMDGOALS)),)
    CFLAGS += -DOPENALLOC_NO_SEG
    NO_SEG_FLAG = (no-seg)
endif

SRCDIR = .
SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:.c=.o)
TEST_OBJS = test.o openalloc.o
BENCH_OBJS = benchmark.o openalloc.o
COMPARE_OBJS = compare_benchmark.o openalloc.o

.PHONY: all clean test benchmark compare no-seg help

all: test benchmark

no-seg:
	$(MAKE) CFLAGS="-DOPENALLOC_NO_SEG" all

test: $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

benchmark: $(BENCH_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

compare: $(COMPARE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) test benchmark compare

run-test: test
	./test

run-benchmark: benchmark
	./benchmark

run-compare: compare
	./compare

help:
	@echo "OpenAlloc Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build test and benchmark (default, with segregated bins)"
	@echo "  test      - Build test executable"
	@echo "  benchmark - Build benchmark executable"
	@echo "  compare   - Build comparison benchmark (vs glibc)"
	@echo "  no-seg    - Build without segregated free list (slower)"
	@echo "  clean     - Remove build artifacts"
	@echo "  run-test  - Build and run tests"
	@echo "  run-benchmark - Build and run benchmark"
	@echo "  run-compare - Build and run comparison benchmark"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make               # Build with segregated bins (fast)"
	@echo "  make no-seg       # Build without segregated bins (original)"
	@echo "  make run-test      # Run tests"
	@echo "  make run-compare   # Compare to glibc malloc"

.SUFFIXES: .c .o
