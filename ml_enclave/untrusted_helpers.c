#include <untrusted_util.h>
#include <api_untrusted.h>
#include "local_cryptography.h"

bool verify_attestation(enclave_id_t enclave_id) {
  api_result_t result;

  public_key_t sm_pk;
  hash_t enclave_measurement;
  public_key_t enclave_pk;
  signature_t enclave_attestation;

  do {
    result = sm_get_public_field(PUBLIC_FIELD_PK_SM, &sm_pk);
  } while (result != MONITOR_OK);

  do {
    result = sm_enclave_get_attest(enclave_id, &enclave_measurement, &enclave_pk, &enclave_attestation);
  } while (result != MONITOR_OK) ;

  // Compute H(enclave_measurement | enclave_pk)
  hash_t my_attestation;
  hash_context_t my_hash_ctx;
  local_hash_init(&my_hash_ctx);
  local_hash_extend(&my_hash_ctx, &enclave_measurement, sizeof(hash_t));
  local_hash_extend(&my_hash_ctx, &enclave_pk, sizeof(public_key_t));
  local_hash_finalize(&my_hash_ctx, &my_attestation);

  // printm("enclave_attestation = [");
  // for (int i = 0; i < sizeof(signature_t); i++) {
  //   printm("0x%x, ", enclave_attestation.bytes[i]);
  // }
  // printm("]\n");
  // printm("\n");

  // printm("enclave_measurement = [");
  // for (int i = 0 ; i < sizeof(hash_t); i++) {
  //   printm("0x%x, ", enclave_measurement.bytes[i]);
  // }
  // printm("]\n");
  // printm("\n");

  // printm("enclave_pk = [");
  // for (int i = 0; i < sizeof(public_key_t); i++) {
  //   printm("0x%x, ", enclave_pk.bytes[i]);
  // }
  // printm("]\n");
  // printm("\n");


  // printm("sm_pk = [");
  // for (int i = 0; i < sizeof(public_key_t); i++) {
  //   printm("0x%x, ", sm_pk.bytes[i]);
  // }
  // printm("]\n");
  // printm("\n");

  // printm("my_attestation = ");
  // for (int i = 0; i < sizeof(hash_t); i++) {
  //   printm("0x%x, ", my_attestation.bytes[i]);
  // }
  // printm("]\n");
  // printm("\n");

  return local_verify(&enclave_attestation, &my_attestation, sizeof(hash_t), &sm_pk);
}