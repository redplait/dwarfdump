#include <stdio.h>
#include <string.h>

struct test_s
{
  // unnamed nested structure
  struct {
    int idx;
    int value;
  } v;
  int arr[10];
};

void w1(struct test_s *s)
{
  s->arr[s->v.idx] = s->v.value;
  printf("idx %d value %d\n", s->v.idx, s->v.value);
}

void w2(struct test_s *s)
{
  memset(s->arr, 0, sizeof(s->arr));
  s->arr[s->v.idx] = s->v.value;
  printf("idx %d value %d\n", s->v.idx, s->v.value);
}

typedef void (*pfn)(struct test_s *);

const pfn workers[2] = { w1, w2 };

int main(int argc, char **argv)
{
  struct test_s s1;
  if ( argc > 1 )
  {
    s1.v.idx = 0;
    s1.v.value = 1;
    workers[0](&s1);
  } else {
    s1.v.idx = 1;
    s1.v.value = 2;
    workers[1](&s1);
  }
};