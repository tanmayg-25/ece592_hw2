#define _GNU_SOURCE // Must be the first line!
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <x86intrin.h>
#include <sched.h> // Header for sched_setaffinity

#define ITERATIONS 1000000
#define NUM_TESTS 1024 // Using 10 for a quicker test run

// --- Timing Harness ---
static inline void cpu_id(uint32_t op, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    uint32_t level = op;
    __asm__ __volatile__ ( "cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(level), "c"(0) );
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

// --- Test Functions ---

// Test 1: Predictable pattern (alternating true/false)
double test_predictable_pattern() {
    uint64_t total_cycles = 0;
    uint32_t aux;
    volatile int sink = 0; // Use a sink to prevent optimization

    for (int test = 0; test < NUM_TESTS; test++) {
        uint64_t start = rdtsc_serial();
        for (int i = 0; i < ITERATIONS; i++) {
            // This T/F/T/F pattern is trivial for a branch predictor
            if ((i & 1) == 0) { 
                sink++;
            }
        }
        uint64_t end = rdtscp_serial(&aux);
        total_cycles += (end - start);
    }
    return (double)total_cycles / (NUM_TESTS * ITERATIONS);
}

// Test 2: Unpredictable pattern (50/50 random)
double test_unpredictable_pattern() {
    uint64_t total_cycles = 0;
    uint32_t aux;
    volatile int sink = 0; // Use a sink

    // Pre-generate random pattern to avoid timing the rand() calls
    int* pattern = malloc(ITERATIONS * sizeof(int));
    for (int i = 0; i < ITERATIONS; i++) {
        pattern[i] = rand() % 2;
    }
    
    for (int test = 0; test < NUM_TESTS; test++) {
        uint64_t start = rdtsc_serial();
        for (int i = 0; i < ITERATIONS; i++) {
            if (pattern[i]) {
                sink++;
            }
        }
        uint64_t end = rdtscp_serial(&aux);
        total_cycles += (end - start);
    }
    
    free(pattern);
    return (double)total_cycles / (NUM_TESTS * ITERATIONS);
}

// Test 3: No branches at all (baseline for loop overhead)
double test_no_branches() {
    uint64_t total_cycles = 0;
    uint32_t aux;
    volatile int sink = 0; // Use a sink

    for (int test = 0; test < NUM_TESTS; test++) {
        uint64_t start = rdtsc_serial();
        for (int i = 0; i < ITERATIONS; i++) {
            sink++;
        }
        uint64_t end = rdtscp_serial(&aux);
        total_cycles += (end - start);
    }
    
    return (double)total_cycles / (NUM_TESTS * ITERATIONS);
}

int main() {
    // Pin process to a single CPU core
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity failed");
        return 1;
    }
    srand(time(NULL));
    
    printf("Testing Branch Prediction...\n");
    
    // Run tests
    double cycles_predictable = test_predictable_pattern();
    double cycles_unpredictable = test_unpredictable_pattern();
    double cycles_no_branches = test_no_branches();
    
    // Output results and analysis
    printf("\nRESULTS:\n");
    printf("Predictable Branches:     %.3f cycles/iteration\n", cycles_predictable);
    printf("Unpredictable Branches:   %.3f cycles/iteration\n", cycles_unpredictable);
    printf("No Branches (Baseline):   %.3f cycles/iteration\n", cycles_no_branches);

    // Calculate misprediction penalty
    double misprediction_penalty = cycles_unpredictable - cycles_predictable;
    printf("\nANALYSIS:\n");
    printf("Branch Misprediction Penalty: ~%.2f cycles\n", misprediction_penalty);
    
    // Save to CSV for plotting
    FILE* csv = fopen("branch_prediction_results.csv", "w");
    fprintf(csv, "test_type,cycles_per_iteration\n");
    fprintf(csv, "predictable,%.6f\n", cycles_predictable);
    fprintf(csv, "unpredictable,%.6f\n", cycles_unpredictable);
    fprintf(csv, "no_branch,%.6f\n", cycles_no_branches);
    fclose(csv);
    
    printf("\nData saved to branch_prediction_results.csv\n");
    return 0;
}

