/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <string.h>
#include <api_enclave.h>
#include "py/mpconfig.h"
#include "cryptography.h"

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;

void Reset_Handler(void) __attribute__((naked));
void bare_main(void);

static void enclave_init(void);
void mp_hal_stdout_tx_strn(const char *str, mp_uint_t len);

static char* console_buf_idx;

hash_t measurement;
public_key_t pk;
secret_key_t sk;
signature_t attestation;

hash_context_t output_h_ctxt;

const char hex_map[] = "0123456789ABCDEF";

// The CPU runs this function when entering the enclave.
void enclave_entry(uintptr_t console_buf) {
    // Zero out .bss section.
    memset(&_sbss, 0, (char *)&_ebss - (char *)&_sbss);
    
    // Setup console
    console_buf_idx = (char *)console_buf;

    // Initialise the cpu and peripherals.
    enclave_init();

    // Now that there is a basic system up and running, call the main application code.
    bare_main();

    // Sign the output hash and print the signature
    hash_t output_h;
    hash_finalize(&output_h_ctxt, &output_h);

    signature_t signature;
    sign(&output_h, sizeof(hash_t), &pk, &sk, &signature);

    int len = sizeof(signature_t);
    char signature_str[2 * len + 1];
    for(int i=0; i < len; i++) {
      signature_str[i * 2] = hex_map[signature.bytes[i] >> 4];
      signature_str[i * 2 + 1] = hex_map[signature.bytes[i] & 0x0F];
    }
    signature_str[len * 2] = '\n';

    mp_hal_stdout_tx_strn(signature_str, len * 2 + 1);

    sm_exit_enclave(); 
    // This function must not return.
    for (;;) {
    }
}

// Set up the ENCLAVE.
static void enclave_init(void) {
  api_result_t res;
  do{
    res = sm_enclave_get_keys(&measurement, &pk, &sk, &attestation);
  } while(res != MONITOR_OK);

  hash_init(&output_h_ctxt);
}

static inline uintptr_t console_putchar(uint8_t c) {
  hash_extend(&output_h_ctxt, &c, sizeof(uint8_t));
  *console_buf_idx++ = c;
  return 0x0;
}

static uint64_t console_getchar() {
  return 0x0;
}

// Receive single character
int mp_hal_stdin_rx_chr(void) {
  return console_getchar();
}

// Send string of given length
void mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
    while (len--) {
      console_putchar(*str++);
    }
}

// Send string of given length to stdout, converting \n to \r.
void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len) {
    while (len--) {
        if (*str == '\n') {
            console_putchar('\r');
        }
        console_putchar(*str++);
    }
}
