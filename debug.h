#pragma once

#define IN	__attribute__((param_in))
#define OUT	__attribute__((param_out))

#define DEBUG 0
#if DEBUG
  #define DBG_PRINTF(f_, ...) fprintf(stderr, (f_), ##__VA_ARGS__)
#else
  #define DBG_PRINTF(f_, ...)
#endif