#include <api_enclave.h>
#include "clib.h"

#if (DEBUG_ENCLAVE == 1)
#include "../sbi/console.h"
#endif

#define riscv_perf_cntr_begin() asm volatile("csrwi 0x801, 1")
#define riscv_perf_cntr_end() asm volatile("csrwi 0x801, 0")

void enclave_main() {
#if (DEBUG_ENCLAVE == 1)
  printm("Made it inside the enclave!\n");
#endif
  while(1) {
    sm_exit_enclave();
  }
}
