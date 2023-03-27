#include <getopt.h>
#include "ElfFile.h"
#include "JsonRender.h"
#include "PlainRender.h"

extern int g_opt_d, g_opt_f, g_opt_g, g_opt_l, g_opt_L, g_opt_v, g_opt_V;
extern FILE *g_outf;

int use_json = 0;

void usage(const char *prog)
{
  printf("%s usage: [options] elf-file\n", prog);
  printf("Options:\n");
  printf("-d - dump debug info\n");
  printf("-f - add functions\n");
  printf("-g - dump all elements in last pass\n");
  printf("-j - produce json\n");
  printf("-k - keep already dumped types\n");
  printf("-l - add levels\n");
  printf("-L - process lexical blocks\n");
  printf("-o out-file\n");
  printf("-v - verbose mode\n");
  printf("-V - dump vars\n");
  exit(6);
}

int main(int argc, char* argv[]) 
{
  FILE *fp = NULL;
  // read options
  while(1)
  {
    int c = getopt(argc, argv, "dfgjklLvVo:");
    if ( c == -1 )
      break;
    switch(c)
    {
      case 'd': g_opt_d = 1;
        break;
      case 'f': g_opt_f = 1;
        break;
      case 'j': use_json = 1;
        break; 
      case 'g': g_opt_g = 1;
        break;
      case 'k': g_opt_k = 1;
        break;
      case 'l': g_opt_l = 1;
        break;
      case 'L': g_opt_L = 1;
        break;
      case 'v': g_opt_v = 1;
        break;
      case 'V': g_opt_V = 1;
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

  TreeBuilder *render = nullptr;
  if ( use_json )
    render = new JsonRender();
  else
    render = new PlainRender();
  bool success;
  std::string binary_path = std::string(argv[optind]);
  ElfFile file(binary_path, success, render);
  if (!success) {
    fprintf(stderr, "cannot load %s\n", argv[optind]);
    delete render;
    return 2;
  }

  // setup g_outf
  g_outf = (fp == NULL) ? stdout : fp;

  if ( use_json )
    fprintf(g_outf, "{");
  file.GetAllClasses();
  if ( use_json )
    fprintf(g_outf, "}\n");

  if ( fp != NULL )
    fclose(fp);
  delete render;
  return 0;
}