#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <sched.h>
#include <inttypes.h>
#include <string.h>

#define NUM_RUNS 5000
// 128MB buffer larger than L3 cache
#define L3_EVICT_BUFFER_SIZE (128 * 1024 * 1024)

// Intel-recommended timing harness
static inline uint64_t start_timer(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ __volatile__ ( "cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0), "c"(0) );
    return __rdtsc();
}

static inline uint64_t end_timer(void) {
    uint32_t aux, eax, ebx, ecx, edx;
    uint64_t t = __rdtscp(&aux);
    __asm__ __volatile__ ( "cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0), "c"(0) );
    return t;
}

// Thrash L3 cache by accessing large buffer
void thrash_l3(volatile char* buf) {
    for (size_t i = 0; i < L3_EVICT_BUFFER_SIZE; i += 64) {
        buf[i]++;
    }
}

int main() {
    // Pin to single CPU core
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity failed");
        return 1;
    }
    
    // Allocate eviction buffer
    volatile char* evict_buffer = malloc(L3_EVICT_BUFFER_SIZE);
    if (!evict_buffer) {
        perror("malloc failed");
        return 1;
    }
    memset((void*)evict_buffer, 1, L3_EVICT_BUFFER_SIZE);

    FILE* csv = fopen("inclusivity_data.csv", "w");
    if (!csv) {
        perror("Failed to open CSV file");
        free((void*)evict_buffer);
        return 1;
    }
    fprintf(csv, "run,initial_hit_time,probe_after_evict_time\n");

    volatile int target = 42;
    int sink;

    printf("Running LLC inclusivity test (%d iterations)...\n", NUM_RUNS);
    printf("This will take a few seconds...\n\n");
    
    for (int i = 0; i < NUM_RUNS; ++i) {
        // 1. PRIME: Load target into L1 and measure hit time
        sink = target;  // Warm up
        uint64_t start = start_timer();
        sink = target;
        uint64_t end = end_timer();
        uint64_t initial_hit = end - start;

        // 2. EVICT: Thrash L3 cache
        thrash_l3(evict_buffer);

        // 3. PROBE: Access target again and measure
        start = start_timer();
        sink = target;
        end = end_timer();
        uint64_t probed_time = end - start;
        
        fprintf(csv, "%d,%" PRIu64 ",%" PRIu64 "\n", i, initial_hit, probed_time);
        
        // Progress indicator
        if ((i + 1) % 100 == 0) {
            printf("Progress: %d/%d runs complete\n", i + 1, NUM_RUNS);
        }
    }

    fclose(csv);
    free((void*)evict_buffer);
    
    printf("\n Test complete!\n");
    printf("Data saved to inclusivity_data.csv\n");

    // Prevent optimization of sink
    if (sink == -1) printf("%d", sink);
    
    return 0;
}