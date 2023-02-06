#pragma once

#define DEBUG 0
#if DEBUG
  #define DBG_PRINTF(f_, ...) fprintf(stderr, (f_), ##__VA_ARGS__)
#else
  #define DBG_PRINTF(f_, ...)
#endif