#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <sys/mman.h>
#include <x86intrin.h>

#define CACHE_LINE_SIZE 64
#define ACCESSES_PER_RUN (1 << 22) // Increased for more stable measurements
#define MAX_ASSOCIATIVITY_TO_TEST 32

typedef struct {
    void *next;
    char padding[CACHE_LINE_SIZE - sizeof(void*)];
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

/**
 * @brief Measures TLB associativity by creating conflicts within a single TLB set.
 * * @param page_size The page size (4KB or 2MB) to test.
 * @param num_sets_in_tlb A guess for the number of sets in the TLB level being probed.
 * This determines the stride between conflicting accesses.
 */
void measure_tlb_associativity(size_t page_size, int num_sets_in_tlb) {
    printf("## Probing Associativity for a %d-set TLB with %zu KiB pages\n", num_sets_in_tlb, page_size / 1024);
    
    // The key to this benchmark: create a stride that ensures each access
    // hits the same TLB set. We do this by making the stride a multiple
    // of (number of sets * page_size).
    const size_t conflict_stride = page_size * num_sets_in_tlb;

    int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
    size_t total_mem_size = MAX_ASSOCIATIVITY_TO_TEST * conflict_stride;
    
    void *mem = mmap(NULL, total_mem_size, PROT_READ | PROT_WRITE, mmap_flags, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap failed");
        return;
    }
    
    // "Touch" all pages to ensure they are resident in memory
    for (size_t i = 0; i < total_mem_size; i += page_size) {
        ((char*)mem)[i] = 1;
    }
    
    printf("Ways, Cycles_per_Access\n");

    // Test for associativity from 1 up to the max
    for (int ways = 1; ways <= MAX_ASSOCIATIVITY_TO_TEST; ways++) {
        Node *nodes = (Node *)mem;

        int* indices = malloc(ways * sizeof(int));
        for (int i = 0; i < ways; i++) {
            indices[i] = i;
        }
        shuffle(indices, ways);

        // Build the pointer-chasing chain with the conflict stride
        for (int i = 0; i < ways - 1; i++) {
            Node* current_node = (Node*)((char*)nodes + indices[i] * conflict_stride);
            Node* next_node = (Node*)((char*)nodes + indices[i+1] * conflict_stride);
            current_node->next = next_node;
        }
        Node* last_node = (Node*)((char*)nodes + indices[ways-1] * conflict_stride);
        Node* first_node = (Node*)((char*)nodes + indices[0] * conflict_stride);
        last_node->next = first_node;
        
        // --- Measurement ---
        unsigned int junk;
        uint64_t start, end;
        volatile Node *current = first_node;

        // Warmup
        for(int i = 0; i < (1 << 16); i++) {
            current = current->next;
        }

        start = __rdtscp(&junk);
        for(int i = 0; i < ACCESSES_PER_RUN; i++) {
            current = current->next;
        }
        end = __rdtscp(&junk);
        
        double cycles_per_access = (double)(end - start) / ACCESSES_PER_RUN;
        printf("%d, %.2f\n", ways, cycles_per_access);

        free(indices);
    }

    munmap(mem, total_mem_size);
    printf("\n");
}


int main() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == -1) {
        perror("sched_setaffinity failed");
        return 1;
    }

    // From your previous results, the L1 DTLB for 4KB pages appears to be small.
    // Let's assume it has 32 sets to probe its associativity.
    measure_tlb_associativity(4096, 32);

    // Your L2 TLB (STLB) for 4KB pages appeared to have 512 entries.
    // If it's, for example, 8-way associative, it would have 64 sets.
    // If it's 4-way, it would have 128 sets. Let's probe for 512 sets as an upper bound.
    measure_tlb_associativity(4096, 512);
  

    // We don't know the 2MB page TLB structure, so let's start by probing
    // for a small number of sets (e.g., 32), which is common for huge page TLBs.
    // Note: This requires Transparent Huge Pages to be enabled and working.
    //measure_tlb_associativity(2 * 1024 * 1024, 32);


    return 0;
}
