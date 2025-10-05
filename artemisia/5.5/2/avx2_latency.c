#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <sched.h>

#define ITERATIONS 1000000   // iterations per run
#define NUM_RUNS 500         // number of runs for averaging
#define DISCARD 15           // discard first few runs (warmup)
#define CHAIN_LENGTH 10      // dependent ops per loop

// --- Timing Harness ---
static inline uint64_t start_timer(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ __volatile__("cpuid"
                         : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                         : "a"(0), "c"(0));
    return __rdtsc();
}
static inline uint64_t end_timer(void) {
    uint32_t aux, eax, ebx, ecx, edx;
    uint64_t t = __rdtscp(&aux);
    __asm__ __volatile__("cpuid"
                         : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                         : "a"(0), "c"(0));
    return t;
}

double measure_empty_loop() {
    uint64_t start = start_timer();
    for (int i = 0; i < ITERATIONS; i++) {
        __asm__ volatile("" ::: "ymm0"); // just loop overhead
    }
    uint64_t end = end_timer();
    return (double)(end - start);
}

double measure_latency_chain() {
    // Initialize ymm1 with nonzero constant (avoid zero-idiom)
    __m256i init = _mm256_set1_epi32(0xdeadbeef);
    __asm__ volatile("vmovdqa %0, %%ymm1" :: "m"(init));

    uint64_t start = start_timer();
    for (int i = 0; i < ITERATIONS; i++) {
        __asm__ volatile(
            "vpxor %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpxor %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpxor %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpxor %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpxor %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpxor %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpxor %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpxor %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpxor %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpxor %%ymm1, %%ymm0, %%ymm0\n\t"
            ::: "ymm0");
    }
    uint64_t end = end_timer();
    return (double)(end - start);
}

int main() {
    // Pin process to core 0
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity failed");
        return 1;
    }

    double results[NUM_RUNS];

    FILE *csv = fopen("avx2_vpxor_latency.csv", "w");
    if (!csv) { perror("fopen failed"); return 1; }
    fprintf(csv, "run,latency_cycles\n");

    printf("Measuring AVX2 VPXOR true latency (%d dependent ops per loop)...\n",
           CHAIN_LENGTH);

    for (int run = 0; run < NUM_RUNS; run++) {
        double empty_cycles = measure_empty_loop();
        double chain_cycles = measure_latency_chain();

        double total_ops = (double)ITERATIONS * CHAIN_LENGTH;
        double latency = (chain_cycles - empty_cycles) / total_ops;

        results[run] = latency;

        // Print + CSV
        printf("Run %3d: %.3f cycles/op\n", run, latency);
        fprintf(csv, "%d,%.6f\n", run, latency);
    }

    fclose(csv);

    // Average (excluding warmup)
    double sum = 0.0;
    for (int i = DISCARD; i < NUM_RUNS; i++) sum += results[i];
    double avg = sum / (NUM_RUNS - DISCARD);

    printf("\n--- FINAL SUMMARY ---\n");
    printf("Average Latency (excluding first %d runs): %.3f cycles/op\n",
           DISCARD, avg);
    printf("CSV saved: avx2_vpxor_latency.csv\n");
    printf("---------------------\n");

    return 0;
}
