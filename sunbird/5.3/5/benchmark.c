#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <sched.h>
#include <string.h>

// Array size range: 4 KB to 128 MB
#define MAX_ARRAY_SIZE (128 * 1024 * 1024)
#define MIN_ARRAY_SIZE (4 * 1024)

// Stride range: 8 bytes to 1024 bytes
#define MIN_STRIDE 8
#define MAX_STRIDE 1024

// Fixed number of accesses for consistent timing
#define NUM_ACCESSES 1000000

// Timing with serialization
static inline uint64_t rdtsc_begin(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0), "c"(0));
    return __rdtsc();
}

static inline uint64_t rdtsc_end(void) {
    uint32_t aux, eax, ebx, ecx, edx;
    uint64_t t = __rdtscp(&aux);
    __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0), "c"(0));
    return t;
}

int main(void) {
    // Pin to core 0
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity");
        return 1;
    }

    // Allocate maximum array size
    char* array = malloc(MAX_ARRAY_SIZE);
    if (!array) {
        perror("malloc");
        return 1;
    }
    
    // Initialize array to ensure pages are mapped
    memset(array, 0xAB, MAX_ARRAY_SIZE);

    FILE* csv = fopen("cache_heatmap.csv", "w");
    if (!csv) {
        perror("fopen");
        free(array);
        return 1;
    }
    
    fprintf(csv, "array_size_kb,stride_bytes,avg_cycles_per_access\n");

    printf("============================================================\n");
    printf("Cache Hierarchy Heatmap Generation\n");
    printf("============================================================\n");
    printf("Array sizes: %d KB to %d MB\n", MIN_ARRAY_SIZE/1024, MAX_ARRAY_SIZE/1024/1024);
    printf("Strides: %d to %d bytes\n", MIN_STRIDE, MAX_STRIDE);
    printf("Accesses per measurement: %d\n", NUM_ACCESSES);
    printf("============================================================\n\n");

    int test_count = 0;
    int total_tests = 0;
    
    // Count total tests for progress
    for (size_t size = MIN_ARRAY_SIZE; size <= MAX_ARRAY_SIZE; size *= 2) {
        for (size_t stride = MIN_STRIDE; stride <= MAX_STRIDE; stride *= 2) {
            total_tests++;
        }
    }

    // Iterate through array sizes (powers of 2)
    for (size_t size = MIN_ARRAY_SIZE; size <= MAX_ARRAY_SIZE; size *= 2) {
        printf("Testing array size: %6zu KB  ", size / 1024);
        
        // Iterate through stride sizes (powers of 2)
        for (size_t stride = MIN_STRIDE; stride <= MAX_STRIDE; stride *= 2) {
            test_count++;
            
            // Warm-up: touch all pages in working set
            volatile size_t dummy = 0;
            for (size_t i = 0; i < NUM_ACCESSES / 10; i++) {
                dummy += array[(i * stride) & (size - 1)];
            }

            // Measurement
            uint64_t start = rdtsc_begin();
            for (size_t i = 0; i < NUM_ACCESSES; i++) {
                array[(i * stride) & (size - 1)]++;
            }
            uint64_t end = rdtsc_end();

            // Prevent optimization
            __asm__ __volatile__("" : "+m" (array[0]));

            double avg_cycles = (double)(end - start) / NUM_ACCESSES;
            fprintf(csv, "%zu,%zu,%.3f\n", size / 1024, stride, avg_cycles);
        }
        
        printf("[%3d/%3d tests complete]\n", test_count, total_tests);
    }

    fclose(csv);
    free(array);

    printf("\n============================================================\n");
    printf("Test complete! Data saved to cache_heatmap.csv\n");
    printf("============================================================\n");
    printf("\nExpected patterns:\n");
    printf("- Low latency plateau: Data fits in L1 cache\n");
    printf("- First step up: Exceeds L1, now in L2\n");
    printf("- Second step up: Exceeds L2, now in L3\n");
    printf("- High latency: Exceeds L3, accessing RAM\n");
    printf("- Vertical bands: Stride effects on cache line utilization\n");
    printf("\nRun: python3 plot_cache_heatmap.py cache_heatmap.csv\n");

    return 0;
}