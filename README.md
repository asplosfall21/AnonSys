# AnonSys

We present AnonSys, to our knowledge, the first enclave platform with microarchitectural isolation to run realistic secure programs on a speculative out-of-order multicore processor.

This repository contains pointers to the hardware and software infrastructure required to run our secure enclaves.
Our multicore processor runs on an FPGA and boots untrusted Linux from which users can securely launch and interact with enclaves.
We open-source our end-to-end hardware and software infrastructure, hoping to spark more research and bridge the gap between conceptual proposals and FPGA prototypes.

## Software Infrastructure

### Security Monitor

The Security Monitor (SM) is a small (~9K LOC), trusted piece of software running at a higher privilege mode than the hypervisor or the OS.
Its role is to link low-level invariants exposed by the hardware (e.g., if a specific memory access is authorized), and the high-level security policies defined by the platform (e.g., which enclave is currently executing on which core and which memory region does it own).

### Secure Bootloader

The Secure Bootloader is the first piece of code to run from the root-of-trust read-only-memory when the machine boots. 
It is trusted and will measure and attest the Security Monitor as part of the attestation mechanism.

### Linux Building Tools And Kernel Module

This repository contains tools and scripts to compile Linux to run on AnonSys. It also contains the code for the SM Kernel Module to enable Linux to interact with the Security Monitor and allow applications to launch secure enclaves.

### Static Analysis Tool

Simple static analysis tool that implement the software analysis part of Burst mode in Python.

### AnonSys QEMU Target

To debug software, you might want to install QEMU with the special target that emulates the Riscy-OO processor. 
Building instructions can be found in the repository.

## Enclave Repositories

We provide a series of example enclaves running on AnonSys.

### ML Enclave

This enclave is a proof of concept for a private inference service.

We convert a handwritten digit recognition model (trained on MNIST) to standard C code using onnx2c and run private inference inside our enclave.
A user uses our remote attestation mechanism to verify the enclave identity and establish a secure communication channel.
When receiving requests through shared memory, the enclave decrypts the ciphertext, performs the inference task on the private input and sends back an encrypted output.

### Crypto Enclave

This enclave is a wrapper around an ED2556 RISC-V library.
The wrapper makes it possible to keep the keys used by the library inside of the enclave and for the library functions to be accessed in a Remote Procedure Call style (RPC).
We add a queue to shared memory so untrusted applications can send requests to the library to generate keys (that will stay in enclave memory), or to sign messages using previously generated keys.

### Micropython Enclave

We do a minimal port of [MicroPython](https://github.com/micropython/micropython) inside one of our enclaves.
This repository contains the port it-self while the MicroPython Testbench repository contains a testbench to test the enclave and to debug it.

### CoreMark Enclave

Simple port of the [CoreMark benchmark](https://github.com/eembc/coremark) to run inside of a AnonSys enclave.

### SpectreV1 Enclave

Simple proof-of-concept Spectre V1 attack that leverages the LLC as a side channel when shared memory is enabled insecurely.

### Enclave Skeleton

Simple infrastructure to write, run and debug an enclave. Great place to start if you want to try to run your own.

## Hardware Infrastructure

### Riscy-OO and MI6

AnonSys is based on Riscy-OO and MI6, a fork of Riscy-OO that implement and evaluates several hardware security mechanisms.
AnonSys introduces several new hardware mechanisms such as secure shared memory, DMA chanel partitioning, dynamic LLC partitioning and other mechanisms that relax the strict security policy of MI6

We also implement several mechanisms that were described in MI6 but only evaluated using models such as MSHR partitioning and LLC arbiter partitioning.

We will release the AnonSys hardware changes soon.

### Microbenchmarks

This repository contains microbenchmarks to evaluate our fine-grained LLC partitioning scheme and our secure shared memory mechanism on sequential and random accesses to memory.

### Hardware Tests

This repository contains simple tests to verify the implementation correctness of the AnonSys memory isolation mechanisms introduced by Sanctum.