#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>
#include <stdlib.h>
#define ITERATIONS 1000000
#define L1D_PER_CORE (48 * 1024)  // 48 KiB L1 data cache per core from lscpu
#define L2_PER_CORE (2 * 1024 * 1024) // 2 MiB L2 cache per core from lscpu
#define EVICTION_BUFFER_SIZE (L1D_PER_CORE + L2_PER_CORE) // A buffer large enough to evict both L1 and L2 caches completely

int main(void) 
{
    static uint64_t hit_times[ITERATIONS];
    static uint64_t miss_times[ITERATIONS];
    static volatile int probe_variable = 42;
    uint64_t start, end;
    unsigned aux;
    int tmp; // Dummay var
    size_t buf_len = EVICTION_BUFFER_SIZE / sizeof(int); // Buffer Creation
    int* evict_buf = malloc(buf_len * sizeof(int));
    if (!evict_buf) 
    { 
        perror("malloc for eviction buffer failed"); 
        return 1; 
    }
    
    for (size_t i = 0; i < buf_len; i++) // Initialization
    {
        evict_buf[i] = i;
    }

    for (int i = 0; i < ITERATIONS; i++) 
    {
        tmp = probe_variable; 
        _mm_mfence(); 
        for (size_t j = 0; j < buf_len; j += 16) // Eviction loop
        {
            evict_buf[j]++;
        }
        _mm_mfence(); 
        
        // L1/L2 miss but L3 hit
        start = __rdtscp(&aux);
        tmp = probe_variable;
        end = __rdtscp(&aux);
        hit_times[i] = end - start;
        
        // Eviction from all levels 
        _mm_clflush((void*)&probe_variable);
        _mm_mfence(); 
        
        start = __rdtscp(&aux);
        tmp = probe_variable;
        end = __rdtscp(&aux);
        miss_times[i] = end - start; //Time the access
    }
    
    free(evict_buf);
    
    printf("hit_time,miss_time\n");
    for (int i = 0; i < ITERATIONS; i++) 
    {
        printf("%llu,%llu\n", (unsigned long long)hit_times[i], (unsigned long long)miss_times[i]);
    }

    return 0;
}