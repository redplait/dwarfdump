#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
  int f = 0, s = 0;
  if ( argc < 2 )
    return 0;
  if ( argc < 3 )
    goto skip3;
  s = atoi(argv[2]);
skip3:
  f = atoi(argv[1]);
  switch(f)
  {
    case 0:
      printf("nil");
      break;
    case 1: {
      if ( argc > 2 )
      {
        switch(s)
        {
          case 1:
            printf("elf");
            break;
          case 2:
            printf("zvolf");
            break;
          case 3:
            printf("unluck");
          default:
            printf("dofiga");
        }
      } else
       printf("ain");
      break;
    }
    case 2:
      printf("zwai");
      break;
    case 3:
      printf("dry");
      break;
    case 4:
      printf("v1er");
      break;
    case 5:
      printf("funv");
      break;
    case 6:
      printf("$ex");
      break;
    case 7:
      printf("seben");
      break;
    case 8:
      printf("aht");
      break;
    case 9:
      printf("nain");
      break;
  }
  return f;
}