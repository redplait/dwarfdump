#include <getopt.h>
#include "ElfFile.h"

extern int g_opt_d, g_opt_l, g_opt_v;
extern FILE *g_outf;

void usage(const char *prog)
{
  printf("%s usage: [options] elf-file\n", prog);
  printf("Options:\n");
  printf("-d - dump debug info\n");
  printf("-l - add levels\n");
  printf("-o out-file\n");
  printf("-v - verbose mode\n");
  exit(6);
}

int main(int argc, char* argv[]) 
{
  FILE *fp = NULL;
  // read options
  while(1)
  {
    int c = getopt(argc, argv, "dlvo:");
    if ( c == -1 )
      break;
    switch(c)
    {
      case 'd': g_opt_d = 1;
        break;
      case 'l': g_opt_l = 1;
        break;
      case 'v': g_opt_v = 1;
        break;
      case 'o':
         if ( fp )
           fclose(fp);
         fp = fopen(optarg, "w");
         if ( NULL == fp )
           fprintf(stderr, "cannot open file %s, error %s", optarg, strerror(errno));
        break;
      default:
        usage(argv[0]);
    }
  }
  if (optind == argc )
    usage(argv[0]);

  bool success;
  std::string binary_path = std::string(argv[optind]);
  ElfFile file(binary_path, success);
  if (!success) {
    return 2;
  }

  // setup g_outf
  g_outf = (fp == NULL) ? stdout : fp;

  file.GetAllClasses();
  std::string json = file.json();
  fprintf(g_outf, "%s\n", json.c_str());

  if ( fp != NULL )
    fclose(fp);

  return 0;
}