#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <sys/mman.h>
#include <x86intrin.h>

#define NUM_NODES (1 << 16)
#define ACCESSES_PER_RUN (1 << 20)

typedef struct Node {
    struct Node *next;
    int payload;
} Node;

void shuffle(int *array, size_t n) {
    if (n > 1) {
        for (size_t i = n - 1; i > 0; i--) {
            size_t j = rand() % (i + 1);
            int temp = array[i];
            array[i] = array[j];
            array[j] = temp;
        }
    }
}

int main() {
    // Pin to a single core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    // Allocate and set up the circular linked list
    Node *nodes = malloc(NUM_NODES * sizeof(Node));
    int* indices = malloc(NUM_NODES * sizeof(int));
    for (int i = 0; i < NUM_NODES; i++) indices[i] = i;
   
    for (int i = 0; i < NUM_NODES - 1; i++) {
        nodes[indices[i]].next = &nodes[indices[i+1]];
        nodes[indices[i]].payload = rand();
    }
    nodes[indices[NUM_NODES - 1]].next = &nodes[indices[0]];
    nodes[indices[NUM_NODES - 1]].payload = rand();

    unsigned int junk;
    uint64_t start, end;
    
    // --- 1. Measure Baseline (Correctly Predicted Branch) ---
    volatile Node *current = &nodes[0];
    // Warmup
    for(int i = 0; i < ACCESSES_PER_RUN; i++) {
        current = current->next;
    }
    
    start = __rdtscp(&junk);
    for(int i = 0; i < ACCESSES_PER_RUN; i++) {
        current = current->next;
    }
    end = __rdtscp(&junk);
    double baseline_cycles = (double)(end - start) / ACCESSES_PER_RUN;
    shuffle(indices, NUM_NODES);
    // --- 2. Measure Mispredicted Branch ---
    current = &nodes[0];
    volatile int dummy = 0;
    // Warmup
    for(int i = 0; i < ACCESSES_PER_RUN; i++) {
        if (current->payload & 1) dummy++;
        current = current->next;
    }

    start = __rdtscp(&junk);
    for(int i = 0; i < ACCESSES_PER_RUN; i++) {
        if (current->payload & 1) dummy++;
        current = current->next;
    }
    end = __rdtscp(&junk);
    double mispredicted_cycles = (double)(end - start) / ACCESSES_PER_RUN;
    
    // --- 3. Calculate the Penalty ---
    double misprediction_penalty = mispredicted_cycles - baseline_cycles;
    
    printf("--- Pipeline Depth Estimation ---\n");
    printf("Cycles per mispredicted iteration: %.2f\n", mispredicted_cycles);
    printf("Measured baseline for correct prediction:  %.2f\n", baseline_cycles);
    printf("-----------------------------------------\n");
    printf("Estimated Branch Misprediction Penalty: %.0f cycles\n", misprediction_penalty);
    printf("-----------------------------------------\n");

    free(indices);
    free(nodes);
    return 0;
}

// #define _GNU_SOURCE
// #include <stdio.h>
// #include <stdlib.h>
// #include <stdint.h>
// #include <unistd.h>
// #include <string.h>
// #include <sched.h>
// #include <sys/mman.h>
// #include <x86intrin.h>

// #define NUM_NODES (1 << 16) // 65536 nodes
// #define ACCESSES_PER_RUN (1 << 20)

// typedef struct Node {
//     struct Node *next;
//     int payload;
// } Node;

// void shuffle(int *array, size_t n) {
//     if (n > 1) {
//         for (size_t i = n - 1; i > 0; i--) {
//             size_t j = rand() % (i + 1);
//             int temp = array[i];
//             array[i] = array[j];
//             array[j] = temp;
//         }
//     }
// }

// int main() {
//     // Pin to a single core
//     cpu_set_t cpuset;
//     CPU_ZERO(&cpuset);
//     CPU_SET(1, &cpuset);
//     if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == -1) {
//         perror("sched_setaffinity");
//         return 1;
//     }

//     // Allocate memory
//     Node *nodes = malloc(NUM_NODES * sizeof(Node));
//     if (!nodes) {
//         perror("malloc");
//         return 1;
//     }

//     // Create a shuffled index array
//     int* indices = malloc(NUM_NODES * sizeof(int));
//     for (int i = 0; i < NUM_NODES; i++) indices[i] = i;
//     shuffle(indices, NUM_NODES);

//     // Build the pointer-chasing chain
//     for (int i = 0; i < NUM_NODES - 1; i++) {
//         nodes[indices[i]].next = &nodes[indices[i+1]];
//         nodes[indices[i]].payload = rand(); // Fill with random data
//     }
//     nodes[indices[NUM_NODES - 1]].next = &nodes[indices[0]];
//     nodes[indices[NUM_NODES - 1]].payload = rand();

//     // --- Measurement ---
//     unsigned int junk;
//     uint64_t start, end;
//     volatile Node *current = &nodes[0];
//     volatile int dummy = 0;

//     // Warmup
//     for(int i = 0; i < (1 << 18); i++) {
//         if (current->payload & 1) dummy++; // Unpredictable branch
//         current = current->next;
//     }

//     start = __rdtscp(&junk);
//     for(int i = 0; i < ACCESSES_PER_RUN; i++) {
//         if (current->payload & 1) dummy++; // This is the mispredicted branch
//         current = current->next;
//     }
//     end = __rdtscp(&junk);

//     double cycles_per_iteration = (double)(end - start) / ACCESSES_PER_RUN;
    
//     // The baseline for a correctly predicted dependent branch + load is ~5-7 cycles.
//     // The misprediction penalty is the extra time on top of that.
//     double baseline = 7.0; 
//     double misprediction_penalty = cycles_per_iteration - baseline;
    
//     printf("--- Pipeline Depth Estimation ---\n");
//     printf("Cycles per mispredicted iteration: %.2f\n", cycles_per_iteration);
//     printf("Baseline for correctly predicted:  ~%.2f\n", baseline);
//     printf("-----------------------------------------\n");
//     printf("Estimated Branch Misprediction Penalty: %.0f cycles\n", misprediction_penalty);
//     printf("-----------------------------------------\n");

//     free(indices);
//     free(nodes);
//     return 0;
// }