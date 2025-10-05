#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <sched.h>

#define ITERATIONS 1000000
#define NUM_TESTS 10
#define UNROLL 100   // number of asm unrolls per loop
#define OPS_PER_LOOP 10

// stringify helper
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

static inline void cpu_id(uint32_t op, uint32_t* eax, uint32_t* ebx,
                          uint32_t* ecx, uint32_t* edx) {
    __asm__ __volatile__("cpuid"
                         : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                         : "a"(op), "c"(0));
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

int main() {
    // Pin process to core 0
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity failed");
        return 1;
    }

    uint64_t start, end, total_cycles = 0;
    uint32_t aux;
    
    printf("Measuring AVX2 vpxor throughput...\n");

    for (int t = 0; t < NUM_TESTS; t++) {
        start = rdtsc_serial();
        for (int i = 0; i < ITERATIONS; i++) {
            __asm__ volatile(
                ".rept " STR(UNROLL) "\n\t"
                "vpxor %%ymm1, %%ymm0, %%ymm0\n\t"
                "vpxor %%ymm2, %%ymm1, %%ymm1\n\t"
                "vpxor %%ymm3, %%ymm2, %%ymm2\n\t"
                "vpxor %%ymm4, %%ymm3, %%ymm3\n\t"
                "vpxor %%ymm5, %%ymm4, %%ymm4\n\t"
                "vpxor %%ymm6, %%ymm5, %%ymm5\n\t"
                "vpxor %%ymm7, %%ymm6, %%ymm6\n\t"
                "vpxor %%ymm8, %%ymm7, %%ymm7\n\t"
                "vpxor %%ymm9, %%ymm8, %%ymm8\n\t"
                "vpxor %%ymm0, %%ymm9, %%ymm9\n\t"
                ".endr\n\t"
                :
                :
                : "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7", "ymm8", "ymm9"
            );
        }
        end = rdtscp_serial(&aux);
        total_cycles += (end - start);
    }

    double total_insts = (double)NUM_TESTS * ITERATIONS * UNROLL * OPS_PER_LOOP;
    double cpi = (double)total_cycles / total_insts;
    double ipc = 1.0 / cpi;

    printf("\n--- RAW RESULTS ---\n");
    printf("Total Cycles:      %llu\n", (unsigned long long)total_cycles);
    printf("Total Instructions:  %.0f\n", total_insts);
    
    printf("\n--- FINAL METRICS ---\n");
    printf("Average CPI (Cycles Per Instruction): %.3f\n", cpi);
    printf("Average IPC (Instructions Per Cycle): %.3f\n", ipc);
    
    printf("\n--- ANALYSIS ---\n");
    if (ipc > 2.0) {
        printf("Observation: Throughput is very high (IPC > 2.0).\n");
        printf("Inference: The CPU's out-of-order engine is extremely effective at finding and exploiting the instruction-level parallelism in this specific workload, keeping its multiple execution units busy.\n");
    } else if (ipc > 1.0) {
        printf("Observation: Throughput is high (IPC > 1.0).\n");
        printf("Inference: The CPU is successfully executing more than one instruction per cycle, demonstrating superscalar capabilities.\n");
    } else {
        printf("Observation: Throughput is low (IPC < 1.0).\n");
        printf("Inference: The complex dependency chain in the benchmark is stalling the pipeline, preventing the \n CPU from using its parallel execution units effectively.\n");
    }

    return 0;
}