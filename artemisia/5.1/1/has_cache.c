#define _GNU_SOURCE // Required for sched_setaffinity
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <sched.h> // Required for this!

// --- PORTABLE TIMING HARNESS ---
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

typedef struct {
    volatile char* buffer;
    size_t num_accesses;
    size_t stride;
} benchmark_args_t;

void access_working_set(void* arg) {
    benchmark_args_t* args = (benchmark_args_t*)arg;
    volatile char* buf = args->buffer;
    size_t accesses = args->num_accesses;
    size_t st = args->stride;
    for (size_t i = 0; i < accesses; i++) {
        volatile char x = buf[(i * st) % (accesses * st)];
        (void)x;
    }
}

int main() {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity failed");
        return 1;
    }

    size_t min_size = 4 * 1024;
    size_t max_size = 200 * 1024 * 1024;
    size_t stride = 128;
    int iterations = 100;

    volatile char* buffer = (volatile char*)malloc(max_size);
    if (!buffer) { perror("malloc failed"); return 1; }
    for (size_t i = 0; i < max_size; i += 4096) buffer[i] = 0;

    FILE* csv_file = fopen("cache_sweep_results.csv", "w");
    if (!csv_file) { perror("Failed to open CSV file"); free((void*)buffer); return 1; }
    
    fprintf(csv_file, "working_set_size_bytes,time_per_access_cycles\n");

    for (size_t size = min_size; size <= max_size; size *= 1.1) {
        if(size > max_size) size = max_size;
        size_t num_accesses = size / stride;
        if (num_accesses < 10) num_accesses = 10;

        benchmark_args_t args = {buffer, num_accesses, stride};
        uint64_t total_cycles = 0;
        uint32_t aux;

        for (int i = 0; i < iterations; i++) {
            uint64_t start = rdtsc_serial();
            access_working_set(&args);
            uint64_t end = rdtscp_serial(&aux);
            total_cycles += (end - start);
        }
        double avg_cycles = (double)total_cycles / (double)iterations;
        double time_per_access = avg_cycles / (double)num_accesses;

        fprintf(csv_file, "%zu,%.2f\n", size, time_per_access);
        printf("Size: %zu KB, Time/Access: %.2f cycles\n", size/1024, time_per_access);
    }

    fclose(csv_file);
    free((void*)buffer);
    printf("Data written to cache_sweep_results.csv\n");
    return 0;
}
