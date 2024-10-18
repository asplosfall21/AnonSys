#include "console.h"
#include <stdint.h>

// LLC
#define LLC_INDEX_OFFSET 6
#define LLC_NUM_WAYS 16

// Zero-device for LLC flush
#define ZERO_DEVICE_OFFSET 0x100000000

#define NUM_REGIONS 64

// ### Protection Agaisnt Side Channels
#define CSR_MSPEC  0x7ca

#define set_csr(reg, bit) _set_csr(reg, bit)
#define _set_csr(reg, bit) ({ unsigned long __tmp; \
  asm volatile ("csrrs %0, " #reg ", %1" : "=r"(__tmp) : "rK"(bit)); \
  __tmp; })

#define clear_csr(reg, bit) _clear_csr(reg, bit)
#define _clear_csr(reg, bit) ({ unsigned long __tmp; \
  asm volatile ("csrrc %0, " #reg ", %1" : "=r"(__tmp) : "rK"(bit)); \
  __tmp; })

// MSPEC configuration
#define MSPEC_ALL    (0UL)
#define MSPEC_NONMEM (1UL)
#define MSPEC_NONE   (3UL)
#define MSPEC_NOTRAINPRED (4UL)
#define MSPEC_NOUSEPRED (8UL)
#define MSPEC_NOUSEL1 (16UL)

#define REGION_2_ADDR 0x86000000

#define L2_CTRL_ADDR 0x200000000
#define NUM_REGIONS 64

#define RAM_BASE 0x80000000
#define REGION_SHIFT (25)

static inline void * region_id_to_addr (uint64_t region_id) {
  return (void *)(RAM_BASE + (region_id << REGION_SHIFT));
}

#define riscv_perf_cntr_begin() asm volatile("csrwi 0x801, 1")
#define riscv_perf_cntr_end() asm volatile("csrwi 0x801, 0")

void flush_llc_region(uintptr_t start_addr, int lgsize){

    int size_index_range = 1 << lgsize;

    for(int stride = 0; stride < LLC_NUM_WAYS; stride++) {
        for(int i = 0; i < size_index_range; i++) {
            uintptr_t index = (size_index_range * stride) + i;
            uintptr_t addr = ZERO_DEVICE_OFFSET | start_addr | (index << LLC_INDEX_OFFSET);
            register uintptr_t t0 asm ("t0") = addr;
            // Activate and deactivate L1 use near the load responsible for the flush.
            asm volatile(" \
            #csrsi 0x7ca, 16; \n \
            #csrwi 0x801,  1; \n \
            ld t0, 0(t0); \n \
            #csrwi 0x801,  0; \n \
            #csrci 0x7ca, 16; \n\
            " : : "r" (t0));
        }
        asm volatile ("fence" ::: "memory");
    }
}

void dut_entry_c() {
    printm("Made it here\n");
    //uint64_t *l2Ctrl = (uint64_t *) L2_CTRL_ADDR;

    //riscv_perf_cntr_begin();
    //flush_llc_region(REGION_2_ADDR, 0x9);
    //riscv_perf_cntr_end();
   /* 
    for(int rid = 0; rid < NUM_REGIONS; rid++) {
        uint64_t base = rid * 16;
        uint64_t size = 0x4;
        printm("Setting up region %d with base %x and size %x\n", rid, base, size);
        *l2Ctrl = (rid << 14) + (base << 4) + size;
    }
    
    for(int rid = 0; rid < NUM_REGIONS; rid++) {
        uint64_t base = region_id_to_addr(rid * 16);
        uint64_t lgsize = 0x4;
        printm("Flushing region %d with base %x and lgsize %x\n", rid, base, lgsize);
        void flush_llc_region(uintptr_t base, int lgsize);
    }
    */
    
    return;
}
