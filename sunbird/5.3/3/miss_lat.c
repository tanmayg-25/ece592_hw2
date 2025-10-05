#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <x86intrin.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>

#define NUM_RUNS 1000000

// Conservative cache sizes (adjust for your CPU)
#define L1D_SIZE (48 * 1024)           // 48 KB L1 data cache
#define L2_SIZE (2 * 1024 * 1024)      // 2 MB L2 cache
#define L3_SIZE (60 * 1024 * 1024)     // 60 MB L3 cache

// Eviction buffer sizes (2x cache size for thorough eviction)
#define L1_EVICT_SIZE (L1D_SIZE * 2)
#define L2_EVICT_SIZE (L2_SIZE * 2)
#define L3_EVICT_SIZE (L3_SIZE * 2)

// Intel-recommended timing harness with serialization
static inline uint64_t start_timer(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ __volatile__ ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0), "c"(0));
    return __rdtsc();
}

static inline uint64_t end_timer(void) {
    uint32_t aux, eax, ebx, ecx, edx;
    uint64_t t = __rdtscp(&aux);
    __asm__ __volatile__ ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0), "c"(0));
    return t;
}

// Evict cache by reading through a large buffer multiple times
void evict_cache(volatile char* buf, size_t size) {
    // Read through buffer twice to ensure eviction
    for (int pass = 0; pass < 2; pass++) {
        for (size_t i = 0; i < size; i += 64) {  // 64-byte cache line
            buf[i]++;
        }
    }
}

int main(void) {
    // Pin to CPU core 0
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity failed");
        return 1;
    }

    // Allocate eviction buffers
    volatile char* evict_l1 = malloc(L1_EVICT_SIZE);
    volatile char* evict_l2 = malloc(L2_EVICT_SIZE);
    volatile char* evict_l3 = malloc(L3_EVICT_SIZE);
    
    if (!evict_l1 || !evict_l2 || !evict_l3) {
        perror("malloc failed");
        return 1;
    }
    
    // Initialize buffers
    memset((void*)evict_l1, 1, L1_EVICT_SIZE);
    memset((void*)evict_l2, 1, L2_EVICT_SIZE);
    memset((void*)evict_l3, 1, L3_EVICT_SIZE);

    // Target data
    volatile int target = 42;
    int sink;

    // Open CSV file
    FILE* fp = fopen("cache_latency_data.csv", "w");
    if (!fp) {
        perror("fopen failed");
        return 1;
    }
    fprintf(fp, "run,l1_hit,l2_hit,l3_hit,ram_access\n");

    printf("Running cache latency measurements (%d iterations)...\n", NUM_RUNS);
    printf("Pinned to CPU core 0\n\n");

    for (int i = 0; i < NUM_RUNS; i++) {
        uint64_t start, end;
        uint64_t l1_hit, l2_hit, l3_hit, ram_access;

        // ===== 1. L1 HIT =====
        // Prime: Load target into all cache levels
        sink = target;
        _mm_mfence();
        
        // Measure L1 hit
        start = start_timer();
        sink = target;
        end = end_timer();
        l1_hit = end - start;

        // ===== 2. L2 HIT (L1 miss) =====
        // Evict only L1, keep L2/L3
        evict_cache(evict_l1, L1_EVICT_SIZE);
        _mm_mfence();
        
        // Measure L2 hit (L1 miss)
        start = start_timer();
        sink = target;
        end = end_timer();
        l2_hit = end - start;

        // ===== 3. L3 HIT (L1+L2 miss) =====
        // Reload target to all levels first
        sink = target;
        _mm_mfence();
        
        // Evict L1 and L2, keep L3
        evict_cache(evict_l2, L2_EVICT_SIZE);
        _mm_mfence();
        
        // Measure L3 hit (L1+L2 miss)
        start = start_timer();
        sink = target;
        end = end_timer();
        l3_hit = end - start;

        // ===== 4. RAM ACCESS (all caches miss) =====
        // Use clflush to evict from all cache levels
        _mm_mfence();
        _mm_clflush((void*)&target);
        _mm_mfence();
        
        // Measure RAM access
        start = start_timer();
        sink = target;
        end = end_timer();
        ram_access = end - start;

        // Write to CSV
        fprintf(fp, "%d,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n", 
                i, l1_hit, l2_hit, l3_hit, ram_access);

        // Progress indicator
        if ((i + 1) % 1000 == 0) {
            printf("Progress: %d/%d runs complete\n", i + 1, NUM_RUNS);
        }
    }

    fclose(fp);
    free((void*)evict_l1);
    free((void*)evict_l2);
    free((void*)evict_l3);

    printf("\n✓ Test complete!\n");
    printf("Data saved to cache_latency_data.csv\n");
    printf("Run: python3 plot_cache_latency.py cache_latency_data.csv\n");

    // Prevent optimization
    if (sink == -1) printf("%d", sink);

    return 0;
}