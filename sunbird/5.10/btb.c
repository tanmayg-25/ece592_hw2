#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <x86intrin.h>
#include <time.h> // ADDED for srand()

// IMPORTANT: Use `lscpu -e` to find the two logical CPUs for a single physical core.
#define LOGICAL_CPU_A 0
#define LOGICAL_CPU_B 24

// --- Workload for the Polluter Thread ---
// SIMPLIFIED: Reduced to 64 for a clearer example. The principle is the same.
#define NUM_POLLUTER_FUNCS 64

// SIMPLIFIED: Declare functions directly
#define DECLARE_FUNC(n) __attribute__((noinline)) void func##n() { __asm__ volatile("":::"memory"); }
DECLARE_FUNC(0); DECLARE_FUNC(1); DECLARE_FUNC(2); DECLARE_FUNC(3); DECLARE_FUNC(4); DECLARE_FUNC(5); DECLARE_FUNC(6); DECLARE_FUNC(7);
DECLARE_FUNC(8); DECLARE_FUNC(9); DECLARE_FUNC(10); DECLARE_FUNC(11); DECLARE_FUNC(12); DECLARE_FUNC(13); DECLARE_FUNC(14); DECLARE_FUNC(15);
DECLARE_FUNC(16); DECLARE_FUNC(17); DECLARE_FUNC(18); DECLARE_FUNC(19); DECLARE_FUNC(20); DECLARE_FUNC(21); DECLARE_FUNC(22); DECLARE_FUNC(23);
DECLARE_FUNC(24); DECLARE_FUNC(25); DECLARE_FUNC(26); DECLARE_FUNC(27); DECLARE_FUNC(28); DECLARE_FUNC(29); DECLARE_FUNC(30); DECLARE_FUNC(31);
DECLARE_FUNC(32); DECLARE_FUNC(33); DECLARE_FUNC(34); DECLARE_FUNC(35); DECLARE_FUNC(36); DECLARE_FUNC(37); DECLARE_FUNC(38); DECLARE_FUNC(39);
DECLARE_FUNC(40); DECLARE_FUNC(41); DECLARE_FUNC(42); DECLARE_FUNC(43); DECLARE_FUNC(44); DECLARE_FUNC(45); DECLARE_FUNC(46); DECLARE_FUNC(47);
DECLARE_FUNC(48); DECLARE_FUNC(49); DECLARE_FUNC(50); DECLARE_FUNC(51); DECLARE_FUNC(52); DECLARE_FUNC(53); DECLARE_FUNC(54); DECLARE_FUNC(55);
DECLARE_FUNC(56); DECLARE_FUNC(57); DECLARE_FUNC(58); DECLARE_FUNC(59); DECLARE_FUNC(60); DECLARE_FUNC(61); DECLARE_FUNC(62); DECLARE_FUNC(63);

// SIMPLIFIED: Initialize the array directly
void (*polluter_funcs[NUM_POLLUTER_FUNCS])(void) = {
    &func0, &func1, &func2, &func3, &func4, &func5, &func6, &func7,
    &func8, &func9, &func10, &func11, &func12, &func13, &func14, &func15,
    &func16, &func17, &func18, &func19, &func20, &func21, &func22, &func23,
    &func24, &func25, &func26, &func27, &func28, &func29, &func30, &func31,
    &func32, &func33, &func34, &func35, &func36, &func37, &func38, &func39,
    &func40, &func41, &func42, &func43, &func44, &func45, &func46, &func47,
    &func48, &func49, &func50, &func51, &func52, &func53, &func54, &func55,
    &func56, &func57, &func58, &func59, &func60, &func61, &func62, &func63
};


// --- Global variables for thread control ---
pthread_barrier_t barrier;
volatile int exit_flag = 0;

// --- Polluter Thread ---
void* polluter_thread_func(void* args) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(LOGICAL_CPU_A, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    pthread_barrier_wait(&barrier); // Synchronize start with victim

    int i = 0;
    while (!exit_flag) {
        polluter_funcs[i]();
        i = (i + 1) % NUM_POLLUTER_FUNCS;
    }
    return NULL;
}

// --- Victim Thread ---
void* victim_thread_func(void* args) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(LOGICAL_CPU_B, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    // Setup for the benchmark
    #define NUM_VICTIM_NODES (1 << 10)
    #define ACCESSES (1 << 20)
    typedef struct Node { struct Node *next; int p; } Node;
    Node* nodes = malloc(NUM_VICTIM_NODES * sizeof(Node));
    int* indices = malloc(NUM_VICTIM_NODES * sizeof(int));
    for (int i = 0; i < NUM_VICTIM_NODES; i++) indices[i] = i;
    for (size_t i = NUM_VICTIM_NODES - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        int temp = indices[i]; indices[i] = indices[j]; indices[j] = temp;
    }
    for (int i = 0; i < NUM_VICTIM_NODES - 1; i++) nodes[indices[i]].next = &nodes[indices[i+1]];
    nodes[indices[NUM_VICTIM_NODES - 1]].next = &nodes[indices[0]];

    pthread_barrier_wait(&barrier); // Synchronize start with polluter

    // Measurement
    unsigned int junk;
    volatile Node* current = &nodes[0];
    uint64_t start = __rdtscp(&junk);
    for (int i = 0; i < ACCESSES; i++) {
        if (current->p & 1) current = current->next;
        current = current->next;
    }
    uint64_t end = __rdtscp(&junk);

    free(nodes);
    free(indices);

    uint64_t total_cycles = end - start;
    pthread_exit((void*)total_cycles); // Return result
}


int main() {
    // ADDED: Seed random number generator
    srand(time(NULL));

    void* victim_result;
    uint64_t baseline_cycles, interference_cycles;
    pthread_t polluter, victim;

    printf("### BTB Shared vs. Partitioned Test ###\n");
    printf("Using logical CPUs %d and %d.\n\n", LOGICAL_CPU_A, LOGICAL_CPU_B);

    // --- 1. Baseline Run (Victim only) ---
    printf("--- Running Baseline (Victim Only) ---\n");
    pthread_barrier_init(&barrier, NULL, 1); // Barrier for 1 thread
    pthread_create(&victim, NULL, victim_thread_func, NULL);
    pthread_join(victim, &victim_result);
    baseline_cycles = (uint64_t)victim_result;
    printf("Baseline Cycles: %lu\n\n", baseline_cycles);
    pthread_barrier_destroy(&barrier);

    // --- 2. Interference Run (Polluter + Victim) ---
    printf("--- Running Interference Test (Polluter + Victim) ---\n");
    pthread_barrier_init(&barrier, NULL, 2); // Barrier for 2 threads
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
        printf("Result: The BTB appears to be SHARED across Hyper-Threads.\n");
    } else {
        printf("Result: The BTB appears to be PARTITIONED for each Hyper-Thread.\n");
    }

    return 0;
}
