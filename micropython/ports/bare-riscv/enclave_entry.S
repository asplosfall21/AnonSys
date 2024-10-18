.option norvc
.section .text.entry

# save shared mem addr passed as the sp 
mv a0, sp

# set stack pointer
la sp, enclave_stack_ptr 

la ra, enclave_entry
jr ra

# Should not return!
.hang:
  j .hang
