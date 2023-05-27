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
#include "al.inc"
};

#include "graph.inc"

static int s_encode = 0;
// replace it with your own flag
                                   //           10        20        30
static unsigned char aes_input[32] = "ReplaceThisWithYour0wnStringL32";
// put to data.inc output of this program with -e
static unsigned char s_dec_out[32] = {
#include "data.inc"
};

void decrypt(std::set<unsigned short> &v)
{
  std::vector<unsigned short> varr;
  std::copy(v.begin(), v.end(), std::back_inserter(varr));
  std::sort(varr.begin(), varr.end());  
  unsigned char tmp_key[20];
  memset(tmp_key, 0, sizeof(tmp_key));
  tmp_key[0] = varr[0] & 0xff;
  tmp_key[1] = varr[1] & 0xff;
  tmp_key[2] = varr[2] & 0xff;
  tmp_key[3] = varr[3] & 0xff;
  tmp_key[4] = (varr[0] & 3) | ((varr[1] & 3) << 2) | ((varr[2] & 3) << 4) | ((varr[3] & 3) << 6);
  tmp_key[5] = varr[4] & 0xff;
  tmp_key[6] = varr[5] & 0xff;
  tmp_key[7] = varr[6] & 0xff;
  tmp_key[8] = varr[7] & 0xff;
  tmp_key[9] = (varr[4] & 3) | ((varr[5] & 3) << 2) | ((varr[6] & 3) << 4) | ((varr[7] & 3) << 6);  
  tmp_key[10] = varr[8] & 0xff;
  tmp_key[11] = varr[9] & 0xff;
  tmp_key[12] = varr[10] & 0xff;
  tmp_key[13] = varr[11] & 0xff;
  tmp_key[14] = (varr[8] & 3) | ((varr[9] & 3) << 2) | ((varr[10] & 3) << 4) | ((varr[11] & 3) << 6);
  tmp_key[15] = varr[12] & 0xff;
  tmp_key[16] = varr[13] & 0xff;
  tmp_key[17] = varr[14] & 0xff;
  tmp_key[18] = varr[15] & 0xff;
  tmp_key[19] = (varr[12] & 3) | ((varr[13] & 3) << 2) | ((varr[14] & 3) << 4) | ((varr[15] & 3) << 6);

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
    if ( args.size() < 16 )
    {
      printf("too short clique\n");
      exit(1);
    }
    decrypt(args);
  }
  exit(2);
}