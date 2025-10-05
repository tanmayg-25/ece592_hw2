#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <sched.h>
#include <time.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>

#define ITERATIONS 1000000
#define NUM_RUNS 30

#define ARCH_REQ_XCOMP_PERM 0x1023
#define XFEATURE_XTILEDATA 18

// --- Helper Structs & Functions ---
typedef struct {
    uint8_t palette_id;
    uint8_t start_row;
    uint8_t reserved_0[14];
    uint16_t colsb[16];
    uint8_t rows[16];
} tilecfg_t;

static bool set_tiledata_use() {
    if (syscall(SYS_arch_prctl, ARCH_REQ_XCOMP_PERM, XFEATURE_XTILEDATA)) { return false; }
    return true;
}

// --- Tile Configurations ---
__attribute__((target("amx-int8")))
void configure_amx_int8() {
    alignas(64) tilecfg_t cfg = {0};
    cfg.palette_id = 1;
    // Tile A (tmm0): 16 rows, 64 bytes (64 int8s)
    cfg.rows[0] = 16; cfg.colsb[0] = 64;
    // Tile B (tmm1): 16 rows, 64 bytes (64 int8s)
    cfg.rows[1] = 16; cfg.colsb[1] = 64;
    // Result Tile C (tmm2): 16 rows, 64 bytes (16 x 4-byte integers)
    cfg.rows[2] = 16; cfg.colsb[2] = 64;
    _tile_loadconfig(&cfg);
}

__attribute__((target("amx-bf16")))
void configure_amx_bf16() {
    alignas(64) tilecfg_t cfg = {0};
    cfg.palette_id = 1;
    // Tile A (tmm0): 16 rows, 64 bytes (32 bf16s)
    cfg.rows[0] = 16; cfg.colsb[0] = 64;
    // Tile B (tmm1): 16 rows, 64 bytes (32 bf16s)
    cfg.rows[1] = 16; cfg.colsb[1] = 64;
    // Result Tile C (tmm2): 16 rows, 64 bytes (16 x 4-byte floats)
    cfg.rows[2] = 16; cfg.colsb[2] = 64;
    _tile_loadconfig(&cfg);
}

// --- Timing Harness & Sparsity Generator ---
static inline uint64_t start_timer(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ __volatile__ ( "cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0), "c"(0) );
    return __rdtsc();
}
static inline uint64_t end_timer(void) {
    uint32_t aux, eax, ebx, ecx, edx;
    uint64_t t = __rdtscp(&aux);
    __asm__ __volatile__ ( "cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0), "c"(0) );
    return t;
}

void generate_sparse_matrix(void* matrix, size_t rows, size_t cols, int sparsity_percent, const char* type) {
    size_t total_elements = rows * cols;
    size_t num_zeros = (total_elements * sparsity_percent) / 100;

    if (strcmp(type, "INT8") == 0) {
        int8_t* mat = (int8_t*)matrix;
        for (size_t i = 0; i < total_elements; ++i) mat[i] = (rand() % 127) + 1;
    } else { // BF16
        unsigned short* mat = (unsigned short*)matrix;
        for (size_t i = 0; i < total_elements; ++i) mat[i] = (rand() % 65535) + 1;
    }
    
    // Randomly place zeros using a shuffle
    for (size_t i = 0; i < num_zeros; ++i) {
        size_t j = i + rand() / (RAND_MAX / (total_elements - i) + 1);
        if (j >= total_elements) j = total_elements - 1;
        if (strcmp(type, "INT8") == 0) {
            int8_t temp = ((int8_t*)matrix)[i]; ((int8_t*)matrix)[i] = ((int8_t*)matrix)[j]; ((int8_t*)matrix)[j] = temp;
        } else {
            unsigned short temp = ((unsigned short*)matrix)[i]; ((unsigned short*)matrix)[i] = ((unsigned short*)matrix)[j]; ((unsigned short*)matrix)[j] = temp;
        }
    }
    if (strcmp(type, "INT8") == 0) { memset(matrix, 0, num_zeros); } 
    else { memset(matrix, 0, num_zeros * 2); }
}

// --- Main Benchmark Function ---
__attribute__((target("amx-int8,amx-bf16")))
void run_benchmark(FILE* csv, const char* data_type, uint8_t rows, uint16_t cols_bytes) {
    size_t cols = (strcmp(data_type, "INT8") == 0) ? cols_bytes : cols_bytes / 2;
    char tile_shape[32];
    snprintf(tile_shape, sizeof(tile_shape), "%dx%zu", rows, cols);
    
    void* matrix_a = malloc(rows * cols_bytes);
    void* matrix_b = malloc(rows * cols_bytes);
    // FIX: Allocate a result buffer
    void* result_matrix = malloc(rows * 64); // Result stride is always 64 bytes

    double dense_cycles = 0.0, sparse_cycles = 0.0;

    for (int sparsity = 0; sparsity <= 100; sparsity += 10) {
        printf("  Testing %s, Shape: %s, Sparsity: %d%%\n", data_type, tile_shape, sparsity);
        
        generate_sparse_matrix(matrix_a, rows, cols, sparsity, data_type);
        generate_sparse_matrix(matrix_b, rows, cols, sparsity, data_type);

        _tile_loadd(0, matrix_a, cols_bytes);
        _tile_loadd(1, matrix_b, cols_bytes);
        
        uint64_t total_cycles = 0;
        for(int r = 0; r < NUM_RUNS; ++r) {
            _tile_zero(2);
            uint64_t start = start_timer();
            for (int i = 0; i < ITERATIONS; ++i) {
                if (strcmp(data_type, "INT8") == 0) _tile_dpbssd(2, 0, 1);
                else _tile_dpbf16ps(2, 0, 1);
            }
            uint64_t end = end_timer();
            total_cycles += (end-start);
        }
        
        // FIX: Store the result to prevent dead code elimination
        _tile_stored(2, result_matrix, 64);
        asm volatile("" : "+m" (*(char*)result_matrix)); // Use the result

        double avg_cycles = (double)total_cycles / (NUM_RUNS * ITERATIONS);
        fprintf(csv, "%s,%s,%d,%.2f\n", data_type, tile_shape, sparsity, avg_cycles);
        
        if (sparsity == 0) dense_cycles = avg_cycles;
        if (sparsity == 100) sparse_cycles = avg_cycles;
    }
    
    printf("\n--- STATS SUMMARY for %s (%s) ---\n", data_type, tile_shape);
    printf("Dense Matrix (0%% Zeros) Time:   %.2f cycles\n", dense_cycles);
    printf("Sparse Matrix (100%% Zeros) Time: %.2f cycles\n", sparse_cycles);
    if (dense_cycles > 0 && sparse_cycles > 0) {
        double speedup = dense_cycles / sparse_cycles;
        printf("Speedup Factor:                   %.2fx\n", speedup);
        if (speedup > 1.1) {
            printf("Conclusion:                       Evidence of Zero-Skipping found.\n");
        } else {
            printf("Conclusion:                       No significant evidence of Zero-Skipping.\n");
        }
    }
    printf("------------------------------------------\n\n");
    
    free(matrix_a);
    free(matrix_b);
    free(result_matrix);
}

// Main function now calls the two working tests
__attribute__((target("amx-int8,amx-bf16")))
int main() {
    // Pin thread
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    sched_setaffinity(0, sizeof(set), &set);
    srand(time(NULL));

    if (!set_tiledata_use()) {
        printf("Failed to enable AMX tile data feature.\n"); return 1;
    }

    FILE* csv = fopen("amx_combined_results.csv", "w");
    fprintf(csv, "data_type,tile_shape,sparsity_percent,avg_cycles\n");
    printf("Running AMX TMUL benchmark for INT8 and BF16...\n");

    // --- Test 1: INT8 with 16x64 shape ---
    printf("\n--- Starting INT8 Test (16x64) ---\n");
    configure_amx_int8();
    run_benchmark(csv, "INT8", 16, 64);

    // --- Test 2: BF16 with 16x32 shape ---
    printf("\n--- Starting BF16 Test (16x32) ---\n");
    configure_amx_bf16();
    run_benchmark(csv, "BF16", 16, 64); // Note: 64 bytes = 32 bf16 elements

    _tile_release();
    fclose(csv);
    printf("\nData saved to amx_combined_results.csv\n");
    return 0;
}

