#include <stdio.h>

struct dumb_item;

typedef int (*worker)(const dumb_item&, int);

// test for strict array
struct dumb_item
{
  int i;
  worker pfn;
  dumb_item(): i(0), pfn(nullptr)
  {}
};

int add(const dumb_item &di, int v)
{
  return v + di.i;
}

int sub(const dumb_item &di, int v)
{
  return di.i - v;
}

int mul(const dumb_item &di, int v)
{
  return v * di.i;
}

int div(const dumb_item &di, int v)
{
  return di.i / v;
}

struct aggr
{
  int res;
  struct dumb_item ops[4];
  aggr()
  {
    res = 0;
    ops[0].pfn = add;
    ops[1].pfn = sub;
    ops[2].pfn = mul;
    ops[3].pfn = div;
  }
};

int main()
{
  aggr a;
  for ( int i = 0; i < 4; ++i )
  {
    a.res += (a.ops[i].pfn)(a.ops[i], 8 - i);
  }
  printf("res %d\n", a.res);
}