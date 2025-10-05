#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include <string.h>

// This size (128 MB) is chosen to be larger than the L3 cache
#define ARRAY_SIZE_BYTES (128 * 1024 * 1024)
#define NUM_ELEMENTS (ARRAY_SIZE_BYTES / sizeof(long))
#define NUM_RUNS 100   // You can increase to 500 for statistics
#define MAX_STRIDE 1024 // Test strides up to 1024

// A portable rdtsc wrapper to read the Time Stamp Counter on x86_64 CPUs.
static inline uint64_t rdtsc() {
    unsigned lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// Standard Fisher-Yates shuffle
void shuffle(size_t *array, size_t n) {
    if (n > 1) {
        for (size_t i = 0; i < n - 1; i++) {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            size_t t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

// Flush array from cache
void flush_cache(long *arr, size_t n) {
    for (size_t i = 0; i < n; i += 64 / sizeof(long)) {
        __asm__ volatile("clflush (%0)" :: "r"(&arr[i]));
    }
    __asm__ volatile("mfence");
}

int main() {
    // Pin the process to a single CPU core.
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity failed");
        return -1;
    }

    // Allocate memory (aligned for cache line).
    long *data_array;
    if (posix_memalign((void **)&data_array, 64, ARRAY_SIZE_BYTES)) {
        fprintf(stderr, "Memory allocation failed.\n");
        return -1;
    }
    size_t *indices = malloc(NUM_ELEMENTS * sizeof(size_t));
    if (!indices) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(data_array);
        return -1;
    }

    // Initialize arrays.
    for (size_t i = 0; i < NUM_ELEMENTS; i++) {
        data_array[i] = i;
        indices[i] = i;
    }

    // Open CSV file
    FILE *csv_file = fopen("prefetcher_data.csv", "w");
    if (!csv_file) {
        fprintf(stderr, "Could not open prefetcher_data.csv for writing.\n");
        free(data_array);
        free(indices);
        return -1;
    }
    fprintf(csv_file, "type,stride,run,cycles_per_access\n");

    srand(time(NULL));
    volatile long sum = 0;

    printf("Running %d runs for sequential, stride, and random access...\n", NUM_RUNS);

    for (int run = 0; run < NUM_RUNS; run++) {
        // Sequential Access
        flush_cache(data_array, NUM_ELEMENTS);
        uint64_t start_seq = rdtsc();
        for (size_t i = 0; i < NUM_ELEMENTS; i++) sum += data_array[i];
        uint64_t end_seq = rdtsc();
        double sequential_avg = (double)(end_seq - start_seq) / NUM_ELEMENTS;
        fprintf(csv_file, "sequential,1,%d,%.2f\n", run, sequential_avg);

        // Strided Access
        for (size_t stride = 2; stride <= MAX_STRIDE; stride *= 2) {
            flush_cache(data_array, NUM_ELEMENTS);
            uint64_t start_stride = rdtsc();
            for (size_t i = 0; i < NUM_ELEMENTS; i += stride) sum += data_array[i];
            uint64_t end_stride = rdtsc();
            double stride_avg = (double)(end_stride - start_stride) / (NUM_ELEMENTS / stride);
            fprintf(csv_file, "stride,%zu,%d,%.2f\n", stride, run, stride_avg);
        }

        // Random Access
        shuffle(indices, NUM_ELEMENTS);
        flush_cache(data_array, NUM_ELEMENTS);
        uint64_t start_rand = rdtsc();
        for (size_t i = 0; i < NUM_ELEMENTS; i++) sum += data_array[indices[i]];
        uint64_t end_rand = rdtsc();
        double random_avg = (double)(end_rand - start_rand) / NUM_ELEMENTS;
        fprintf(csv_file, "random,NA,%d,%.2f\n", run, random_avg);
    }

    fclose(csv_file);
    free(data_array);
    free(indices);

    printf("Done. Results saved to prefetcher_data.csv\n");
    return 0;
}
