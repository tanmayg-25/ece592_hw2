#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <sched.h>

#define ARRAY_SIZE (128 * 1024 * 1024)
#define NUM_ACCESSES 1000000 // A good number for a single run
#define MAX_STRIDE 65536
#define NUM_RUNS 50 // Collect 50 raw data points per stride

// --- Timing Harness ---
static inline uint64_t rdtscp_serial(uint32_t* aux) {
    uint64_t t = __rdtscp(aux);
    uint32_t eax, ebx, ecx, edx;
    __asm__ __volatile__ ( "cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0), "c"(0) );
    return t;
}

int main() {
    // Pin process to a single CPU core
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity failed"); return 1;
    }

    char* array = (char*)malloc(ARRAY_SIZE);
    if (!array) { perror("malloc failed"); return 1; }

    FILE* csv = fopen("cache_line_raw_data.csv", "w");
    if (!csv) { perror("fopen failed"); free(array); return 1; }
    fprintf(csv, "stride_bytes,run,avg_cycles_per_access\n");

    printf("Running cache line size benchmark with %d runs per stride...\n", NUM_RUNS);

    for (size_t stride = 1; stride <= MAX_STRIDE; stride *= 2) {
        printf("  Testing stride: %zu bytes\n", stride);
        for (int run = 0; run < NUM_RUNS; ++run) {
            // "Warm up" to ensure pages are mapped
            for (size_t i = 0; i < NUM_ACCESSES; i++) {
                array[(i * stride) & (ARRAY_SIZE - 1)]++;
            }

            uint32_t aux;
            uint64_t start = rdtscp_serial(&aux);
            for (size_t i = 0; i < NUM_ACCESSES; i++) {
                array[(i * stride) & (ARRAY_SIZE - 1)]++;
            }
            uint64_t end = rdtscp_serial(&aux);

            asm volatile("" : "+m" (array[0])); // Prevent loop optimization

            double avg_cycles = (double)(end - start) / NUM_ACCESSES;
            fprintf(csv, "%zu,%d,%.2f\n", stride, run, avg_cycles);
        }
    }

    fclose(csv);
    free(array);
    printf("\nRaw data saved to cache_line_raw_data.csv\n");
    return 0;
}
