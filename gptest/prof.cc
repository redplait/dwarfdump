#include <stdlib.h>
#include <stdio.h>
#include "eread.h"
#include "lditer.h"
#include "prof.h"

extern "C" void _mcleanup (void);
extern "C" void mcount(void);
extern "C" void monstartup (char *, char *);

static char s_prefix[250] = { 0 };

int start_prof(const char *full_name, const char *target)
{
  struct prof_data pd;
  if ( process_elf(full_name, &pd) <= 0 ) return 0;
  if ( cmp_sonames(full_name, target) )
  {
    printf("[+] %s loaded", target);
    ld_data ld;
    ld.name = target;
    if ( ld_iter(&ld) )
    {
      printf(" at %p, x_start %p x_size %lX\n", ld.base, ld.x_start, ld.x_size);
      sprintf(s_prefix, "gmon.%lX", ld.base);
      if ( pd.m_mcount )
      {
        // this library was compiled with -pg option - so we can just call monstartup with right address range
        monstartup(ld.base, ld.x_start + ld.x_size);
        return 1;
      } else if ( pd.m_func_enter )
      {
        // -finstrument-functions - patch __cyg_profile_func_enter to ncount and call monstartup
        void **iat = (void **)(ld.base + pd.m_func_enter);
        printf("[+] patch func_enter at %p\n", iat);
        void *real_m = (void *)&mcount;
        *iat = real_m;
        monstartup(ld.base, ld.x_start + ld.x_size);
        return 1;
      } else {
        printf("[-] your library %s is not profileable\n", full_name);
        return 0;
      }
    } else
      printf(" ld_iter failed\n");
  } else
    printf(" cannot find %s\n", target);
  return 0;
}

void stop_prof(void)
{
  setenv("GMON_OUT_PREFIX", s_prefix, 1);
  _mcleanup();
}