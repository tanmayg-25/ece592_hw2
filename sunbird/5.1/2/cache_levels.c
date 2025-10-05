#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <x86intrin.h>
#include <inttypes.h>
#include <sched.h>
#include <time.h>

#define MAX_BUF (128 * 1024 * 1024)
#define MIN_BUF (4 * 1024)
#define ITERATIONS 50  // Multiple measurements for stability

// Portable CPUID
static inline void cpu_id(uint32_t op, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    __asm__ __volatile__ (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(op), "c"(0)
    );
}

static inline uint64_t rdtsc_serial() {
    uint32_t eax, ebx, ecx, edx;
    cpu_id(0, &eax, &ebx, &ecx, &edx);
    return __rdtsc();
}

static inline uint64_t rdtscp_serial(uint32_t* aux) {
    uint64_t t = __rdtscp(aux);
    uint32_t eax, ebx, ecx, edx;
    cpu_id(0, &eax, &ebx, &ecx, &edx);
    return t;
}

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

int main(void) {
    // Pin process to a single CPU core
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity failed");
        return 1;
    }

    srand(time(NULL));
    FILE *fp = fopen("cache_hierarchy_data.csv", "w");
    if (!fp) { perror("fopen failed"); return 1; }

    fprintf(fp, "working_set_size_bytes,time_per_access_cycles\n");
    printf("Running pointer-chasing benchmark...\n");

    // Sweep in powers of two
    for (size_t buf_size = MIN_BUF; buf_size <= MAX_BUF; buf_size <<= 1) {
        size_t num_elements = buf_size / sizeof(void*);

        // Allocate 64-byte aligned memory
        void** array;
        if (posix_memalign((void**)&array, 64, buf_size) != 0) {
            perror("posix_memalign failed");
            return 1;
        }

        size_t* indices = (size_t*)malloc(num_elements * sizeof(size_t));
        if (!indices) { perror("malloc failed"); free(array); return 1; }

        // Create random permutation
        for (size_t i = 0; i < num_elements; i++) indices[i] = i;
        shuffle(indices, num_elements);

        // Create circular linked list
        for (size_t i = 0; i < num_elements - 1; i++) {
            array[indices[i]] = (void*)&array[indices[i+1]];
        }
        array[indices[num_elements - 1]] = (void*)&array[indices[0]];

        // Multiple measurements
        double total_cycles = 0;
        for (int iter = 0; iter < ITERATIONS; iter++) {
            void** p = array;
            unsigned aux;

            // Ensure enough traversals
            size_t traversals = (num_elements < 100000) ? 100000 : num_elements;

            uint64_t start = rdtsc_serial();
            for (size_t i = 0; i < traversals; i++) {
                p = (void**)*p; // pointer chase
            }
            asm volatile("" : "+r" (p)); // prevent optimization
            uint64_t end = rdtscp_serial(&aux);

            total_cycles += (double)(end - start) / traversals;
        }

        double avg_cycles = total_cycles / ITERATIONS;
        printf("Size: %9zu bytes, Latency: %8.2f cycles\n", buf_size, avg_cycles);
        fprintf(fp, "%zu,%.2f\n", buf_size, avg_cycles);

        free(array);
        free(indices);
    }

    fclose(fp);
    printf("Data written to cache_hierarchy_data.csv\n");
    return 0;
}
