#include <getopt.h>
#include "ElfFile.h"
#include "JsonRender.h"
#include "PlainRender.h"
#include "nfilter.h"

extern int g_opt_d, g_opt_f, g_opt_F, g_opt_g, g_opt_l, g_opt_L, g_opt_s, g_opt_v, g_opt_V, g_opt_x, g_opt_z;
extern FILE *g_outf;

int use_json = 0;

void usage(const char *prog)
{
  printf("%s usage: [options] elf-file\n", prog);
  printf("Options:\n");
  printf("-d - dump debug info\n");
  printf("-f - add functions\n");
  printf("-F - dump file names for decl_file attribute\n");
  printf("-g - dump all elements in last pass\n");
  printf("-I - original elf file for .debug\n");
  printf("-j - produce json\n");
  printf("-k - keep already dumped types\n");
  printf("-l - add levels\n");
  printf("-L - process lexical blocks\n");
  printf("-N - filter file name\n");
  printf("-o out-file\n");
  printf("-s - dump section names\n");
  printf("-v - verbose mode\n");
  printf("-V - dump vars\n");
  printf("-x - dump local vars and locations. Also turns on -L & -V\n");
  printf("-z - dump uncompressed sections\n");
  exit(6);
}

int main(int argc, char* argv[]) 
{
  FILE *fp = NULL;
  std::string iname;
  // read options
  while(1)
  {
    int c = getopt(argc, argv, "dfFgjklLsvVxo:I:N:");
    if ( c == -1 )
      break;
    switch(c)
    {
      case 'd': g_opt_d = 1;
        break;
      case 'f': g_opt_f = 1;
        break;
      case 'F': g_opt_F = 1;
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
      case 's': g_opt_s = 1;
        break;
      case 'v': g_opt_v = 1;
        break;
      case 'V': g_opt_V = 1;
        break;
      case 'x': g_opt_x = g_opt_V = g_opt_L = 1;
        break;
      case 'z': g_opt_z = 1;
        break;
      case 'o':
         if ( fp )
           fclose(fp);
         fp = fopen(optarg, "w");
         if ( NULL == fp )
           fprintf(stderr, "cannot open file %s, error %s", optarg, strerror(errno));
        break;
      case 'I':
         iname = optarg;
        break;
      case 'N':
         add_filter(optarg);
        break;
      default:
        usage(argv[0]);
    }
  }
  if (optind == argc )
    usage(argv[0]);

  FLog ferr(stderr);
  TreeBuilder *render = nullptr;
  if ( use_json )
    render = new JsonRender(&ferr);
  else
    render = new PlainRender(&ferr);
  bool success;
  std::string binary_path = std::string(argv[optind]);
  {
    ElfReaderOwner file(binary_path, success, render);
    if (!success) {
      fprintf(stderr, "cannot load %s\n", argv[optind]);
      delete render;
      return 2;
    }

    // save sections for original stripped elf file
    if ( !iname.empty() )
      file.SaveSections(iname);

    if ( g_opt_x || g_opt_f )
      render->m_locX = (IGetLoclistX *)&file;

    // setup g_outf
    g_outf = (fp == NULL) ? stdout : fp;

    if ( use_json )
      fprintf(g_outf, "{");
    file.GetAllClasses();
    if ( use_json )
      fprintf(g_outf, "}\n");
  }

  if ( fp != NULL )
    fclose(fp);
  delete render;
  return 0;
}