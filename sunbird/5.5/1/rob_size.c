#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <x86intrin.h>
#include <sched.h>
#include <sys/mman.h>

#define MAX_FILLERS 600
#define ITERATIONS 100000
#define NUM_RUNS 5

// --- Timing Functions ---
static inline uint64_t start_timer(void) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ __volatile__ ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0), "c"(0));
    return __rdtsc();
}

static inline uint64_t end_timer(void) {
    uint32_t aux, eax, ebx, ecx, edx;
    uint64_t t = __rdtscp(&aux);
    __asm__ __volatile__ ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0), "c"(0));
    return t;
}

// --- Pointer-Chasing Setup ---
void init_dbuf(void **dbuf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        dbuf[i] = &dbuf[(i + 1) % size];
    }
    
    // Shuffle for random access pattern
    srand(42);
    for (size_t i = size - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        void* temp = dbuf[i]; 
        dbuf[i] = dbuf[j]; 
        dbuf[j] = temp;
    }
}

// --- Code Generation with Better Dependency Chain ---
void make_routine(unsigned char* code_buf, void* p1, int filler_count) {
    int pos = 0;
    
    // Prologue: push rbp; mov rbp, rsp; push rbx
    code_buf[pos++] = 0x55;
    code_buf[pos++] = 0x48; code_buf[pos++] = 0x89; code_buf[pos++] = 0xE5;
    code_buf[pos++] = 0x53;
    
    // mov rcx, p1
    code_buf[pos++] = 0x48; code_buf[pos++] = 0xB9;
    *(uintptr_t*)(code_buf + pos) = (uintptr_t)p1;
    pos += 8;
    
    // mov rax, ITERATIONS
    code_buf[pos++] = 0x48; code_buf[pos++] = 0xB8;
    *(uintptr_t*)(code_buf + pos) = ITERATIONS;
    pos += 8;
    
    // Align loop start
    while (pos % 16 != 0) code_buf[pos++] = 0x90;
    int loop_start = pos;
    
    // === CRITICAL: Single long-latency operation ===
    // mov rcx, [rcx] - pointer chase (high latency, ~4 cycles + cache)
    code_buf[pos++] = 0x48; code_buf[pos++] = 0x8B; code_buf[pos++] = 0x09;
    
    // === Filler instructions (independent of the load) ===
    // Use simple ALU ops that create a dependency chain but don't touch rcx
    // This allows them to fill the ROB while waiting for the load
    
    // Start with rdx = 0
    code_buf[pos++] = 0x48; code_buf[pos++] = 0x31; code_buf[pos++] = 0xD2; // xor rdx, rdx
    
    for (int i = 0; i < filler_count; ++i) {
        // add rdx, 1 (creates dependency chain)
        code_buf[pos++] = 0x48; code_buf[pos++] = 0x83; code_buf[pos++] = 0xC2; code_buf[pos++] = 0x01;
    }
    
    // Use rdx to prevent dead code elimination
    // test rdx, rdx (doesn't affect rcx chain)
    code_buf[pos++] = 0x48; code_buf[pos++] = 0x85; code_buf[pos++] = 0xD2;
    
    // === Loop control ===
    // dec rax
    code_buf[pos++] = 0x48; code_buf[pos++] = 0xFF; code_buf[pos++] = 0xC8;
    
    // jnz loop_start
    code_buf[pos++] = 0x0F; code_buf[pos++] = 0x85;
    int32_t jump_offset = loop_start - (pos + 4);
    *(int32_t*)(code_buf + pos) = jump_offset;
    pos += 4;
    
    // Epilogue: pop rbx; pop rbp; ret
    code_buf[pos++] = 0x5B;
    code_buf[pos++] = 0x5D;
    code_buf[pos++] = 0xC3;
}

int main() {
    // Pin to core 0
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    sched_setaffinity(0, sizeof(set), &set);
    
    // Allocate large buffer for pointer chasing
    const size_t dbuf_size = 256 * 1024 * 1024;
    const size_t num_elements = dbuf_size / sizeof(void*);
    void** dbuf = (void**)malloc(dbuf_size);
    
    if (!dbuf) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }
    
    init_dbuf(dbuf, num_elements);
    
    // Allocate executable memory
    unsigned char* code_buf = (unsigned char*)mmap(NULL, 8192,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (code_buf == MAP_FAILED) {
        fprintf(stderr, "Failed to allocate executable memory\n");
        free(dbuf);
        return 1;
    }
    
    FILE* csv = fopen("robsize.csv", "w");
    fprintf(csv, "filler_count,avg_cycles,min_cycles,max_cycles\n");
    
    printf("ROB Size Benchmark\n");
    printf("==================\n");
    printf("Filler Count | Avg Cycles | Min | Max\n");
    printf("-------------+------------+-----+-----\n");
    
    for (int icount = 0; icount <= MAX_FILLERS; icount += 4) {
        void* p1 = dbuf;
        
        // Clear code buffer
        memset(code_buf, 0, 8192);
        
        // Generate routine
        make_routine(code_buf, p1, icount);
        void(*routine)() = (void(*)())code_buf;
        
        // Warm up
        routine();
        
        // Multiple runs to reduce noise
        uint64_t min_cycles = UINT64_MAX;
        uint64_t max_cycles = 0;
        uint64_t total_cycles = 0;
        
        for (int run = 0; run < NUM_RUNS; ++run) {
            uint64_t start = start_timer();
            routine();
            uint64_t end = end_timer();
            
            uint64_t cycles = end - start;
            if (cycles < min_cycles) min_cycles = cycles;
            if (cycles > max_cycles) max_cycles = cycles;
            total_cycles += cycles;
        }
        
        double avg_cycles = (double)total_cycles / (NUM_RUNS * ITERATIONS);
        double min_per_iter = (double)min_cycles / ITERATIONS;
        double max_per_iter = (double)max_cycles / ITERATIONS;
        
        printf("%12d | %10.2f | %3.0f | %3.0f\n", 
               icount, avg_cycles, min_per_iter, max_per_iter);
        
        fprintf(csv, "%d,%.2f,%.2f,%.2f\n", 
                icount, avg_cycles, min_per_iter, max_per_iter);
        fflush(csv);
    }
    
    printf("\n=== Analysis ===\n");
    printf("Look for a sharp increase ('knee') in avg_cycles.\n");
    printf("The knee occurs approximately at ROB size.\n");
    printf("Data saved to robsize.csv\n");
    
    fclose(csv);
    munmap(code_buf, 8192);
    free(dbuf);
    
    return 0;
}