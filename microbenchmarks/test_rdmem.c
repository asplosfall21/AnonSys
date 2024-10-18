#include "console.h"
#include <stdint.h>
#include <stddef.h>

// CSRs
#define CSR_MSPEC  0x7ca
#define CSR_SSPEC  0x190
#define CSR_SPEC   0x802

// MSPEC configuration
#define MSPEC_ALL    (0)
#define MSPEC_NONMEM (1)
#define MSPEC_NONE   (3)
#define MSPEC_NOTRAINPRED (4)
#define MSPEC_NOUSEPRED (8)
#define MSPEC_NOUSEL1 (16)

#define STR_IMPL_(x) #x      // stringify
#define STR(x) STR_IMPL_(x)

// CSR access macros
#define read_csr(reg) _read_csr(reg)
#define _read_csr(reg) ({ unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })

#define write_csr(reg, val) _write_csr(reg, val)
#define _write_csr(reg, val) ({ \
  asm volatile ("csrw " #reg ", %0" :: "rK"(val)); })

#define swap_csr(reg, val) _swap_csr(reg, val)
#define _swap_csr(reg, val) ({ unsigned long __tmp; \
  asm volatile ("csrrw %0, " #reg ", %1" : "=r"(__tmp) : "rK"(val)); \
  __tmp;})

#define set_csr(reg, bit) _set_csr(reg, bit)
#define _set_csr(reg, bit) ({ unsigned long __tmp; \
  asm volatile ("csrrs %0, " #reg ", %1" : "=r"(__tmp) : "rK"(bit)); \
  __tmp; })

#define clear_csr(reg, bit) _clear_csr(reg, bit)
#define _clear_csr(reg, bit) ({ unsigned long __tmp; \
  asm volatile ("csrrc %0, " #reg ", %1" : "=r"(__tmp) : "rK"(bit)); \
  __tmp; })

static inline void platform_disable_speculation() {
    uint64_t mspec = read_csr(CSR_SPEC);
    mspec |= MSPEC_NONE;
    write_csr(CSR_SPEC, mspec);
}

static inline void platform_enable_speculation() {
    uint64_t mspec = read_csr(CSR_SPEC);
    mspec &= ~MSPEC_NONE;
    write_csr(CSR_SPEC, mspec);
}

static inline void platform_disable_predictors() {
    set_csr(CSR_SPEC, MSPEC_NOTRAINPRED | MSPEC_NOUSEPRED);
}

static inline void platform_enable_predictors() {
    clear_csr(CSR_SPEC, MSPEC_NOTRAINPRED | MSPEC_NOUSEPRED);
}

static inline void platform_disable_L1() {
    uint64_t mspec = read_csr(CSR_SPEC);
    mspec |= MSPEC_NOUSEL1;
    write_csr(CSR_SPEC, mspec);
}

static inline void platform_enable_L1() {
    uint64_t mspec = read_csr(CSR_SPEC);
    mspec &= ~MSPEC_NOUSEL1;
    write_csr(CSR_SPEC, mspec);
}

#define SHARED_MEM_ADDR    ((void *) 0x91000000)
#define SHARED_MEM_SIZE    (0x1FC000)  // Adjust size as needed
#define PRIVATE_MEM_ADDR   ((void *) 0x80040000)
#define PRIVATE_MEM_SIZE   (0x1FC000)  // Adjust size as needed

#define riscv_perf_cntr_begin() asm volatile("csrwi 0x801, 1")
#define riscv_perf_cntr_end() asm volatile("csrwi 0x801, 0")

#include <stdlib.h>  // For rand()

// LCG Random Number Generator Macro
#define LCG_RANDOM(seed) \
    ((seed) = 1664525 * (seed) + 1013904223)

char random_memory_access(void* mem_region, size_t mem_size, size_t num_accesses) {
    char* mem = (char*)mem_region;
    size_t mem_size_in_bytes = mem_size;
    uint32_t seed = 0x12345678;  // Initial seed

    volatile char sink = 0;  // Prevent compiler optimization

    //platform_disable_predictors();
    for (size_t i = 0; i < num_accesses; i++) {
        uint32_t rand_index = LCG_RANDOM(seed) % mem_size_in_bytes;
        sink ^= mem[rand_index];  // Read access
    }
    //platform_enable_predictors();

    return sink;
}

#define SIZE 1024*100

void dut_entry_c() {
    //printm("Starting random memory access benchmark\n");

    char res;

    // Perform random accesses in private memory
    // riscv_perf_cntr_begin();
    // res = random_memory_access(PRIVATE_MEM_ADDR, PRIVATE_MEM_SIZE, SIZE);
    // riscv_perf_cntr_end();
    //printm("Completed random accesses in private memory\n", res);

    // // Perform random accesses in shared memory
    riscv_perf_cntr_begin();
    res = random_memory_access(SHARED_MEM_ADDR, SHARED_MEM_SIZE, SIZE);
    riscv_perf_cntr_end();
    // printm("Completed random accesses in shared memory %c\n", res);

    return;
}
