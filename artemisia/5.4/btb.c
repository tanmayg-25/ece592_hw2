#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <x86intrin.h>

// A series of dummy functions to create distinct branch targets
void f0() { volatile int x = 0; x++; }
void f1() { volatile int x = 0; x++; }
void f2() { volatile int x = 0; x++; }
void f3() { volatile int x = 0; x++; }
void f4() { volatile int x = 0; x++; }
void f5() { volatile int x = 0; x++; }
void f6() { volatile int x = 0; x++; }
void f7() { volatile int x = 0; x++; }

// Array to hold function pointers
void (*functions[4096])(void);

#define MIN_BRANCHES 4
#define MAX_BRANCHES 64
#define STEP_SIZE    4
#define NUM_RUNS     1000

int main() {
    // Populate the function pointer array with a non-repeating sequence
    for (int i = 0; i < 4096; i++) {
        switch (i % 8) {
            case 0: functions[i] = f0; break;
            case 1: functions[i] = f1; break;
            case 2: functions[i] = f2; break;
            case 3: functions[i] = f3; break;
            case 4: functions[i] = f4; break;
            case 5: functions[i] = f5; break;
            case 6: functions[i] = f6; break;
            case 7: functions[i] = f7; break;
        }
    }

    FILE *fp = fopen("btb_performance.csv", "w");
    if (!fp) { perror("fopen"); return 1; }
    fprintf(fp, "Num_Branches,Average_Cycles\n");

    uint64_t start, end, total_cycles;
    unsigned int aux;

    // Loop through different numbers of branches
    for (int num_branches = MIN_BRANCHES; num_branches <= MAX_BRANCHES; num_branches += STEP_SIZE) {
        total_cycles = 0;

        // Run the test multiple times for a stable average
        for (int i = 0; i < NUM_RUNS; i++) {
            start = __rdtscp(&aux);
            for (int j = 0; j < num_branches; j++) {
                functions[j](); // Indirect branch call
            }
            end = __rdtscp(&aux);
            total_cycles += (end - start);
        }

        double average_cycles_per_branch = (double)total_cycles / (NUM_RUNS * num_branches);
        fprintf(fp, "%d,%.2f\n", num_branches, average_cycles_per_branch);
        printf("Branches: %d, Avg Cycles/Branch: %.2f\n", num_branches, average_cycles_per_branch);
    }

    fclose(fp);
    return 0;
}
