#include "platform_control_spec.h"

void* memcpy_shm(void* dest, const void* src, size_t len)
{
  const char* s = src;
  char *d = dest;
  char *end = src + len;

  if (s >= end)
    return dest;

  platform_disable_predictors();
  while (s < end)
    *d++ = *s++;
  platform_enable_predictors();

  return dest;
}