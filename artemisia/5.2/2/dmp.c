#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <sched.h>
#include <time.h>
#include <string.h>

static inline uint64_t rdtsc_start(void){
    unsigned eax, ebx, ecx, edx;
    // FIX: Removed "%rbx" and "%rcx" from the clobber list
    __asm__ volatile("cpuid" 
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) 
        : "a"(0) 
        : );
    return __rdtsc();
}

static inline uint64_t rdtsc_end(void){
    unsigned aux, eax, ebx, ecx, edx;
    uint64_t t = __rdtscp(&aux);
    // This cpuid is just for serialization, clobbering all is fine.
    __asm__ volatile("cpuid" 
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) 
        : "a"(0)
        : );
    return t;
}

void pin_core(int core) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("sched_setaffinity");
    }
}

size_t *make_random_list(size_t n) {
    size_t *arr = malloc(n * sizeof(size_t));
    size_t *idx = malloc(n * sizeof(size_t));
    for (size_t i = 0; i < n; i++) idx[i] = i;
    // Fisher-Yates shuffle
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        size_t t = idx[i];
        idx[i] = idx[j];
        idx[j] = t;
    }
    for (size_t i = 0; i < n - 1; i++) arr[idx[i]] = idx[i+1];
    arr[idx[n-1]] = idx[0]; // Complete the cycle
    free(idx);
    return arr;
}

size_t *make_regular_offset_list(size_t n, size_t offset) {
    size_t *arr = malloc(n * sizeof(size_t));
    for (size_t i = 0; i < n; i++) arr[i] = (i + offset) % n;
    return arr;
}

size_t *make_signature_pattern(size_t n, size_t period) {
    size_t *arr = malloc(n * sizeof(size_t));
    for (size_t i = 0; i < n; i++) {
        arr[i] = (i + (i % period) * 7) % n;
    }
    return arr;
}

double run_chase(size_t *list, size_t n, size_t steps, int warm) {
    volatile size_t cur = 0;
    if (warm) {
        for (size_t i = 0; i < n; i++) cur = list[cur];
    }
    uint64_t t0 = rdtsc_start();
    for (size_t i = 0; i < steps; i++) cur = list[cur];
    uint64_t t1 = rdtsc_end();
    (void)cur; // Prevent unused variable warning
    return (double)(t1 - t0) / (double)steps;
}

int main(int argc, char **argv){
    pin_core(0);
    srand(time(NULL) ^ (uintptr_t)&argc);

    size_t n = 4 * 1024 * 1024 / sizeof(size_t); // array about 32MB
    size_t steps = 1000000;
    int runs = 10;

    FILE *f = fopen("dmp_pointer_chase.csv","w");
    fprintf(f,"pattern,n,run,cycles_per_step\n");

    // Create patterns to test
    size_t *rand_list = make_random_list(n);
    size_t *off16 = make_regular_offset_list(n, 16);
    size_t *off64 = make_regular_offset_list(n, 64);
    size_t *sig8 = make_signature_pattern(n, 8);
    size_t *sig16 = make_signature_pattern(n, 16);

    for (int r = 0; r < runs; r++) {
        double v;
        v = run_chase(rand_list, n, steps, 1); fprintf(f,"random,%zu,%d,%.6f\n", n, r, v);
        v = run_chase(off16, n, steps, 1); fprintf(f,"offset16,%zu,%d,%.6f\n", n, r, v);
        v = run_chase(off64, n, steps, 1); fprintf(f,"offset64,%zu,%d,%.6f\n", n, r, v);
        v = run_chase(sig8, n, steps, 1); fprintf(f,"sig8,%zu,%d,%.6f\n", n, r, v);
        v = run_chase(sig16, n, steps, 1); fprintf(f,"sig16,%zu,%d,%.6f\n", n, r, v);
    }

    fclose(f);
    free(rand_list); free(off16); free(off64); free(sig8); free(sig16);
    printf("Done: written dmp_pointer_chase.csv\n");
    return 0;
}