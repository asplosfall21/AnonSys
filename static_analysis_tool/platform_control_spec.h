#ifndef PLATFORM_CONTROL_SPEC_H
#define PLATFORM_CONTROL_SPEC_H

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

#define STR_IMPL_(x) #x      //stringify
#define STR(x) STR_IMPL_(x)

// Use double-macros so stringified argument can be expended by the pre-processor
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

#endif // PLATFORM_CONTROL_SPEC_H