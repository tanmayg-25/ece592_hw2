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
#define ACCESSES_PER_RUN (1 << 20)

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

void measure_tlb(size_t page_size, size_t max_pages) {
    printf("## Testing with page size: %zu KiB\n", page_size / 1024);
    
    int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
    // We no longer need MAP_HUGETLB
    
    size_t total_mem_size = max_pages * page_size;
    void *mem = mmap(NULL, total_mem_size, PROT_READ | PROT_WRITE, mmap_flags, -1, 0);

    if (mem == MAP_FAILED) {
        perror("mmap failed");
        return;
    }

    // --- THP MODIFICATION START ---
    if (page_size > 4096) {
        printf("    -> Forcing page faults to encourage Transparent Huge Pages...\n");
        // "Touch" every 4KiB page to force the OS to allocate physical memory.
        char *mem_char = (char *)mem;
        for (size_t i = 0; i < total_mem_size; i += 4096) {
            mem_char[i] = 1; // Write to the page
        }
        printf("    -> Pausing for 1 sec to allow kernel to promote pages...\n");
        sleep(1);
    }
    // --- THP MODIFICATION END ---

    printf("NumPages, TotalSize_MiB, Cycles_per_Access\n");

    for (size_t num_pages = 2; num_pages <= max_pages; num_pages+=4) {
        size_t stride = page_size / sizeof(Node);
        Node *nodes = (Node *)mem;

        int* page_indices = malloc(num_pages * sizeof(int));
        for (size_t i = 0; i < num_pages; i++) {
            page_indices[i] = i;
        }
        shuffle(page_indices, num_pages);

        for (size_t i = 0; i < num_pages - 1; i++) {
            nodes[page_indices[i] * stride].next = &nodes[page_indices[i+1] * stride];
        }
        nodes[page_indices[num_pages - 1] * stride].next = &nodes[page_indices[0] * stride];

        unsigned int junk;
        uint64_t start, end;
        volatile Node *current = &nodes[0];

        for(int i = 0; i < ACCESSES_PER_RUN; i++) {
            current = current->next;
        }

        start = __rdtscp(&junk);
        for(int i = 0; i < ACCESSES_PER_RUN; i++) {
            current = current->next;
        }
        end = __rdtscp(&junk);

        double cycles_per_access = (double)(end - start) / ACCESSES_PER_RUN;
        double total_size_mib = (double)(num_pages * page_size) / (1024 * 1024);

        printf("%zu, %.2f, %.2f\n", num_pages, total_size_mib, cycles_per_access);
        free(page_indices);
          //if (num_pages < 16) {
          //   num_pages += 2;
        // } else {
         //    num_pages *= 2;
       //  }
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

    measure_tlb(4096, 1024);
    //measure_tlb(2 * 1024 * 1024, 512);

    return 0;
}

