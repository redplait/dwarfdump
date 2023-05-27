#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <set>
#include <vector>
#include <map>
#include <algorithm>
#include <openssl/aes.h>
#include <openssl/sha.h>

struct one_edge {
  unsigned short e1, e2;
};

const one_edge data[] = {
#include "al2.inc"
};

#include "graph.inc"

static int s_encode = 0;
// replace it with your own flag
                                   //           10        20        30
static unsigned char aes_input[32] = "ReplaceThisWithYour0wnStringL32";
// put to data.inc output of this program with -e
static unsigned char s_dec_out[32] = {
#include "data2.inc"
};

void decrypt(std::set<unsigned short> &v)
{
  std::vector<unsigned short> varr;
  std::copy(v.begin(), v.end(), std::back_inserter(varr));
  std::sort(varr.begin(), varr.end());
  // 20 vertices 12 bit = 240 bits / 8 = 30 bytes  
  unsigned char tmp_key[30];
  memset(tmp_key, 0, sizeof(tmp_key));
#define FILL3(i1, i2) \
  tmp_key[i1] = varr[i2] & 0xff; \
  tmp_key[i1+1] = (varr[i2+1] >> 4) & 0xff; \
  tmp_key[i1+2] = (varr[i2+1] & 0xf) | ((varr[i2] >> 4) & 0xff);

  FILL3(0, 0)
  FILL3(3, 2)
  FILL3(6, 4)
  FILL3(9, 6)
  FILL3(12, 8)
  FILL3(15, 10)
  FILL3(18, 12)
  FILL3(21, 14)
  FILL3(24, 16)
  FILL3(27, 18)

#include "aes.inc"
}

int main(int argc, char **argv)
{
  read_graph();
  int start = 1;
  if ( argc > 1 && !strcmp(argv[1], "-e") )
  {
     start++;
     s_encode = 1;
  }
  std::set<unsigned short> args;
  for ( int i = start; i < argc; i++ )
  {
    char *tmp = NULL;
    auto v = strtol(argv[i], &tmp, 10);
    args.insert((unsigned short)v);
  }
  if ( is_clique(args) )
  {
    if ( args.size() < 20 )
    {
      printf("too short clique\n");
      exit(1);
    }
    decrypt(args);
  }
  exit(2);
}