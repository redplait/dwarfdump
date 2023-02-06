#include <getopt.h>
#include "ElfFile.h"

extern int g_opt_d;
extern FILE *g_outf;

void usage(const char *prog)
{
  printf("%s usage: [options] elf-file\n", prog);
  printf("Options:\n");
  printf("-d - dump debug info\n");
  printf("-o out-file\n");
  exit(6);
}

int main(int argc, char* argv[]) 
{
  FILE *fp = NULL;
  // read options
  while(1)
  {
    int c = getopt(argc, argv, "do:");
    if ( c == -1 )
      break;
    switch(c)
    {
      case 'd': g_opt_d = 1;
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