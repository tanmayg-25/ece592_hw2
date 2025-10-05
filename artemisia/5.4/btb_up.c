#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <x86intrin.h>

// Generate many unique function targets
#define MAX_FUNCTIONS 8192

// Function template - we'll generate many of these
#define DECLARE_FUNC(n) \
    __attribute__((noinline)) void func##n() { \
        volatile int x = n; \
        x = x + 1; \
        __asm__ volatile("" ::: "memory"); \
    }

// Generate 100 unique functions
DECLARE_FUNC(0) DECLARE_FUNC(1) DECLARE_FUNC(2) DECLARE_FUNC(3) DECLARE_FUNC(4)
DECLARE_FUNC(5) DECLARE_FUNC(6) DECLARE_FUNC(7) DECLARE_FUNC(8) DECLARE_FUNC(9)
DECLARE_FUNC(10) DECLARE_FUNC(11) DECLARE_FUNC(12) DECLARE_FUNC(13) DECLARE_FUNC(14)
DECLARE_FUNC(15) DECLARE_FUNC(16) DECLARE_FUNC(17) DECLARE_FUNC(18) DECLARE_FUNC(19)
DECLARE_FUNC(20) DECLARE_FUNC(21) DECLARE_FUNC(22) DECLARE_FUNC(23) DECLARE_FUNC(24)
DECLARE_FUNC(25) DECLARE_FUNC(26) DECLARE_FUNC(27) DECLARE_FUNC(28) DECLARE_FUNC(29)
DECLARE_FUNC(30) DECLARE_FUNC(31) DECLARE_FUNC(32) DECLARE_FUNC(33) DECLARE_FUNC(34)
DECLARE_FUNC(35) DECLARE_FUNC(36) DECLARE_FUNC(37) DECLARE_FUNC(38) DECLARE_FUNC(39)
DECLARE_FUNC(40) DECLARE_FUNC(41) DECLARE_FUNC(42) DECLARE_FUNC(43) DECLARE_FUNC(44)
DECLARE_FUNC(45) DECLARE_FUNC(46) DECLARE_FUNC(47) DECLARE_FUNC(48) DECLARE_FUNC(49)
DECLARE_FUNC(50) DECLARE_FUNC(51) DECLARE_FUNC(52) DECLARE_FUNC(53) DECLARE_FUNC(54)
DECLARE_FUNC(55) DECLARE_FUNC(56) DECLARE_FUNC(57) DECLARE_FUNC(58) DECLARE_FUNC(59)
DECLARE_FUNC(60) DECLARE_FUNC(61) DECLARE_FUNC(62) DECLARE_FUNC(63) DECLARE_FUNC(64)
DECLARE_FUNC(65) DECLARE_FUNC(66) DECLARE_FUNC(67) DECLARE_FUNC(68) DECLARE_FUNC(69)
DECLARE_FUNC(70) DECLARE_FUNC(71) DECLARE_FUNC(72) DECLARE_FUNC(73) DECLARE_FUNC(74)
DECLARE_FUNC(75) DECLARE_FUNC(76) DECLARE_FUNC(77) DECLARE_FUNC(78) DECLARE_FUNC(79)
DECLARE_FUNC(80) DECLARE_FUNC(81) DECLARE_FUNC(82) DECLARE_FUNC(83) DECLARE_FUNC(84)
DECLARE_FUNC(85) DECLARE_FUNC(86) DECLARE_FUNC(87) DECLARE_FUNC(88) DECLARE_FUNC(89)
DECLARE_FUNC(90) DECLARE_FUNC(91) DECLARE_FUNC(92) DECLARE_FUNC(93) DECLARE_FUNC(94)
DECLARE_FUNC(95) DECLARE_FUNC(96) DECLARE_FUNC(97) DECLARE_FUNC(98) DECLARE_FUNC(99)

// Array to hold function pointers
void (*functions[MAX_FUNCTIONS])(void);

#define MIN_BRANCHES 64
#define MAX_BRANCHES 2048
#define STEP_SIZE    64
#define NUM_RUNS     1000000
#define WARMUP_RUNS  10

// Simple LCG for pseudo-random sequence
static uint32_t rng_state = 12345;
uint32_t simple_rand() {
    rng_state = rng_state * 1664525 + 1013904223;
    return rng_state;
}

void init_functions() {
    // Initialize array with our 100 unique functions
    void (*base_funcs[100])(void) = {
        func0, func1, func2, func3, func4, func5, func6, func7, func8, func9,
        func10, func11, func12, func13, func14, func15, func16, func17, func18, func19,
        func20, func21, func22, func23, func24, func25, func26, func27, func28, func29,
        func30, func31, func32, func33, func34, func35, func36, func37, func38, func39,
        func40, func41, func42, func43, func44, func45, func46, func47, func48, func49,
        func50, func51, func52, func53, func54, func55, func56, func57, func58, func59,
        func60, func61, func62, func63, func64, func65, func66, func67, func68, func69,
        func70, func71, func72, func73, func74, func75, func76, func77, func78, func79,
        func80, func81, func82, func83, func84, func85, func86, func87, func88, func89,
        func90, func91, func92, func93, func94, func95, func96, func97, func98, func99
    };
    
    // Fill the array with random selection from our base functions
    for (int i = 0; i < MAX_FUNCTIONS; i++) {
        functions[i] = base_funcs[simple_rand() % 100];
    }
}

int main() {
    printf("Initializing BTB size measurement...\n");
    init_functions();
    
    FILE *fp = fopen("btb_performance.csv", "w");
    if (!fp) { perror("fopen"); return 1; }
    fprintf(fp, "Num_Branches,Average_Cycles,Min_Cycles,Max_Cycles\n");

    uint64_t start, end;
    unsigned int aux;

    // Loop through different numbers of branches
    for (int num_branches = MIN_BRANCHES; num_branches <= MAX_BRANCHES; num_branches += STEP_SIZE) {
        
        uint64_t total_cycles = 0;
        uint64_t min_cycles = UINT64_MAX;
        uint64_t max_cycles = 0;
        
        
        // Actual measurement runs
        for (int run = 0; run < NUM_RUNS; run++) {
            // Serialize execution and flush pipeline
            __asm__ volatile("cpuid" ::: "eax", "ebx", "ecx", "edx", "memory");
            
            start = __rdtscp(&aux);
            
            // The actual test loop - indirect branches with unpredictable targets
            for (int j = 0; j < num_branches; j++) {
                functions[j](); // This is the indirect branch we're measuring
            }
            
            end = __rdtscp(&aux);
            __asm__ volatile("cpuid" ::: "eax", "ebx", "ecx", "edx", "memory");
            
            uint64_t cycles = end - start;
            total_cycles += cycles;
            if (cycles < min_cycles) min_cycles = cycles;
            if (cycles > max_cycles) max_cycles = cycles;
        }

        double avg_cycles_per_branch = (double)total_cycles / (NUM_RUNS * num_branches);
        double min_cycles_per_branch = (double)min_cycles / num_branches;
        double max_cycles_per_branch = (double)max_cycles / num_branches;
        
        fprintf(fp, "%d,%.2f,%.2f,%.2f\n", num_branches, avg_cycles_per_branch, 
                min_cycles_per_branch, max_cycles_per_branch);
        printf("Branches: %4d, Avg: %6.2f, Min: %6.2f, Max: %6.2f cycles/branch\n", 
               num_branches, avg_cycles_per_branch, min_cycles_per_branch, max_cycles_per_branch);
        
        // Add some noise between measurements to avoid measurement artifacts
        for (volatile int i = 0; i < 1000; i++);
    }

    fclose(fp);
    printf("\nResults written to btb_performance.csv\n");
    printf("Look for a sharp increase in cycles/branch to identify BTB capacity.\n");
    return 0;
}
