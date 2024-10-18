# Secure Bootloader

The Secure Bootloader is part of the prototype for AnonSys, the first side-channel-resistant enclave platform to run realistic secure programs on a speculative out-of-order multicore processor.

The Secure Bootloader is the first piece of code to run from the root-of-trust read-only-memory when the machine boots. 
It is trusted and will measure and attest the Security Monitor as part of the attestation mechanism.
