#include <untrusted_util.h>
#include <api_untrusted.h>
#include <local_cryptography.h>
#include <ttp_api.h>

//extern uintptr_t region1;
extern uintptr_t region2;
extern uintptr_t region3;

extern uintptr_t enclave_start;
extern uintptr_t enclave_end;

volatile enclave_id_t enclave_id;

/*
 * images_bin: greyscale images for our mnist model. array of (float[14][14])*1024
 * labels_bin: labels for our images. (int8_t)*1024
 */
extern uintptr_t images_bin;
extern uintptr_t labels_bin;

#define SHARED_MEM_SYNC 0x90000000

#define STATE_0 0
#define STATE_1 1
#define STATE_2 2
#define STATE_3 3

#define EVBASE 0x20000000

static void enclave_core(void);
static void client_core(void);

key_entry_t enclave_keys;

extern const float tensor_cnn1_weight[20][1][5][5];
extern const float tensor_cnn2_weight[12][20][3][3]; 
extern const float tensor_linear_bias[10]; 
extern const float tensor_linear_weight[10][108];

struct AES_ctx aes_ctx;
#if (MEASURE >= 6)
struct AES_ctx aes_ctx_baseline;
#endif

uint8_t scratch[1024];

volatile int *flag = (int *) SHARED_MEM_SYNC;

void untrusted_main(int core_id, uintptr_t fdt_addr) {
  // Init Peterson's Lock library with core_id
  init_p_lock_global(core_id);

  if(core_id == 0) {
    enclave_core();
    test_completed();
  } else if (core_id == 1) {
    client_core();
    test_completed();
  } else {
    printm("Core n %d\n\n", core_id);
    test_completed();
  }
}

void print_bytes(void * ptr, size_t length) {
  printm("[");
  for (int i = 0 ; i < length - 1; i++) {
    printm("0x%x, ", ((uint8_t *)ptr)[i]);
  }
  printm("0x%x]\n", ((uint8_t *)ptr)[length - 1]);
}

void client_core(void) {
  // volatile int *flag = (int *) SHARED_MEM_SYNC;
  // await flag
  asm volatile("fence");
  while(*flag != STATE_3) {
    if(*flag == STATE_1) {
     api_result_t res = sm_region_update();
     printm("Client res: %d\n", res);
     if(res == MONITOR_OK) {
      *flag = STATE_2;
     }
    }
  };

  // initialize queues
  init_enclave_queues();

  printm("Made it here with flag: %d\n", *flag);

  bool verified = verify_attestation(enclave_id);
  printm("Verified? ");
  if (verified) { printm("Yes!\n"); } else { printm("Nope :(\n"); }

  // initialize queues
  // init_enclave_queues();

  // HACKS ON HACKS - Leaves spaces for the two queues
  init_heap(SHARED_MEM_REG + (2 * sizeof(queue_t)), 500 * PAGE_SIZE);

  msg_t *m;
  queue_t *qresp = SHARED_RESP_QUEUE;
  int ret;

  // Create keypair
  uint8_t seed_bytes[32] = {0xaa};
  key_seed_t * seed = &seed_bytes;
  secret_key_t secret_key;
  public_key_t public_key;
  local_create_secret_signing_key(&seed, &secret_key);
  local_compute_public_signing_key(&secret_key, &public_key);

  // Send/recv pubkey with enclave
  memcpy(&scratch, &public_key, sizeof(public_key));
  request_key_agreement((public_key_t *)&scratch);

  do {
    ret = pop(qresp, (void **) &m);
  } while((ret != 0) || (m->f != F_KEY_AGREEMENT));

  memcpy(&enclave_keys.public_key, &scratch, sizeof(public_key_t));

  // Derive shared secrets & nonce
  local_perform_key_agreement(&enclave_keys.public_key, &secret_key, &enclave_keys.shared_key);
  local_hash(&enclave_keys.shared_key, sizeof(symmetric_key_t), &scratch);

  memcpy(&enclave_keys.stream_key, &scratch, sizeof(stream_key_t));

  // TODO: Unsafe IV generation :)
  memcpy(&enclave_keys.nonce, &scratch[sizeof(stream_key_t)], sizeof(stream_nonce_t));
  memset(&scratch, 0, sizeof(hash_t));

  // Initialize AES context
  local_aes_init(&aes_ctx, &enclave_keys.stream_key, &enclave_keys.nonce);
#if (MEASURE >= 6)
  local_aes_init(&aes_ctx_baseline, &enclave_keys.stream_key, &enclave_keys.nonce);
#endif

  printm("Client, Stream Key:");
  print_bytes(&enclave_keys.stream_key, sizeof(stream_key_t));

  printm("Client, Nonce:");
  print_bytes(&enclave_keys.nonce, sizeof(stream_nonce_t));

  /*
  // Create a batch of data for the enclave to add 1 to.
  size_t data_length = 16;
  for (int i = 0; i < data_length; i++) {
    scratch[i] = (i) << 4;
  }

  // Encrypt before sending
  local_aes_xcrypt(&aes_ctx, &scratch, data_length);
  request_add_1(&scratch, data_length);

  do {
    ret = pop(qresp, (void **) &m);
  } while((ret != 0) || (m->f != F_ADD_1));

  // Decrypt returned data
  local_aes_xcrypt(&aes_ctx, &scratch, m->args[0]);

  printm("Client got back:");
  for (int i = 0; i < m->args[0]; i++) {
    printm(" 0x%x", scratch[i]);
  }

  printm("\n");
  */

  /* Handwriting recognition */
  size_t data_length = sizeof(float)*14*14; // 14x14 images
  void * image_ptr; // ptr to current image
  int8_t label;     // current label
  int8_t res;       // result label

  int correct = 0;  // accuracy
  int wrong = 0;    // counters

#if (MEASURE >= 6)
#if (MEASURE == 6)
  riscv_perf_cntr_begin();
#endif
  for (int i = 0; i < 1024; i++) {
    image_ptr = ((uint8_t *)&images_bin) + i*data_length;
    label = ((uint8_t *)&labels_bin)[i];

#if (MEASURE == 6)
    // We don't include this encryption since it should have taken place by the time we receive this message.
    riscv_perf_cntr_end();
    local_aes_xcrypt(&aes_ctx, image_ptr, data_length);
    riscv_perf_cntr_begin();
#else
    local_aes_xcrypt(&aes_ctx, image_ptr, data_length);
#endif

#if (MEASURE == 7)
    riscv_perf_cntr_begin();
    local_aes_xcrypt(&aes_ctx_baseline, image_ptr, data_length);
    riscv_perf_cntr_end();
#else
    local_aes_xcrypt(&aes_ctx_baseline, image_ptr, data_length);
#endif

    float output[1][10];
#if (MEASURE == 8)
    riscv_perf_cntr_begin();
    entry(image_ptr, &output);
    riscv_perf_cntr_end();
#else
    entry(image_ptr, &output);
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

    if (res == label) { correct++; } else { wrong++; }

    size_t length = sizeof(res);
#if (MEASURE == 7)
    riscv_perf_cntr_begin();
    local_aes_xcrypt(&aes_ctx, &res, length);
    riscv_perf_cntr_end();
#else
    local_aes_xcrypt(&aes_ctx, &res, length);
#endif

#if (MEASURE == 6)
    // We don't include this decryption, just need it to occur to keep both AES contexts aligned.
    riscv_perf_cntr_end();
    local_aes_xcrypt(&aes_ctx_baseline, &res, length);
    riscv_perf_cntr_begin();
#else
    local_aes_xcrypt(&aes_ctx_baseline, &res, length);
#endif
  }
#if (MEASURE == 6)
  riscv_perf_cntr_end();
#endif
#else

  request_mnist_init((uintptr_t) &tensor_cnn1_weight, (uintptr_t) &tensor_cnn2_weight, (uintptr_t) &tensor_linear_bias, (uintptr_t) &tensor_linear_weight);

  do {
    ret = pop(qresp, (void **) &m);
  } while((ret != 0) || (m->f != F_MNIST_INIT));

  for (int i = 0; i < 1024; i++) {
    image_ptr = ((uint8_t *)&images_bin) + i*data_length;
    label = ((uint8_t *)&labels_bin)[i];
    // printm("Client: Sending out image w/ label: %d\n", label);

    // Copy image into scratch space for encryption/sending
    memcpy(&scratch, image_ptr, data_length);

    // Encrypt and send off to enclave
    local_aes_xcrypt(&aes_ctx, &scratch, data_length);

    float output[1][10];
    entry(image_ptr, &output);

    request_mnist(&scratch, data_length, &res);

    do {
      ret = pop(qresp, (void **) &m);
    } while((ret != 0) || (m->f != F_MNIST));

    // Decrypt returned results
    local_aes_xcrypt(&aes_ctx, &res, m->args[0]);

    // printm("Client got label back: %d\n", res);

    if (res == label) { correct++; } else { wrong++; }
  }
#endif
  printm("Correct: %d out of %d\n", correct, correct + wrong);

  request_exit();
  test_completed();
}

void enclave_core(void) {
  // volatile int *flag = (int *) SHARED_MEM_SYNC;
  *flag = STATE_0;

  api_result_t result;

  //uint64_t region1_id = addr_to_region_id((uintptr_t) &region1);
  uint64_t region2_id = addr_to_region_id((uintptr_t) &region2);
  uint64_t region3_id = addr_to_region_id((uintptr_t) &region3);

  // printm("Region block\n");

#if (MEASURE == 5)
  riscv_perf_cntr_begin();
#endif

  result = sm_region_block(region3_id);
  if(result != MONITOR_OK) {
    printm("sm_region_block FAILED with error code %d\n\n", result);
    test_completed();
  }

  // printm("Region block\n");

  result = sm_region_block(region2_id);
  if(result != MONITOR_OK) {
    printm("sm_region_block FAILED with error code %d\n\n", result);
    test_completed();
  }

  *flag = STATE_1;
  printm("Enclave: Flag has been set to %d\n", *flag);
  while(*flag != STATE_2);

  // printm("Region free\n");

  result = sm_region_free(region3_id);
  if(result != MONITOR_OK) {
    printm("sm_region_free FAILED with error code %d\n\n", result);
    test_completed();
  }

  // printm("Region Metadata Create\n");

  result = sm_region_metadata_create(region3_id);
  if(result != MONITOR_OK) {
    printm("sm_region_metadata_create FAILED with error code %d\n\n", result);
    test_completed();
  }

  uint64_t region_metadata_start = sm_region_metadata_start();

  enclave_id = ((uintptr_t) &region3) + (PAGE_SIZE * region_metadata_start);
  uint64_t num_mailboxes = 1;

  // printm("Enclave Create\n");

  result = sm_enclave_create(enclave_id, EVBASE, REGION_MASK, num_mailboxes, true);
  if(result != MONITOR_OK) {
    printm("sm_enclave_create FAILED with error code %d\n\n", result);
    test_completed();
  }

  // printm("Region free\n");

  result = sm_region_free(region2_id);
  if(result != MONITOR_OK) {
    printm("sm_region_free FAILED with error code %d\n\n", result);
    test_completed();
  }

  // printm("Region assign\n");

  result = sm_region_assign(region2_id, enclave_id);
  if(result != MONITOR_OK) {
    printm("sm_region_assign FAILED with error code %d\n\n", result);
    test_completed();
  }

  uintptr_t enclave_handler_address = (uintptr_t) &region2;
  uintptr_t enclave_handler_stack_pointer = enclave_handler_address + HANDLER_LEN + (STACK_SIZE * NUM_CORES);

  // printm("Enclave Load Handler\n");

  result = sm_enclave_load_handler(enclave_id, enclave_handler_address);
  if(result != MONITOR_OK) {
    printm("sm_enclave_load_handler FAILED with error code %d\n\n", result);
    test_completed();
  }

  uintptr_t page_table_address = enclave_handler_stack_pointer;

  // printm("Enclave Load Page Table\n");

  result = sm_enclave_load_page_table(enclave_id, page_table_address, EVBASE, 3, NODE_ACL);
  if(result != MONITOR_OK) {
    printm("sm_enclave_load_page_table FAILED with error code %d\n\n", result);
    test_completed();
  }

  page_table_address += PAGE_SIZE;

  // printm("Enclave Load Page Table\n");

  result = sm_enclave_load_page_table(enclave_id, page_table_address, EVBASE, 2, NODE_ACL);
  if(result != MONITOR_OK) {
    printm("sm_enclave_load_page_table FAILED with error code %d\n\n", result);
    test_completed();
  }

  page_table_address += PAGE_SIZE;

  // printm("Enclave Load Page Table\n");

  result = sm_enclave_load_page_table(enclave_id, page_table_address, EVBASE, 1, NODE_ACL);
  if(result != MONITOR_OK) {
    printm("sm_enclave_load_page_table FAILED with error code %d\n\n", result);
    test_completed();
  }

  uintptr_t phys_addr = page_table_address + PAGE_SIZE;
  uintptr_t untrusted_addr = (uintptr_t) &enclave_start;
  uintptr_t virtual_addr = EVBASE;

  uint64_t size = ((uint64_t) &enclave_end) - ((uint64_t) &enclave_start);
  int num_pages_enclave = size / PAGE_SIZE;

  if((size % PAGE_SIZE) != 0) num_pages_enclave++;

  for(int i = 0; i < num_pages_enclave; i++) {

    // printm("Enclave Load Page\n");

    result = sm_enclave_load_page(enclave_id, phys_addr, virtual_addr, untrusted_addr, LEAF_ACL);
    if(result != MONITOR_OK) {
      printm("sm_enclave_load_page FAILED with error code %d\n\n", result);
      test_completed();
    }

    phys_addr    += PAGE_SIZE;
    untrusted_addr      += PAGE_SIZE;
    virtual_addr += PAGE_SIZE;

  }

  //uintptr_t enclave_sp = virtual_addr;

  uint64_t size_enclave_metadata = sm_enclave_metadata_pages(num_mailboxes);

  thread_id_t thread_id = enclave_id + (size_enclave_metadata * PAGE_SIZE);
  uint64_t timer_limit = 0xeffffffffff;

  // printm("Thread Load\n");

  result = sm_thread_load(enclave_id, thread_id, EVBASE, 0x0, timer_limit); // SP is set by the enclave itself
  if(result != MONITOR_OK) {
    printm("sm_thread_load FAILED with error code %d\n\n", result);
    test_completed();
  }

  // printm("Enclave Init\n");

  result = sm_enclave_init(enclave_id);
  if(result != MONITOR_OK) {
    printm("sm_enclave_init FAILED with error code %d\n\n", result);
    test_completed();
  }

  // Let client thread know we are ready
  while(*flag != STATE_2);
  *flag = STATE_3;
  asm volatile("fence");

  // printm("Enclave Enter\n");

  result = sm_enclave_enter(enclave_id, thread_id);

  send_exit_cmd(0);
  test_completed();
}
