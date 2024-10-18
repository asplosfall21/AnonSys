#include <api_enclave.h>
#include "clib.h"
#include <ttp_enclave_util.h>

#if (DEBUG_ENCLAVE == 1)
#include "../sbi/console.h"
#endif

#define riscv_perf_cntr_begin() asm volatile("csrwi 0x801, 1")
#define riscv_perf_cntr_end() asm volatile("csrwi 0x801, 0")

uint8_t scratch[1024] = {0};
float output[1][10];

key_entry_t client_keys = {0};

struct AES_ctx aes_ctx;

hash_t measurement;
public_key_t enclave_pk;
secret_key_t enclave_sk;
signature_t signature;

void print_bytes(void * ptr, size_t length) {
  printm("[");
  for (int i = 0 ; i < length - 1; i++) {
    printm("0x%x, ", ((uint8_t *)ptr)[i]);
  }
  printm("0x%x]\n", ((uint8_t *)ptr)[length - 1]);
}

void handle_key_agreement(msg_t * msg) {
  // TODO: No longer seem to be getting the keys I set in sm_keys.c :(
  // TODO: If I don't refetch these, enclave keys become wrong and key derivation is incorrect.
  sm_enclave_get_keys(&measurement, &enclave_pk, &enclave_sk, &signature);

  // Copy client's public key
  memcpy(&client_keys.public_key, (void *)msg->args[0], sizeof(public_key_t));

  // Derive shared secrets & nonce
  perform_key_agreement(&client_keys.public_key, &enclave_sk, &client_keys.shared_key);
  hash(&client_keys.shared_key, sizeof(symmetric_key_t), &scratch);

  memcpy(&client_keys.stream_key, &scratch, sizeof(stream_key_t));

  // TODO: Unsafe IV generation :)
  memcpy(&client_keys.nonce, &scratch[sizeof(stream_key_t)], sizeof(stream_nonce_t));

  // Initialize AES context
  aes_init(&aes_ctx, &client_keys.stream_key, &client_keys.nonce);

  printm("Enclave, Stream Key:");
  print_bytes(&client_keys.stream_key, sizeof(stream_key_t));

  printm("Enclave, Nonce:");
  print_bytes(&client_keys.nonce, sizeof(stream_nonce_t));

  // Cleanup
  memset(&scratch, 0, sizeof(hash_t));
  memcpy((public_key_t *)msg->args[0], &enclave_pk, sizeof(public_key_t));
  msg->ret = 0;
}

void handle_add_1(msg_t * msg) {
  size_t length = msg->args[0];
  void * encrypted_msg = msg->args[1];

  // Copy encrypted message into enclave private memory
  memcpy(&scratch, encrypted_msg, length);

  // Decrypt before adding 1!
  aes_xcrypt(&aes_ctx, &scratch, length);

  for (int i = 0; i < length; i++) {
    printm("Adding 1 to 0x%x\n", scratch[i]);
    scratch[i] += 1;
  }

  // Encrypt before sending back to client
  aes_xcrypt(&aes_ctx, &scratch, length);

  memcpy(encrypted_msg, &scratch, length);
  memset(&scratch, 0, length);

  msg->ret = 0;
}

void handle_mnist_init(msg_t * msg) {
  uintptr_t tensor_cnn1_weight_ptr = msg->args[0];
  uintptr_t tensor_cnn2_weight_ptr = msg->args[1];
  uintptr_t tensor_linear_bias_ptr = msg->args[2];
  uintptr_t tensor_linear_weight_ptr = msg->args[3];
  uintptr_t tensor_14_ptr = msg->args[4];

  mnist_14x14_init(tensor_cnn1_weight_ptr, tensor_cnn2_weight_ptr, tensor_linear_bias_ptr, tensor_linear_weight_ptr, tensor_14_ptr);
}

void handle_mnist(msg_t * msg) {
  size_t length = msg->args[0];
  void * encrypted_msg = msg->args[1];
  void * encrypted_results = msg->args[2];

  // Copy encrypted image into enclave private memory
#if (MEASURE == 2)
  riscv_perf_cntr_begin();
#endif
  memcpy(&scratch, encrypted_msg, length);
#if (MEASURE == 2)
  riscv_perf_cntr_end();
#endif

  // Decrypt
#if (MEASURE == 3)
  riscv_perf_cntr_begin();
#endif
  aes_xcrypt(&aes_ctx, &scratch, length);
#if (MEASURE == 3)
  riscv_perf_cntr_end();
#endif

  // Call MNIST classifying model
  float output[1][10];
#if (MEASURE == 4)
  riscv_perf_cntr_begin();
#endif
  entry(&scratch, &output);
#if (MEASURE == 4)
  riscv_perf_cntr_end();
#endif

  // Find most likely label
  int8_t res = 0;
  float max = output[0][0];
  for (int i = 0; i < 10; i++) {
    if (output[0][i] > max) {
      max = output[0][i];
      res = i;
    }
  }

  // printm("Enclave detected label: %d\n", res);

  // Copy and send encrypted results back to client
  length = sizeof(res);
#if (MEASURE == 2)
  riscv_perf_cntr_begin();
#endif
  memcpy(&scratch, &res, length);
#if (MEASURE == 2)
  riscv_perf_cntr_end();
#endif

#if (MEASURE == 3)
  riscv_perf_cntr_begin();
#endif
  aes_xcrypt(&aes_ctx, &scratch, length);
#if (MEASURE == 3)
  riscv_perf_cntr_end();
#endif

#if (MEASURE == 2)
  riscv_perf_cntr_begin();
#endif
  memcpy(encrypted_results, &scratch, length);
#if (MEASURE == 2)
  riscv_perf_cntr_end();
#endif

  msg->args[0] = length;
  msg->ret = 0;
}

void enclave_main() {
  init_p_lock_global(0);

#if (MEASURE == 5)
  riscv_perf_cntr_end();
#endif

#if (DEBUG_ENCLAVE == 1)
  printm("Made it inside the enclave!\n");
#endif

  // TODO: No longer seem to be getting the keys I set in sm_keys.c :(
  sm_enclave_get_keys(&measurement, &enclave_pk, &enclave_sk, &signature);

  printm("Enclave SK:");
  print_bytes(&enclave_sk, sizeof(secret_key_t));

  printm("Enclave PK:");
  print_bytes(&enclave_pk, sizeof(public_key_t));

  queue_t * qreq = SHARED_REQU_QUEUE;
  queue_t * qres = SHARED_RESP_QUEUE;

  msg_t *m;
  int ret;

  while(true) {
    ret = pop(qreq, (void **) &m);
    if(ret != 0) continue;
    switch((m)->f) {
      case F_KEY_AGREEMENT:
        handle_key_agreement(m);
        break;
      case F_ADD_1:
        handle_add_1(m);
        break;
      case F_MNIST_INIT:
        handle_mnist_init(m);
        break;
      case F_MNIST:
#if (MEASURE == 1)
        riscv_perf_cntr_begin();
#endif
        handle_mnist(m);
#if (MEASURE == 1)
        riscv_perf_cntr_end();
#endif
        break;
      case F_EXIT:
        m->ret = 0;
        m->done = true;
        do {
          ret = push(qres, m);
        } while(ret != 0);
        while(1) {
          sm_exit_enclave();
        }
      default:
        printm("Enclave recv unhandled message\n");
        break;
    }
    m->done = true;
    do {
      ret = push(qres, m);
    } while(ret != 0);
  }

  while(1) {
    sm_exit_enclave();
  }
}
