#include <stdio.h>

struct state {
  int idx;
  int arr[12];
};

__thread state tls_state;
static state *s_state = nullptr;

int use_global(int idx)
{
  if ( idx < sizeof(tls_state.arr[0]) / sizeof(tls_state.arr[0]) )
  {
    s_state->arr[idx] = ++tls_state.idx;
  }
  return -1;
}