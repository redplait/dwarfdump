#include <stdio.h>

// crazy sample ripped from https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html
int do_madness(int idx)
{
  static const int array[] = { &&foo - &&foo, &&bar - &&foo,
                             &&hack - &&foo };
  if ( idx > 2 ) {
    printf("dont know\n");
    return 0;
  }
  goto *(&&foo + array[idx]);
foo:
  printf("foo called\n");
  return 1;
bar:
  printf("bar called\n");
  return 2;
hack:
  printf("hack called\n");
  return 3;
}

int main(int argc)
{
  return do_madness(argc - 1);
}