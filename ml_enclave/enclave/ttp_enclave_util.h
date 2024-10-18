#ifndef TTP_ENCLAVE_UTIL_H
#define TTP_ENCLAVE_UTIL_H

#include <api_crypto_types.h>
#include <stdbool.h>
#include <msgq.h>
#include <cryptography.h>

#define F_KEY_AGREEMENT  0x0
#define F_ADD_1          0x1
#define F_MNIST_INIT     0x2
#define F_MNIST          0x3
#define F_EXIT           0x20

typedef struct msg_t {
  int f;
  uintptr_t args[5];
  int ret;
  bool done;
} msg_t;

typedef struct key_entry_t {
  public_key_t public_key;
  symmetric_key_t shared_key;
  stream_key_t stream_key;
  stream_nonce_t nonce;
} key_entry_t;

#define SHARED_MEM_REG (0x8a000000)
#define SHARED_REQU_QUEUE ((queue_t *) SHARED_MEM_REG)
#define SHARED_RESP_QUEUE ((queue_t *) (SHARED_MEM_REG + sizeof(queue_t)))

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
#define set_csr(reg, bit) _set_csr(reg, bit)
#define _set_csr(reg, bit) ({ unsigned long __tmp; \
  asm volatile ("csrrs %0, " #reg ", %1" : "=r"(__tmp) : "rK"(bit)); \
  __tmp; })

#define clear_csr(reg, bit) _clear_csr(reg, bit)
#define _clear_csr(reg, bit) ({ unsigned long __tmp; \
  asm volatile ("csrrc %0, " #reg ", %1" : "=r"(__tmp) : "rK"(bit)); \
  __tmp; })

static inline void platform_disable_predictors() {
    set_csr(CSR_SPEC, MSPEC_NOTRAINPRED | MSPEC_NOUSEPRED);
}

static inline void platform_enable_predictors() {
    clear_csr(CSR_SPEC, MSPEC_NOTRAINPRED | MSPEC_NOUSEPRED);
}

#endif // TTP_ENCLAVE_UTIL_H
