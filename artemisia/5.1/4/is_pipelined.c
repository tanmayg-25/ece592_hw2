#define _GNU_SOURCE // Must be the first line!
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <sched.h>

#define ITERATIONS 1000000
#define NUM_TESTS 10
#define CHAIN_LENGTH 10 // Number of instructions per loop

// --- Timing Harness (from your code) ---
static inline void cpu_id(uint32_t op, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    uint32_t level = op;
    __asm__ __volatile__ ( "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(level), "c"(0) );
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
    // Pin process to a single CPU core
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity failed"); return 1;
    }

    uint64_t start, end;
    uint32_t aux;

    // We'll use volatile local variables that map to registers via constraints.
    // Initialize them so the assembler operates on well-defined values.
    // Using multiple variables for independent test to enable parallelism.
    register uint64_t r0 asm("rax") = 1;
    register uint64_t r1 asm("rcx") = 2;
    register uint64_t r2 asm("rdx") = 3;
    register uint64_t r3 asm("rsi") = 4;
    register uint64_t r4 asm("rdi") = 5;
    register uint64_t r5 asm("r8")  = 6;
    register uint64_t r6 asm("r9")  = 7;
    register uint64_t r7 asm("r10") = 8;
    register uint64_t r8 asm("r11") = 9;
    register uint64_t r9 asm("r12") = 10; // r12 is callee-saved on x86_64 ABI, but binding won't force saves here; we avoid relying on that later.

    // --- Test 1: Independent Operations ---
    uint64_t independent_cycles = 0;
    for (int t = 0; t < NUM_TESTS; ++t) {
        // Reinitialize registers before each measurement to avoid carrying state
        r0 = 1; r1 = 2; r2 = 3; r3 = 4; r4 = 5; r5 = 6; r6 = 7; r7 = 8; r8 = 9; r9 = 10;

        start = rdtsc_serial();
        for (int i = 0; i < ITERATIONS; i++) {
            // Perform chain of independent adds using asm with read/write constraints.
            // Each "+r"(rx) tells compiler the variable is both input and output (prevents removal).
            __asm__ volatile (
                "add %1, %0\n\t"
                "add %2, %1\n\t"
                "add %3, %2\n\t"
                "add %4, %3\n\t"
                "add %5, %4\n\t"
                "add %6, %5\n\t"
                "add %7, %6\n\t"
                "add %8, %7\n\t"
                "add %9, %8\n\t"
                "add %10, %9\n\t"
                : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4),
                  "+r"(r5), "+r"(r6), "+r"(r7), "+r"(r8), "+r"(r9)
                :
                : "memory"
            );
        }
        end = rdtscp_serial(&aux);
        independent_cycles += (end - start);
    }
    double cpi_independent = (double)independent_cycles / (NUM_TESTS * (double)ITERATIONS * CHAIN_LENGTH);
    double ipc_independent = 1.0 / cpi_independent;

    // --- Test 2: Dependent Operations ---
    uint64_t dependent_cycles = 0;
    for (int t = 0; t < NUM_TESTS; ++t) {
        // Single dependent register chain: r0 depends on r0 each time.
        r0 = 1;

        start = rdtsc_serial();
        for (int i = 0; i < ITERATIONS; i++) {
            // All adds update the same register (dependent chain).
            __asm__ volatile (
                "add %0, %0\n\t" // 1
                "add %0, %0\n\t" // 2
                "add %0, %0\n\t" // 3
                "add %0, %0\n\t" // 4
                "add %0, %0\n\t" // 5
                "add %0, %0\n\t" // 6
                "add %0, %0\n\t" // 7
                "add %0, %0\n\t" // 8
                "add %0, %0\n\t" // 9
                "add %0, %0\n\t" // 10
                : "+r"(r0)
                :
                : "memory"
            );
        }
        end = rdtscp_serial(&aux);
        dependent_cycles += (end - start);
    }
    double cpi_dependent = (double)dependent_cycles / (NUM_TESTS * (double)ITERATIONS * CHAIN_LENGTH);
    double ipc_dependent = 1.0 / cpi_dependent;

    // --- Print and Save Results ---
    printf("\n=== RESULTS (Cycles per Instruction - CPI) ===\n");
    printf("Independent operations (Throughput): %.6f CPI\n", cpi_independent);
    printf("Dependent operations (Latency):    %.6f CPI\n", cpi_dependent);

    printf("\n=== RESULTS (Instructions per Cycle - IPC) ===\n");
    printf("Independent operations (Throughput): %.6f IPC\n", ipc_independent);
    printf("Dependent operations (Latency):    %.6f IPC\n", ipc_dependent);

    printf("\n=== CONCLUSION ===\n");
    if (ipc_independent > ipc_dependent * 1.5) {
        printf("STRONG EVIDENCE OF PIPELINING & SUPERSCALAR EXECUTION\n");
        printf("IPC for independent instructions is %.2fx higher than for dependent ones.\n", ipc_independent / ipc_dependent);
    } else {
        printf("MEASUREMENT ISSUE OR NO STRONG EVIDENCE\n");
    }

    FILE* csv = fopen("pipeline_analysis.csv", "w");
    if (csv) {
        fprintf(csv, "test_type,cpi,ipc\n");
        fprintf(csv, "independent,%.6f,%.6f\n", cpi_independent, ipc_independent);
        fprintf(csv, "dependent,%.6f,%.6f\n", cpi_dependent, ipc_dependent);
        fclose(csv);
        printf("\nData saved to pipeline_analysis.csv\n");
    } else {
        perror("fopen");
    }

    return 0;
}
