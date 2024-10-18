.section ".stack"
.globl enclave_stack_base, enclave_stack_ptr
enclave_stack_base:
  .skip (0x3000)
enclave_stack_ptr:
