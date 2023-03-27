#include "regnames.h"

struct tableRegNames: public RegNames
{
  tableRegNames(const char * const* tab, size_t size)
   : tab_(tab),
     tab_size(size)
  {}
  virtual const char *reg_name(unsigned int regno)
  {
    if ( regno < tab_size )
      return tab_[regno];
    return NULL;
  }
  const char * const* tab_;
  size_t tab_size;
};

// most of code ripped from binutils dwarf.c
static const char *const dwarf_regnames_i386[] =
{
  "eax", "ecx", "edx", "ebx",			  /* 0 - 3  */
  "esp", "ebp", "esi", "edi",			  /* 4 - 7  */
  "eip", "eflags", NULL,			  /* 8 - 10  */
  "st0", "st1", "st2", "st3",			  /* 11 - 14  */
  "st4", "st5", "st6", "st7",			  /* 15 - 18  */
  NULL, NULL,					  /* 19 - 20  */
  "xmm0", "xmm1", "xmm2", "xmm3",		  /* 21 - 24  */
  "xmm4", "xmm5", "xmm6", "xmm7",		  /* 25 - 28  */
  "mm0", "mm1", "mm2", "mm3",			  /* 29 - 32  */
  "mm4", "mm5", "mm6", "mm7",			  /* 33 - 36  */
  "fcw", "fsw", "mxcsr",			  /* 37 - 39  */
  "es", "cs", "ss", "ds", "fs", "gs", NULL, NULL, /* 40 - 47  */
  "tr", "ldtr",					  /* 48 - 49  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 50 - 57  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 58 - 65  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 66 - 73  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 74 - 81  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 82 - 89  */
  NULL, NULL, NULL,				  /* 90 - 92  */
  "k0", "k1", "k2", "k3", "k4", "k5", "k6", "k7"  /* 93 - 100  */
};

static const char *const dwarf_regnames_iamcu[] =
{
  "eax", "ecx", "edx", "ebx",			  /* 0 - 3  */
  "esp", "ebp", "esi", "edi",			  /* 4 - 7  */
  "eip", "eflags", NULL,			  /* 8 - 10  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 11 - 18  */
  NULL, NULL,					  /* 19 - 20  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 21 - 28  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 29 - 36  */
  NULL, NULL, NULL,				  /* 37 - 39  */
  "es", "cs", "ss", "ds", "fs", "gs", NULL, NULL, /* 40 - 47  */
  "tr", "ldtr",					  /* 48 - 49  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 50 - 57  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 58 - 65  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 66 - 73  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 74 - 81  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 82 - 89  */
  NULL, NULL, NULL,				  /* 90 - 92  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL  /* 93 - 100  */
};

static const char *const dwarf_regnames_x86_64[] =
{
  "rax", "rdx", "rcx", "rbx",
  "rsi", "rdi", "rbp", "rsp",
  "r8",  "r9",  "r10", "r11",
  "r12", "r13", "r14", "r15",
  "rip",
  "xmm0",  "xmm1",  "xmm2",  "xmm3",
  "xmm4",  "xmm5",  "xmm6",  "xmm7",
  "xmm8",  "xmm9",  "xmm10", "xmm11",
  "xmm12", "xmm13", "xmm14", "xmm15",
  "st0", "st1", "st2", "st3",
  "st4", "st5", "st6", "st7",
  "mm0", "mm1", "mm2", "mm3",
  "mm4", "mm5", "mm6", "mm7",
  "rflags",
  "es", "cs", "ss", "ds", "fs", "gs", NULL, NULL,
  "fs.base", "gs.base", NULL, NULL,
  "tr", "ldtr",
  "mxcsr", "fcw", "fsw",
  "xmm16",  "xmm17",  "xmm18",  "xmm19",
  "xmm20",  "xmm21",  "xmm22",  "xmm23",
  "xmm24",  "xmm25",  "xmm26",  "xmm27",
  "xmm28",  "xmm29",  "xmm30",  "xmm31",
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 83 - 90  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 91 - 98  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 99 - 106  */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 107 - 114  */
  NULL, NULL, NULL,				  /* 115 - 117  */
  "k0", "k1", "k2", "k3", "k4", "k5", "k6", "k7"
};

static const char *const dwarf_regnames_aarch64[] =
{
   "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
   "x8",  "x9", "x10", "x11", "x12", "x13", "x14", "x15",
  "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
  "x24", "x25", "x26", "x27", "x28", "x29", "x30", "sp",
   NULL, "elr",  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
   NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  "vg", "ffr",
   "p0",  "p1",  "p2",  "p3",  "p4",  "p5",  "p6",  "p7",
   "p8",  "p9", "p10", "p11", "p12", "p13", "p14", "p15",
   "v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",
   "v8",  "v9", "v10", "v11", "v12", "v13", "v14", "v15",
  "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
  "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
   "z0",  "z1",  "z2",  "z3",  "z4",  "z5",  "z6",  "z7",
   "z8",  "z9", "z10", "z11", "z12", "z13", "z14", "z15",
  "z16", "z17", "z18", "z19", "z20", "z21", "z22", "z23",
  "z24", "z25", "z26", "z27", "z28", "z29", "z30", "z31",
};

static const char *const dwarf_regnames_s390[] =
{
  /* Avoid saying "r5 (r5)", so omit the names of r0-r15.  */
  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
  "f0",  "f2",  "f4",  "f6",  "f1",  "f3",  "f5",  "f7",
  "f8",  "f10", "f12", "f14", "f9",  "f11", "f13", "f15",
  "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7",
  "cr8", "cr9", "cr10", "cr11", "cr12", "cr13", "cr14", "cr15",
  "a0",  "a1",  "a2",  "a3",  "a4",  "a5",  "a6",  "a7",
  "a8",  "a9",  "a10", "a11", "a12", "a13", "a14", "a15",
  "pswm", "pswa",
  NULL, NULL,
  "v16", "v18", "v20", "v22", "v17", "v19", "v21", "v23",
  "v24", "v26", "v28", "v30", "v25", "v27", "v29", "v31",
};

#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))

RegNames *get_regnames(ELFIO::Elf_Half mac)
{
  RegNames *res = nullptr;
  switch(mac)
  {
    case ELFIO::EM_386:
       res = new tableRegNames(dwarf_regnames_i386, ARRAY_SIZE(dwarf_regnames_i386));
       return res;
      break;
    case ELFIO::EM_486: // 6
       res = new tableRegNames(dwarf_regnames_iamcu, ARRAY_SIZE(dwarf_regnames_iamcu));
       return res;
      break;
    case ELFIO::EM_X86_64:
    case 180: // EM_L1OM
    case 181: // EM_K1OM
       res = new tableRegNames(dwarf_regnames_x86_64, ARRAY_SIZE(dwarf_regnames_x86_64));
       return res;
      break;
    case ELFIO::EM_AARCH64:
       res = new tableRegNames(dwarf_regnames_aarch64, ARRAY_SIZE(dwarf_regnames_aarch64));
       return res;
      break;
    case ELFIO::EM_S390:
       res = new tableRegNames(dwarf_regnames_s390, ARRAY_SIZE(dwarf_regnames_s390));
       return res;
      break;
  } 
  return nullptr; 
}