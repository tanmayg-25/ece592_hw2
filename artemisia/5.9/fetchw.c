#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <unistd.h>
#include <x86intrin.h>

// The number of NOPs we will unroll in our assembly code.
// A larger number reduces the relative overhead of the loop's jump.
#define NUM_NOPS 1024

// The number of times we repeat the entire block of NOPs.
#define NUM_RUNS 100000

// Use macros to make writing thousands of NOPs easier.
#define NOP1 "nop\n\t"
#define NOP16 NOP1 NOP1 NOP1 NOP1 NOP1 NOP1 NOP1 NOP1 NOP1 NOP1 NOP1 NOP1 NOP1 NOP1 NOP1 NOP1
#define NOP256 NOP16 NOP16 NOP16 NOP16 NOP16 NOP16 NOP16 NOP16 NOP16 NOP16 NOP16 NOP16 NOP16 NOP16 NOP16 NOP16

int main() {
    // Pin the process to a single core for stable measurements.
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == -1) {
        perror("sched_setaffinity failed");
        return 1;
    }

    uint64_t start, end;
    unsigned int junk;

    // A simple loop to warm up the instruction cache.
    for (int i = 0; i < 1000; i++) {
        __asm__ volatile(
            // We only need a fraction of the NOPs for the warmup.
            NOP256
            NOP256
        );
    }
    
    // --- Measurement ---
    __asm__ volatile("cpuid" ::: "eax", "ebx", "ecx", "edx", "memory"); // Serialize
    start = __rdtscp(&junk);

    for (long i = 0; i < NUM_RUNS; i++) {
        // This block executes NUM_NOPS (1024) instructions.
        __asm__ volatile(
            NOP256 // 256 NOPs
            NOP256 // 512 NOPs
            NOP256 // 768 NOPs
            NOP256 // 1024 NOPs
        );
    }

    end = __rdtscp(&junk);
    __asm__ volatile("cpuid" ::: "eax", "ebx", "ecx", "edx", "memory"); // Serialize

    uint64_t total_cycles = end - start;
    uint64_t total_instructions = (uint64_t)NUM_RUNS * NUM_NOPS;
    double ipc = (double)total_instructions / total_cycles;

    printf("--- Superscalar Fetch Width Test ---\n");
    printf("Total Instructions: %lu\n", total_instructions);
    printf("Total Cycles:       %lu\n", total_cycles);
    printf("------------------------------------\n");
    printf("Instructions Per Cycle (IPC): %.2f\n", ipc);
    printf("------------------------------------\n");
    
    // Round the IPC to the nearest integer to guess the width.
    int fetch_width = (int)(ipc + 0.5);
    printf("Inferred CPU Fetch Width: %d\n", fetch_width);

    return 0;
}
