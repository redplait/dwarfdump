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

// powepc register names ripped from llvm/lib/Target/PowerPC
static const char *const regnames_ppc[] =
{
  /* 0 */ "X0",
  /* 1 */ "X1",
  /* 2 */ "X2",
  /* 3 */ "X3",
  /* 4 */ "X4",
  /* 5 */ "X5",
  /* 6 */ "X6",
  /* 7 */ "X7",
  /* 8 */ "X8",
  /* 9 */ "X9",
  /* 10 */ "X10",
  /* 11 */ "X11",
  /* 12 */ "X12",
  /* 13 */ "X13",
  /* 14 */ "X14",
  /* 15 */ "X15",
  /* 16 */ "X16",
  /* 17 */ "X17",
  /* 18 */ "X18",
  /* 19 */ "X19",
  /* 20 */ "X20",
  /* 21 */ "X21",
  /* 22 */ "X22",
  /* 23 */ "X23",
  /* 24 */ "X24",
  /* 25 */ "X25",
  /* 26 */ "X26",
  /* 27 */ "X27",
  /* 28 */ "X28",
  /* 29 */ "X29",
  /* 30 */ "X30",
  /* 31 */ "X31",
  /* 32 */ "F0",
  /* 33 */ "F1",
  /* 34 */ "F2",
  /* 35 */ "F3",
  /* 36 */ "F4",
  /* 37 */ "F5",
  /* 38 */ "F6",
  /* 39 */ "F7",
  /* 40 */ "F8",
  /* 41 */ "F9",
  /* 42 */ "F10",
  /* 43 */ "F11",
  /* 44 */ "F12",
  /* 45 */ "F13",
  /* 46 */ "F14",
  /* 47 */ "F15",
  /* 48 */ "F16",
  /* 49 */ "F17",
  /* 50 */ "F18",
  /* 51 */ "F19",
  /* 52 */ "F20",
  /* 53 */ "F21",
  /* 54 */ "F22",
  /* 55 */ "F23",
  /* 56 */ "F24",
  /* 57 */ "F25",
  /* 58 */ "F26",
  /* 59 */ "F27",
  /* 60 */ "F28",
  /* 61 */ "F29",
  /* 62 */ "F30",
  /* 63 */ "F31",
  /* 64 */ NULL,
  /* 65 */ "LR8",
  /* 66 */ "CTR8",
  /* 67 */ NULL,
  /* 68 */ "CR0",
  /* 69 */ "CR1",
  /* 70 */ "CR2",
  /* 71 */ "CR3",
  /* 72 */ "CR4",
  /* 73 */ "CR5",
  /* 74 */ "CR6",
  /* 75 */ "CR7",
  /* 76 */ "XER",
  /* 77 */ "VF0",
  /* 78 */ "VF1",
  /* 79 */ "VF2",
  /* 80 */ "VF3",
  /* 81 */ "VF4",
  /* 82 */ "VF5",
  /* 83 */ "VF6",
  /* 84 */ "VF7",
  /* 85 */ "VF8",
  /* 86 */ "VF9",
  /* 87 */ "VF10",
  /* 88 */ "VF11",
  /* 89 */ "VF12",
  /* 90 */ "VF13",
  /* 91 */ "VF14",
  /* 92 */ "VF15",
  /* 93 */ "VF16",
  /* 94 */ "VF17",
  /* 95 */ "VF18",
  /* 96 */ "VF19",
  /* 97 */ "VF20",
  /* 98 */ "VF21",
  /* 99 */ "VF22",
  /* 100 */ "VF23",
  /* 101 */ "VF24",
  /* 102 */ "VF25",
  /* 103 */ "VF26",
  /* 104 */ "VF27",
  /* 105 */ "VF28",
  /* 106 */ "VF29",
  /* 107 */ "VF30",
  /* 108 */ "VF31",
  /* 109 */ "VRSAVE",
};

// mips register names ripped from llvm/lib/Target/Mips
static const char *const regnames_mips[] =
{
  /* 0 */ "ZERO",
  /* 1 */ "AT",
  /* 2 */ "V0",
  /* 3 */ "V1",
  /* 4 */ "A0",
  /* 5 */ "A1",
  /* 6 */ "A2",
  /* 7 */ "A3",
  /* 8 */ "T0",
  /* 9 */ "T1",
  /* 10 */ "T2",
  /* 11 */ "T3",
  /* 12 */ "T4",
  /* 13 */ "T5",
  /* 14 */ "T6",
  /* 15 */ "T7",
  /* 16 */ "S0",
  /* 17 */ "S1",
  /* 18 */ "S2",
  /* 19 */ "S3",
  /* 20 */ "S4",
  /* 21 */ "S5",
  /* 22 */ "S6",
  /* 23 */ "S7",
  /* 24 */ "T8",
  /* 25 */ "T9",
  /* 26 */ "K0",
  /* 27 */ "K1",
  /* 28 */ "GP",
  /* 29 */ "SP",
  /* 30 */ "FP",
  /* 31 */ "RA",
  /* 32 */ "D0",
  /* 33 */ "D1",
  /* 34 */ "D2",
  /* 35 */ "D3",
  /* 36 */ "D4",
  /* 37 */ "D5",
  /* 38 */ "D6",
  /* 39 */ "D7",
  /* 40 */ "D8",
  /* 41 */ "D9",
  /* 42 */ "D10",
  /* 43 */ "D11",
  /* 44 */ "D12",
  /* 45 */ "D13",
  /* 46 */ "D14",
  /* 47 */ "D15",
  /* 48 */ "D16",
  /* 49 */ "D17",
  /* 50 */ "D18",
  /* 51 */ "D19",
  /* 52 */ "D20",
  /* 53 */ "D21",
  /* 54 */ "D22",
  /* 55 */ "D23",
  /* 56 */ "D24",
  /* 57 */ "D25",
  /* 58 */ "D26",
  /* 59 */ "D27",
  /* 60 */ "D28",
  /* 61 */ "D29",
  /* 62 */ "D30",
  /* 63 */ "D31",
  /* 64 */ "HI0",
  /* 65 */ "LO0",  
};

// loongarch register names ripped from llvm/lib/Target/LoongArch
static const char *const regnames_loongarch[] =
{
  /* 0 */ "R0",
  /* 1 */ "R1",
  /* 2 */ "R2",
  /* 3 */ "R3",
  /* 4 */ "R4",
  /* 5 */ "R5",
  /* 6 */ "R6",
  /* 7 */ "R7",
  /* 8 */ "R8",
  /* 9 */ "R9",
  /* 10 */ "R10",
  /* 11 */ "R11",
  /* 12 */ "R12",
  /* 13 */ "R13",
  /* 14 */ "R14",
  /* 15 */ "R15",
  /* 16 */ "R16",
  /* 17 */ "R17",
  /* 18 */ "R18",
  /* 19 */ "R19",
  /* 20 */ "R20",
  /* 21 */ "R21",
  /* 22 */ "R22",
  /* 23 */ "R23",
  /* 24 */ "R24",
  /* 25 */ "R25",
  /* 26 */ "R26",
  /* 27 */ "R27",
  /* 28 */ "R28",
  /* 29 */ "R29",
  /* 30 */ "R30",
  /* 31 */ "R31",
  /* 32 */ "F0",
  /* 33 */ "F1",
  /* 34 */ "F2",
  /* 35 */ "F3",
  /* 36 */ "F4",
  /* 37 */ "F5",
  /* 38 */ "F6",
  /* 39 */ "F7",
  /* 40 */ "F8",
  /* 41 */ "F9",
  /* 42 */ "F10",
  /* 43 */ "F11",
  /* 44 */ "F12",
  /* 45 */ "F13",
  /* 46 */ "F14",
  /* 47 */ "F15",
  /* 48 */ "F16",
  /* 49 */ "F17",
  /* 50 */ "F18",
  /* 51 */ "F19",
  /* 52 */ "F20",
  /* 53 */ "F21",
  /* 54 */ "F22",
  /* 55 */ "F23",
  /* 56 */ "F24",
  /* 57 */ "F25",
  /* 58 */ "F26",
  /* 59 */ "F27",
  /* 60 */ "F28",
  /* 61 */ "F29",
  /* 62 */ "F30",
  /* 63 */ "F31"
};

// sparc register names ripped from llvm/lib/Target/Sparc
static const char *const regnames_sparc[] =
{
  /* 0 */ "G0",
  /* 1 */ "G1",
  /* 2 */ "G2",
  /* 3 */ "G3",
  /* 4 */ "G4",
  /* 5 */ "G5",
  /* 6 */ "G6" ,
  /* 7 */ "G7",
  /* 8 */ "O0",
  /* 9 */ "O1",
  /* 10 */ "O2",
  /* 11 */ "O3",
  /* 12 */ "O4",
  /* 13 */ "O5",
  /* 14 */ "O6",
  /* 15 */ "O7",
  /* 16 */ "L0",
  /* 17 */ "L1",
  /* 18 */ "L2",
  /* 19 */ "L3",
  /* 20 */ "L4",
  /* 21 */ "L5",
  /* 22 */ "L6",
  /* 23 */ "L7",
  /* 24 */ "I0",
  /* 25 */ "I1",
  /* 26 */ "I2",
  /* 27 */ "I3",
  /* 28 */ "I4",
  /* 29 */ "I5",
  /* 30 */ "I6",
  /* 31 */ "I7",
  /* 32 */ "F0",
  /* 33 */ "F1",
  /* 34 */ "F2",
  /* 35 */ "F3",
  /* 36 */ "F4",
  /* 37 */ "F5",
  /* 38 */ "F6",
  /* 39 */ "F7",
  /* 40 */ "F8",
  /* 41 */ "F9",
  /* 42 */ "F10",
  /* 43 */ "F11",
  /* 44 */ "F12",
  /* 45 */ "F13",
  /* 46 */ "F14",
  /* 47 */ "F15",
  /* 48 */ "F16",
  /* 49 */ "F17",
  /* 50 */ "F18",
  /* 51 */ "F19",
  /* 52 */ "F20",
  /* 53 */ "F21",
  /* 54 */ "F22",
  /* 55 */ "F23",
  /* 56 */ "F24",
  /* 57 */ "F25",
  /* 58 */ "F26",
  /* 59 */ "F27",
  /* 60 */ "F28",
  /* 61 */ "F29",
  /* 62 */ "F30",
  /* 63 */ "F31",
  /* 64 */ "Y",
  /* 65 */ NULL,
  /* 66 */ NULL,
  /* 67 */ NULL,
  /* 68 */ NULL,
  /* 69 */ NULL,
  /* 70 */ NULL,
  /* 71 */ NULL,
  /* 72 */ "D0",
  /* 73 */ "D1",
  /* 74 */ "D2",
  /* 75 */ "D3",
  /* 76 */ "D4",
  /* 77 */ "D5",
  /* 78 */ "D6",
  /* 79 */ "D7",
  /* 80 */ "D8",
  /* 81 */ "D9",
  /* 82 */ "D10",
  /* 83 */ "D11",
  /* 84 */ "D12",
  /* 85 */ "D13",
  /* 86 */ "D14",
  /* 87 */ "D15",
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
    case ELFIO::EM_PPC:
    case ELFIO::EM_PPC64: // TODO - perhaps they have different registers set?
       res = new tableRegNames(regnames_ppc, ARRAY_SIZE(regnames_ppc));
       return res;
      break;
    case ELFIO::EM_MIPS:
       res = new tableRegNames(regnames_mips, ARRAY_SIZE(regnames_mips));
       return res;
      break;
    case ELFIO::EM_SPARCV9:
       res = new tableRegNames(regnames_sparc, ARRAY_SIZE(regnames_sparc));
       return res;
      break;
    case 258:
       res = new tableRegNames(regnames_loongarch, ARRAY_SIZE(regnames_loongarch));
       return res;
      break;
  } 
  return nullptr; 
}