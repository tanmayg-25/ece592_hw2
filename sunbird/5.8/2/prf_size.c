/*
  Fixed PRF Size Measurement
  Based on Henry Wong's methodology with corrections
*/
#define _GNU_SOURCE
#include <assert.h>
#include <getopt.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sched.h>
#include <stdint.h>

#define ADD_BYTE(val) do{ibuf[pbuf] = (val); pbuf++;} while(0)
#define ADD_WORD(val) do{*(unsigned short*)(&ibuf[pbuf]) = (val); pbuf+=2;} while(0)
#define ADD_DWORD(val) do{*(unsigned int*)(&ibuf[pbuf]) = (val); pbuf+=4;} while(0)
#define ADD_QWORD(val) do{*(unsigned long long*)(&ibuf[pbuf]) = (val); pbuf+=8;} while(0)

// --- Configuration ---
static int its = 10000;  // Inner loop iterations
const int MAX_ICOUNT = 400;
static const int unroll = 20;  // Increased unroll
const int STACK_SPACE = MAX_ICOUNT * unroll * 2 + 200;
static bool plot_mode;
static int start_icount = 10;
static int stop_icount = 180;
static int instr_type = 0;

// --- Test Definitions ---
struct test_info { int flags; const char *desc; };
const test_info tests[] = { {0, "parallel xor regN, regN+1"} };
const int test_count = sizeof(tests) / sizeof(tests[0]);

const test_info* get_test(int i) { 
    if (i < 0 || i >= test_count) return NULL; 
    return tests + i; 
}

const char *test_name(int i) { 
    const test_info* info = get_test(i); 
    return info ? info->desc : NULL; 
}

// --- Dynamic Code Generation ---
int add_filler(unsigned char* ibuf, int instr, int i, int k) {
    const int reg[8] = {3, 5, 6, 7, 8, 9, 10, 11};  // rbx, rbp, rsi, rdi, r8-r11
    int pbuf = 0;
    if (instr == 0) { 
        // xor reg[i], reg[i+1]
        int r1 = reg[i % 8];
        int r2 = reg[(i + 1) % 8];
        
        // REX prefix if needed (for r8-r15)
        if (r1 >= 8 || r2 >= 8) {
            ADD_BYTE(0x48 | ((r2 >= 8) ? 4 : 0) | ((r1 >= 8) ? 1 : 0));
        } else {
            ADD_BYTE(0x48);
        }
        ADD_BYTE(0x31);
        ADD_BYTE(0xc0 | ((r2 & 7) << 3) | (r1 & 7));
    }
    return pbuf;
}

void make_routine(unsigned char* ibuf, void *p1, void *p2, const int icount, const int instr) {
    assert(icount <= MAX_ICOUNT);
    const test_info* info = get_test(instr);
    if (!info) { 
        printf("invalid test ID %d\n", instr); 
        exit(EXIT_FAILURE); 
    }
    int pbuf = 0;
    
    // Prologue - save all callee-saved registers
    ADD_BYTE(0x53);  // push rbx
    ADD_BYTE(0x55);  // push rbp
    ADD_BYTE(0x41); ADD_BYTE(0x54);  // push r12
    ADD_BYTE(0x41); ADD_BYTE(0x55);  // push r13
    ADD_BYTE(0x41); ADD_BYTE(0x56);  // push r14
    ADD_BYTE(0x41); ADD_BYTE(0x57);  // push r15
    
    // Allocate stack space
    ADD_BYTE(0x48); ADD_BYTE(0x81); ADD_BYTE(0xEC); ADD_DWORD(STACK_SPACE);
    
    // Load pointer chain heads into rcx and rdx (rdi=p1, rsi=p2 from args)
    // mov rcx, rdi
    ADD_BYTE(0x48); ADD_BYTE(0x89); ADD_BYTE(0xf9);
    // mov rdx, rsi  
    ADD_BYTE(0x48); ADD_BYTE(0x89); ADD_BYTE(0xf2);
    
    // Load iteration count into r15
    ADD_BYTE(0x49); ADD_BYTE(0xc7); ADD_BYTE(0xc7); ADD_DWORD(its);
    
    // Align loop start to 16-byte boundary
    while (((uintptr_t)ibuf + pbuf) & 0xf) ADD_BYTE(0x90);
    int loop_start = pbuf;
    
    const int chain_length = 12;  // Longer dependency chain
    
    for (int u = 0; u < unroll; u++) {
        // First dependency chain: rcx = [rcx] repeated
        for(int l = 0; l < chain_length; l++) { 
            ADD_BYTE(0x48); ADD_BYTE(0x8b); ADD_BYTE(0x09);  // mov rcx, [rcx]
        }
        
        // Test instructions (creating register pressure)
        for (int j = 0; j < icount; j++) { 
            pbuf += add_filler(ibuf + pbuf, instr, j, u * icount + j); 
        }
        
        // Second dependency chain: rdx = [rdx] repeated
        for(int l = 0; l < chain_length; l++) { 
            ADD_BYTE(0x48); ADD_BYTE(0x8b); ADD_BYTE(0x12);  // mov rdx, [rdx]
        }
        
        // More test instructions
        for (int j = 0; j < icount; j++) { 
            pbuf += add_filler(ibuf + pbuf, instr, j, u * icount + j); 
        }
    }
    
    // Loop control: dec r15; jnz loop_start
    ADD_BYTE(0x49); ADD_BYTE(0xff); ADD_BYTE(0xcf);  // dec r15
    ADD_BYTE(0x0f); ADD_BYTE(0x85); ADD_DWORD(loop_start - pbuf - 4);  // jnz
    
    // Epilogue - restore stack and registers
    ADD_BYTE(0x48); ADD_BYTE(0x81); ADD_BYTE(0xC4); ADD_DWORD(STACK_SPACE);
    ADD_BYTE(0x41); ADD_BYTE(0x5F);  // pop r15
    ADD_BYTE(0x41); ADD_BYTE(0x5E);  // pop r14
    ADD_BYTE(0x41); ADD_BYTE(0x5D);  // pop r13
    ADD_BYTE(0x41); ADD_BYTE(0x5C);  // pop r12
    ADD_BYTE(0x5D);  // pop rbp
    ADD_BYTE(0x5B);  // pop rbx
    ADD_BYTE(0xC3);  // ret
    
    mprotect(ibuf, pbuf, PROT_READ|PROT_WRITE|PROT_EXEC);
}

inline unsigned long long int rdtsc() {
    unsigned int lo, hi;
    __asm__ volatile ("rdtsc" : "=a" (lo), "=d" (hi));
    return (((unsigned long long)hi) << 32) | lo;
}

inline unsigned long long int rdtsc_fenced() {
    unsigned int lo, hi;
    __asm__ volatile ("lfence\n\trdtsc\n\tlfence" : "=a" (lo), "=d" (hi) :: "memory");
    return (((unsigned long long)hi) << 32) | lo;
}

void init_dbuf(void **dbuf, int size) {
    // Create a single circular linked list
    for (int i = 0; i < size - 1; i++) {
        dbuf[i] = &dbuf[i + 1];
    }
    dbuf[size - 1] = &dbuf[0];  // Close the loop
}

void handle_args(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"csv",   no_argument, NULL, 'c'},
        {"start", required_argument, NULL, 'i'},
        {"stop",  required_argument, NULL, 'j'},
        {"iter",  required_argument, NULL, 'n'},
        {0, 0, 0, 0}
    };
    int optval = 0;
    while ((optval = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (optval) {
            case 'c': plot_mode = true; break;
            case 'i': sscanf(optarg, "%d", &start_icount); break;
            case 'j': sscanf(optarg, "%d", &stop_icount); break;
            case 'n': sscanf(optarg, "%d", &its); break;
            default: exit(EXIT_FAILURE);
        }
    }
    if (optind < argc) { 
        sscanf(argv[optind], "%d", &instr_type); 
    }
}

int main(int argc, char *argv[]) {
    // Pin to CPU 0
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity failed"); 
        return 1;
    }
    
    handle_args(argc, argv);
    
    const int memsize = 256 * 1024 * 1024;  // 256MB
    static int outer_its = 50;  // Number of samples per ICOUNT
    
    const char *name = test_name(instr_type);
    if (!name) { 
        fprintf(stderr, "Bad test ID: %d.\n", instr_type); 
        return EXIT_FAILURE; 
    }
    
    unsigned char *ibuf = (unsigned char*)valloc(4 * 1024 * 1024);  // 4MB code buffer
    void **dbuf1 = (void**)valloc(memsize);
    void **dbuf2 = (void**)valloc(memsize);
    
    init_dbuf(dbuf1, memsize / sizeof(void*));
    init_dbuf(dbuf2, memsize / sizeof(void*));
    
    typedef void(*routine_t)(void*, void*);
    routine_t routine = (routine_t)ibuf;
    
    FILE *fp = fopen("prf_raw_data.csv", "w");
    fprintf(fp, "ICOUNT,CYCLES\n");
    printf("Running PRF benchmark (test: %s)...\n", name);
    printf("Expected PRF sizes: Haswell ~168, Sapphire Rapids ~332\n\n");
    
    for (int icount = start_icount; icount <= stop_icount; icount += 2) {
        make_routine(ibuf, dbuf1, dbuf2, icount, instr_type);
        
        // Warmup
        for (int i = 0; i < 10; i++) {
            routine(dbuf1, dbuf2);
        }
        
        // Measure
        for (int i = 0; i < outer_its; i++) {
            unsigned long long start = rdtsc_fenced();
            routine(dbuf1, dbuf2);
            unsigned long long stop = rdtsc_fenced();
            
            long long diff = stop - start;
            double scaled_diff = (double)diff / its / unroll;
            fprintf(fp, "%d,%.2f\n", icount, scaled_diff);
        }
        
        if (icount % 20 == 0) {
            printf("  Progress: ICOUNT = %d\n", icount);
        }
    }
    
    fclose(fp);
    free(dbuf1);
    free(dbuf2);
    free(ibuf);
    
    printf("\nRaw data written to prf_raw_data.csv\n");
    printf("Run your analysis script to find the PRF size.\n");
    
    return 0;
}