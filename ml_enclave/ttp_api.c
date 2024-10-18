#include <ttp_api.h>
#include <untrusted_util.h>
#include <msgq.h>

void request_key_agreement(const public_key_t * public_key) {
  queue_t *q = SHARED_REQU_QUEUE;
  msg_t *msg = malloc(sizeof(msg_t));
  msg->f = F_KEY_AGREEMENT;
  msg->args[0] = (uintptr_t) public_key;
  int ret;
  do {
    ret = push(q, msg);
  } while (ret != 0);
}

void request_add_1(const void * encrypted_msg, const size_t msg_len) {
  queue_t *q = SHARED_REQU_QUEUE;
  msg_t *msg = malloc(sizeof(msg_t));
  msg->f = F_ADD_1;
  msg->args[0] = (uintptr_t) msg_len;
  msg->args[1] = (uintptr_t) encrypted_msg;
  int ret;
  do {
    ret = push(q, msg);
  } while(ret != 0);
}

void request_mnist_init(uintptr_t tensor_cnn1_weight_ptr, uintptr_t tensor_cnn2_weight_ptr, uintptr_t tensor_linear_bias_ptr, uintptr_t tensor_linear_weight_ptr) {
  queue_t *q = SHARED_REQU_QUEUE;
  msg_t *msg = malloc(sizeof(msg_t));
  msg->f = F_MNIST_INIT;
  msg->args[0] = (uintptr_t) tensor_cnn1_weight_ptr;
  msg->args[1] = (uintptr_t) tensor_cnn2_weight_ptr;
  msg->args[2] = (uintptr_t) tensor_linear_bias_ptr;
  msg->args[3] = (uintptr_t) tensor_linear_weight_ptr;
  int ret;
  do {
    ret = push(q, msg);
  } while(ret != 0);
}

void request_mnist(const void * encrypted_msg, const size_t msg_len, const void * encrypted_results) {
  queue_t *q = SHARED_REQU_QUEUE;
  msg_t *msg = malloc(sizeof(msg_t));
  msg->f = F_MNIST;
  msg->args[0] = (uintptr_t) msg_len;
  msg->args[1] = (uintptr_t) encrypted_msg;
  msg->args[2] = (uintptr_t) encrypted_results;
  int ret;
  do {
    ret = push(q, msg);
  } while(ret != 0);
}

void request_exit(void) {
  queue_t *q = SHARED_REQU_QUEUE;
  msg_t *msg = malloc(sizeof(msg_t));
  msg->f = F_EXIT;
  int ret;
  do {
    ret = push(q, msg);
  } while(ret != 0);
}

void init_enclave_queues() {
  queue_t *qrequ = SHARED_REQU_QUEUE;  
  queue_t *qresp = SHARED_RESP_QUEUE;
  init_q(qrequ);
  init_q(qresp);
}

bool req_queue_is_full() {
  queue_t *q = SHARED_REQU_QUEUE;  
  return is_full(q); 
}

bool resp_queue_is_empty() {
  queue_t *q = SHARED_RESP_QUEUE;  
  return is_empty(q); 
}
