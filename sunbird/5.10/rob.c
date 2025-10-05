#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <x86intrin.h>

// Set these to the two logical CPUs of a single physical core from `lscpu -e`
#define LOGICAL_CPU_A 0
#define LOGICAL_CPU_B 24

#define NUM_REPS (1 << 18)

// --- Global variables for thread control ---
pthread_barrier_t barrier;
volatile int exit_flag = 0;

// This is the core workload that fills the ROB.
// A long chain of dependent instructions.
static inline void rob_filler_workload() {
    __asm__ volatile (
        // 32 dependent imul instructions
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        "imul %%rbx, %%rax\n\t"
        : // No output
        : // No input
        : "rax", "rbx" // Clobbered registers
    );
}


// --- Polluter Thread ---
void* polluter_thread_func(void* args) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(LOGICAL_CPU_A, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    pthread_barrier_wait(&barrier);

    while (!exit_flag) {
        rob_filler_workload();
    }
    return NULL;
}

// --- Victim Thread ---
void* victim_thread_func(void* args) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(LOGICAL_CPU_B, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    pthread_barrier_wait(&barrier);

    unsigned int junk;
    uint64_t start = __rdtscp(&junk);
    for (int i = 0; i < NUM_REPS; i++) {
        rob_filler_workload();
    }
    uint64_t end = __rdtscp(&junk);
    
    uint64_t total_cycles = end - start;
    pthread_exit((void*)total_cycles);
}


int main() {
    void* victim_result;
    uint64_t baseline_cycles, interference_cycles;
    pthread_t polluter, victim;

    printf("### ROB Shared vs. Partitioned Test ###\n");
    printf("Using logical CPUs %d and %d.\n\n", LOGICAL_CPU_A, LOGICAL_CPU_B);

    // --- 1. Baseline Run (Victim only) ---
    printf("--- Running Baseline (Victim Only) ---\n");
    pthread_barrier_init(&barrier, NULL, 1);
    pthread_create(&victim, NULL, victim_thread_func, NULL);
    pthread_join(victim, &victim_result);
    baseline_cycles = (uint64_t)victim_result;
    printf("Baseline Cycles: %lu\n\n", baseline_cycles);
    pthread_barrier_destroy(&barrier);

    // --- 2. Interference Run (Polluter + Victim) ---
    printf("--- Running Interference Test (Polluter + Victim) ---\n");
    pthread_barrier_init(&barrier, NULL, 2);
    exit_flag = 0;
    pthread_create(&polluter, NULL, polluter_thread_func, NULL);
    pthread_create(&victim, NULL, victim_thread_func, NULL);
    pthread_join(victim, &victim_result);
    exit_flag = 1; // Signal polluter to stop
    pthread_join(polluter, NULL);
    interference_cycles = (uint64_t)victim_result;
    printf("Interference Cycles: %lu\n\n", interference_cycles);
    pthread_barrier_destroy(&barrier);

    // --- 3. Conclusion ---
    printf("--- Conclusion ---\n");
    double slowdown = (double)interference_cycles / baseline_cycles;
    printf("Performance slowdown: %.2fx\n", slowdown);
    if (slowdown > 1.20) { // If performance is >20% worse
        printf("Result: The ROB appears to be SHARED across Hyper-Threads.\n");
    } else {
        printf("Result: The ROB appears to be PARTITIONED for each Hyper-Thread.\n");
    }

    return 0;
}
