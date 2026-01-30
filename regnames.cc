#include "regnames.h"

struct CudaRegNames: public RegNames
{
  typedef enum {
    REG_CLASS_INVALID                  = 0x000,   /* invalid register */
    REG_CLASS_REG_CC                   = 0x001,   /* Condition register */
    REG_CLASS_REG_PRED                 = 0x002,   /* Predicate register */
    REG_CLASS_REG_ADDR                 = 0x003,   /* Address register */
    REG_CLASS_REG_HALF                 = 0x004,   /* 16-bit register (Currently unused) */
    REG_CLASS_REG_FULL                 = 0x005,   /* 32-bit register */
    REG_CLASS_MEM_LOCAL                = 0x006,   /* register spilled in memory */
    REG_CLASS_LMEM_REG_OFFSET          = 0x007,   /* register at stack offset (ABI only) */
    REG_CLASS_UREG_PRED                = 0x009,   /* uniform predicate register */
    REG_CLASS_UREG_HALF                = 0x00a,   /* 16-bit uniform register */
    REG_CLASS_UREG_FULL                = 0x00b,   /* 32-bit uniform register */
  } CUDBGRegClass;
  char reg[16], ureg[17], pred[10], upred[11];
  unsigned int REGMAP_CLASS(unsigned int x) { return x >> 24; }
  unsigned int REGMAP_REG(unsigned int x) { return x & 0x00ffffff; }
  unsigned int REGMAP_PRED(unsigned int x) { return x & 0x07; }
  virtual const char *reg_name(unsigned int regno)
  {
    if ( REGMAP_CLASS (regno) == REG_CLASS_REG_FULL ) {
      auto r = REGMAP_REG (regno);
      if ( r == 255 ) return "RZ";
      snprintf(reg, sizeof(reg) - 1, "R%d", r);
      return reg;
    }
    if ( REGMAP_CLASS (regno) == REG_CLASS_UREG_FULL ) {
      auto r = REGMAP_REG (regno);
      if ( r == 255 ) return "URZ";
      snprintf(ureg, sizeof(ureg) - 1, "UR%d", r);
      return ureg;
    }
    if ( REGMAP_CLASS (regno) == REG_CLASS_REG_PRED ) {
      snprintf(pred, sizeof(pred) - 1, "P%d", REGMAP_PRED(regno));
      return pred;
    }
    if ( REGMAP_CLASS (regno) == REG_CLASS_UREG_PRED ) {
      snprintf(upred, sizeof(upred) - 1, "UP%d", REGMAP_PRED(regno));
      return upred;
    }
    // half regs - for hi part add suffix .hi, for low .lo
    if ( REGMAP_CLASS (regno) == REG_CLASS_REG_HALF ) {
     auto raw = REGMAP_REG (regno);
     snprintf(reg, sizeof(reg) - 1, "R%d,%s", raw / 2, raw & 1 ? "hi" : "lo");
     return reg;
    }
    if ( REGMAP_CLASS (regno) == REG_CLASS_UREG_HALF ) {
     auto raw = REGMAP_REG (regno);
     snprintf(ureg, sizeof(ureg) - 1, "UR%d,%s", raw / 2, raw & 1 ? "hi" : "lo");
     return ureg;
    }
    return nullptr;
  }
};

struct ArmRegNames: public RegNames
{
  virtual const char *reg_name(unsigned int regno)
  {
    switch(regno)
    {
     case 0: return "R0";
     case 1: return "R1";
     case 2: return "R2";
     case 3: return "R3";
     case 4: return "R4";
     case 5: return "R5";
     case 6: return "R6";
     case 7: return "R7";
     case 8: return "R8";
     case 9: return "R9";
     case 10: return "R10";
     case 11: return "R11";
     case 12: return "R12";
     case 13: return "SP";
     case 14: return "LR";
     case 15: return "ZR";
     case 143: return "RA_AUTH_CODE";
     case 256: return "D0";
     case 257: return "D1";
     case 258: return "D2";
     case 259: return "D3";
     case 260: return "D4";
     case 261: return "D5";
     case 262: return "D6";
     case 263: return "D7";
     case 264: return "D8";
     case 265: return "D9";
     case 266: return "D10";
     case 267: return "D11";
     case 268: return "D12";
     case 269: return "D13";
     case 270: return "D14";
     case 271: return "D15";
     case 272: return "D16";
     case 273: return "D17";
     case 274: return "D18";
     case 275: return "D19";
     case 276: return "D20";
     case 277: return "D21";
     case 278: return "D22";
     case 279: return "D23";
     case 280: return "D24";
     case 281: return "D25";
     case 282: return "D26";
     case 283: return "D27";
     case 284: return "D28";
     case 285: return "D29";
     case 286: return "D30";
     case 287: return "D31";
    }
    return NULL;
  }
};

typedef const char *(*addr_type)(unsigned int);

struct tableRegNames: public RegNames
{
  tableRegNames(const char * const* tab, size_t size)
   : tab_(tab),
     tab_size(size),
     addr_type_(nullptr)
  {}
  tableRegNames(const char * const* tab, size_t size, addr_type a)
   : tab_(tab),
     tab_size(size),
     addr_type_(a)
  {}
  virtual const char *reg_name(unsigned int regno)
  {
    if ( regno < tab_size )
      return tab_[regno];
    return NULL;
  }
  virtual const char *get_addr_type(unsigned int regno)
  {
    if ( addr_type_ )
      return addr_type_(regno);
    return nullptr;
  }
  const char * const* tab_;
  size_t tab_size;
  addr_type addr_type_;
};

#define DW_ADDR_near16 1 /* 16-bit offset, no segment */
#define DW_ADDR_far16  2 /* 16-bit offset, 16-bit segment */
#define DW_ADDR_huge16 3 /* 16-bit offset, 16-bit segment */
#define DW_ADDR_near32 4 /* 32-bit offset, no segment */
#define DW_ADDR_far32  5 /* 32-bit offset, 16-bit segm */

const char *i386_addr_type(unsigned int v)
{
  switch(v)
  {
    case DW_ADDR_near16: return "near16";
    case DW_ADDR_far16:  return "far16";
    case DW_ADDR_huge16: return "huge16";
    case DW_ADDR_near32: return "near32";
    case DW_ADDR_far32:  return "far32";
  }
  return nullptr;
}

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

const char *s390_addr_type(unsigned int v)
{
  if ( v & (1 << 4) )
   return "mode32";
  return nullptr;
}


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

const char *ft32_addr_type(unsigned int v)
{
  if ( v & (1 << 4) )
   return "flash";
  return nullptr;
}

static const char *const dwarf_regnames_arc[] = {
  "r0",   "r1",   "r2",   "r3",       "r4",     "r5",     "r6",    "r7",
  "r8",   "r9",  "r10",  "r11",      "r12",    "r13",    "r14",   "r15",
  "r16",  "r17",  "r18",  "r19",      "r20",    "r21",    "r22",   "r23",
  "r24",  "r25",   "gp",   "fp",       "sp",  "ilink",  "ilink2", "blink",
  "r32",  "r33",  "r34",  "r35",      "r36",    "r37",    "r38",   "r39",
  "d1",   "d1",   "d2",   "d2",      "r44",    "r45",    "r46",   "r47",
  "r48",  "r49",  "r50",  "r51",      "r52",    "r53",    "r54",   "r55",
  "r56",  "r57", "r58",  "r59",  "lp_count",    "cc",   "limm",   "pcl",
  "vr0",  "vr1",  "vr2",  "vr3",      "vr4",    "vr5",    "vr6",   "vr7",
  "vr8",  "vr9", "vr10", "vr11",     "vr12",   "vr13",   "vr14",  "vr15",
 "vr16", "vr17", "vr18", "vr19",     "vr20",   "vr21",   "vr22",  "vr23",
 "vr24", "vr25", "vr26", "vr27",     "vr28",   "vr29",   "vr30",  "vr31",
 "vr32", "vr33", "vr34", "vr35",     "vr36",   "vr37",   "vr38",  "vr39",
 "vr40", "vr41", "vr42", "vr43",     "vr44",   "vr45",   "vr46",  "vr47",
 "vr48", "vr49", "vr50", "vr51",     "vr52",   "vr53",   "vr54",  "vr55",
 "vr56", "vr57", "vr58", "vr59",     "vr60",   "vr61",   "vr62",  "vr63",
  "dr0",  "dr1",  "dr2",  "dr3",      "dr4",    "dr5",    "dr6",   "dr7",
  "dr0",  "dr1",  "dr2",  "dr3",      "dr4",    "dr5",    "dr6",   "dr7",
  "arg", "frame"
};

static const char *const dwarf_regnames_bfin[] = {
  "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
  "P0", "P1", "P2", "P3", "P4", "P5", "SP", "FP",
  "I0", "I1", "I2", "I3", "B0", "B1", "B2", "B3",
  "L0", "L1", "L2", "L3", "M0", "M1", "M2", "M3",
  "A0", "A1",
  "CC",
  "RETS", "RETI", "RETX", "RETN", "RETE", "ASTAT", "SEQSTAT", "USP",
  "ARGP",
  "LT0", "LT1", "LC0", "LC1", "LB0", "LB1"
};

static const char *const dwarf_regnames_c6x[] = {
    "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
    "A8", "A9", "A10", "A11", "A12", "A13", "A14", "A15",
    "A16", "A17", "A18", "A19", "A20", "A21", "A22", "A23",
    "A24", "A25", "A26", "A27", "A28", "A29", "A30", "A31",
    "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7",
    "B8", "B9", "B10", "B11", "B12", "B13", "B14", "B15",
    "B16", "B17", "B18", "B19", "B20", "B21", "B22", "B23",
    "B24", "B25", "B26", "B27", "B28", "B29", "B30", "B31",
    "FP", "ARGP", "ILC"
};

static const char *const regnames_ft32[] =
{
  "fp", "sp",
  "r0", "r1", "r2", "r3",  "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  "r16", "r17", "r18", "r19",  "r20", "r21", "r22", "r23",
  "r24", "r25", "r26", "r27", "r28", "cc",
  "pc"
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

static const char *const regnames_avr32[] =
{  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
   "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
   "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
    "SREG", "SP", "PC2", "pc"
};

static const char *const regnames_epiphany[] = {
  "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
  "r8",  "r9",  "r10", "fp",  "ip",  "sp",  "lr",  "r15",
  "r16",  "r17","r18", "r19", "r20", "r21", "r22", "r23",
  "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
  "r32", "r33", "r34", "r35", "r36", "r37", "r38", "r39",
  "r40", "r41", "r42", "r43", "r44", "r45", "r46", "r47",
  "r48", "r49", "r50", "r51", "r52", "r53", "r54", "r55",
  "r56", "r57", "r58", "r59", "r60", "r61", "r62", "r63",
  "ap",  "sfp", "cc1", "cc2",
  "config", "status", "lc", "ls", "le", "iret",
  "fp_near", "fp_trunc", "fp_anyfp"
};

static const char *const regnames_frv[] = {
  "gr0",  "sp",   "fp",   "gr3",  "gr4",  "gr5",  "gr6",  "gr7",
  "gr8",  "gr9",  "gr10", "gr11", "gr12", "gr13", "gr14", "gr15",
  "gr16", "gr17", "gr18", "gr19", "gr20", "gr21", "gr22", "gr23",
  "gr24", "gr25", "gr26", "gr27", "gr28", "gr29", "gr30", "gr31",
  "gr32", "gr33", "gr34", "gr35", "gr36", "gr37", "gr38", "gr39",
  "gr40", "gr41", "gr42", "gr43", "gr44", "gr45", "gr46", "gr47",
  "gr48", "gr49", "gr50", "gr51", "gr52", "gr53", "gr54", "gr55",
  "gr56", "gr57", "gr58", "gr59", "gr60", "gr61", "gr62", "gr63",
  "fr0",  "fr1",  "fr2",  "fr3",  "fr4",  "fr5",  "fr6",  "fr7",
  "fr8",  "fr9",  "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
  "fr16", "fr17", "fr18", "fr19", "fr20", "fr21", "fr22", "fr23",
  "fr24", "fr25", "fr26", "fr27", "fr28", "fr29", "fr30", "fr31",
  "fr32", "fr33", "fr34", "fr35", "fr36", "fr37", "fr38", "fr39",
  "fr40", "fr41", "fr42", "fr43", "fr44", "fr45", "fr46", "fr47",
  "fr48", "fr49", "fr50", "fr51", "fr52", "fr53", "fr54", "fr55",
  "fr56", "fr57", "fr58", "fr59", "fr60", "fr61", "fr62", "fr63",
  "fcc0", "fcc1", "fcc2", "fcc3", "icc0", "icc1", "icc2", "icc3",
  "cc0",  "cc1",  "cc2",  "cc3",  "cc4",  "cc5",  "cc6",  "cc7",
  "acc0", "acc1", "acc2", "acc3", "acc4", "acc5", "acc6", "acc7",
  "acc8", "acc9", "acc10", "acc11",
  "accg0","accg1","accg2","accg3","accg4","accg5","accg6","accg7",
  "accg8", "accg9", "accg10", "accg11",
  "ap",   "lr",   "lcr",  "iacc0h", "iacc0l"
};

static const char *const regnames_lm32[] = {
 "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
 "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
 "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
 "r24", "r25",  "gp",  "fp",  "sp",  "ra",  "ea",  "ba"
};

static const char *const regnames_mcore[] = {
  "sp", "r1", "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  "apvirtual",  "c", "fpvirtual", "x19"
};

static const char *const regnames_microblaze[] = {
  "r0",   "r1",   "r2",   "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",   "r10",  "r11",  "r12",  "r13",  "r14",  "r15",
  "r16",  "r17",  "r18",  "r19",  "r20",  "r21",  "r22",  "r23",
  "r24",  "r25",  "r26",  "r27",  "r28",  "r29",  "r30",  "r31",
  "rmsr", "$ap",  "$rap", "$frp"
};

static const char *const regnames_or1k[] = {
  "r0",   "r1",   "r2",   "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",   "r10",  "r11",  "r12",  "r13",  "r14",  "r15",
  "r17",  "r19",  "r21",  "r23",  "r25",  "r27",  "r29",  "r31",
  "r16",  "r18",  "r20",  "r22",  "r24",  "r26",  "r28",  "r30",
  "?ap",  "?fp",  "?sr_f"
};

static const char *const regnames_riscv[] = {
 "zero","ra",  "sp",  "gp",  "tp",  "t0",  "t1",  "t2",
  "s0",  "s1",  "a0",  "a1",  "a2",  "a3",  "a4",  "a5",
  "a6",  "a7",  "s2",  "s3",  "s4",  "s5",  "s6",  "s7",
  "s8",  "s9",  "s10", "s11", "t3",  "t4",  "t5",  "t6",
  "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7",
  "fs0", "fs1", "fa0", "fa1", "fa2", "fa3", "fa4", "fa5",
  "fa6", "fa7", "fs2", "fs3", "fs4", "fs5", "fs6", "fs7",
  "fs8", "fs9", "fs10","fs11","ft8", "ft9", "ft10","ft11",
  "arg", "frame"
};

static const char *const regnames_rl78[] = {
    "x",   "a",   "c",   "b",   "e",   "d",   "l",   "h",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
    "sp",  "ap",  "psw", "es",  "cs"
};

static const char *const regnames_rx[] = {
 "r0",  "r1",  "r2",   "r3",   "r4",   "r5",   "r6",   "r7",
 "r8",  "r9",  "r10",  "r11",  "r12",  "r13",  "r14",  "r15", "cc"
};

static const char *const regnames_v850[] = {
  "r0",  "r1",  "r2",  "sp",  "gp",  "r5",  "r6" , "r7",
  "r8",  "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
  "r24", "r25", "r26", "r27", "r28", "r29",  "ep", "r31",
  "psw", "fcc", ".fp", ".ap"
};

static const char *const regnames_xtensa[] = {
  "a0",   "sp",   "a2",   "a3",   "a4",   "a5",   "a6",   "a7",
  "a8",   "a9",   "a10",  "a11",  "a12",  "a13",  "a14",  "a15",
  "fp",   "argp", "b0",
  "f0",   "f1",   "f2",   "f3",   "f4",   "f5",   "f6",   "f7",
  "f8",   "f9",   "f10",  "f11",  "f12",  "f13",  "f14",  "f15",
  "acc"
};

static const char *const regnames_pa32[] = {
 "%r0",   "%r1",    "%r2",   "%r3",    "%r4",   "%r5",    "%r6",   "%r7", 
 "%r8",   "%r9",    "%r10",  "%r11",   "%r12",  "%r13",   "%r14",  "%r15",
 "%r16",  "%r17",   "%r18",  "%r19",   "%r20",  "%r21",   "%r22",  "%r23",
 "%r24",  "%r25",   "%r26",  "%r27",   "%r28",  "%r29",   "%r30",  "%r31",
 "%fr4",  "%fr4R",  "%fr5",  "%fr5R",  "%fr6",  "%fr6R",  "%fr7",  "%fr7R",
 "%fr8",  "%fr8R",  "%fr9",  "%fr9R",  "%fr10", "%fr10R", "%fr11", "%fr11R",
 "%fr12", "%fr12R", "%fr13", "%fr13R", "%fr14", "%fr14R", "%fr15", "%fr15R",
 "%fr16", "%fr16R", "%fr17", "%fr17R", "%fr18", "%fr18R", "%fr19", "%fr19R",
 "%fr20", "%fr20R", "%fr21", "%fr21R", "%fr22", "%fr22R", "%fr23", "%fr23R",
 "%fr24", "%fr24R", "%fr25", "%fr25R", "%fr26", "%fr26R", "%fr27", "%fr27R",
 "%fr28", "%fr28R", "%fr29", "%fr29R", "%fr30", "%fr30R", "%fr31", "%fr31R",
 "SAR",   "sfp"
};

static const char *const regnames_pa64[] = {
 "%r0",   "%r1",    "%r2",   "%r3",    "%r4",   "%r5",    "%r6",   "%r7",
 "%r8",   "%r9",    "%r10",  "%r11",   "%r12",  "%r13",   "%r14",  "%r15",
 "%r16",  "%r17",   "%r18",  "%r19",   "%r20",  "%r21",   "%r22",  "%r23",
 "%r24",  "%r25",   "%r26",  "%r27",   "%r28",  "%r29",   "%r30",  "%r31",
 "%fr4",  "%fr5",   "%fr6",  "%fr7",   "%fr8",  "%fr9",   "%fr10", "%fr11",
 "%fr12", "%fr13",  "%fr14", "%fr15",  "%fr16", "%fr17",  "%fr18", "%fr19",
 "%fr20", "%fr21",  "%fr22", "%fr23",  "%fr24", "%fr25",  "%fr26", "%fr27",
 "%fr28", "%fr29",  "%fr30", "%fr31",  "SAR",   "sfp"
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

RegNames *get_regnames(ELFIO::Elf_Half mac, bool is64)
{
  RegNames *res = nullptr;
  switch(mac)
  {
    case ELFIO::EM_386:
       res = new tableRegNames(dwarf_regnames_i386, ARRAY_SIZE(dwarf_regnames_i386), &i386_addr_type);
       return res;
      break;
    case ELFIO::EM_486: // 6
       res = new tableRegNames(dwarf_regnames_iamcu, ARRAY_SIZE(dwarf_regnames_iamcu), &i386_addr_type);
       return res;
      break;
    case ELFIO::EM_X86_64:
    case 180: // EM_L1OM
    case 181: // EM_K1OM
       res = new tableRegNames(dwarf_regnames_x86_64, ARRAY_SIZE(dwarf_regnames_x86_64));
       return res;
      break;
    case ELFIO::EM_ARM:
       res = new ArmRegNames();
       return res;
       break;
    case ELFIO::EM_AARCH64:
       res = new tableRegNames(dwarf_regnames_aarch64, ARRAY_SIZE(dwarf_regnames_aarch64));
       return res;
      break;
    case ELFIO::EM_ARC:
       res = new tableRegNames(dwarf_regnames_arc, ARRAY_SIZE(dwarf_regnames_arc));
       return res;
    case ELFIO::EM_BLACKFIN:
       res = new tableRegNames(dwarf_regnames_bfin, ARRAY_SIZE(dwarf_regnames_bfin));
       return res;
    case ELFIO::EM_TI_C6000:
       res = new tableRegNames(dwarf_regnames_c6x, ARRAY_SIZE(dwarf_regnames_c6x));
       return res;
    case ELFIO::EM_S390:
       res = new tableRegNames(dwarf_regnames_s390, ARRAY_SIZE(dwarf_regnames_s390), &s390_addr_type);
       return res;
      break;
    case ELFIO::EM_PPC:
    case ELFIO::EM_PPC64: // TODO - perhaps they have different registers set?
       res = new tableRegNames(regnames_ppc, ARRAY_SIZE(regnames_ppc));
       return res;
      break;
    case ELFIO::EM_AVR32:
       res = new tableRegNames(regnames_avr32, ARRAY_SIZE(regnames_avr32));
       return res;
      break;
    case ELFIO::EM_ADAPTEVA_EPIPHANY:
       res = new tableRegNames(regnames_epiphany, ARRAY_SIZE(regnames_epiphany));
       return res;
    case ELFIO::EM_FT32:
       res = new tableRegNames(regnames_ft32, ARRAY_SIZE(regnames_ft32), &ft32_addr_type);
       return res;
      break;
    case ELFIO::EM_MIPS:
       res = new tableRegNames(regnames_mips, ARRAY_SIZE(regnames_mips));
       return res;
      break;
    case ELFIO::EM_PARISC:
       if ( is64 )
         res = new tableRegNames(regnames_pa64, ARRAY_SIZE(regnames_pa64));
       else
         res = new tableRegNames(regnames_pa32, ARRAY_SIZE(regnames_pa32));
       return res;
    case ELFIO::EM_RISCV:
       res = new tableRegNames(regnames_riscv, ARRAY_SIZE(regnames_riscv));
       return res;
    case ELFIO::EM_SPARCV9:
       res = new tableRegNames(regnames_sparc, ARRAY_SIZE(regnames_sparc));
       return res;
      break;
    case ELFIO::EM_MCORE:
       res = new tableRegNames(regnames_mcore, ARRAY_SIZE(regnames_mcore));
       return res;
    case ELFIO::EM_LATTICEMICO32:
       res = new tableRegNames(regnames_lm32, ARRAY_SIZE(regnames_lm32));
       return res;
    case ELFIO::EM_CYGNUS_FRV:
       res = new tableRegNames(regnames_frv, ARRAY_SIZE(regnames_frv));
       return res;
    case ELFIO::EM_MICROBLAZE:
       res = new tableRegNames(regnames_microblaze, ARRAY_SIZE(regnames_microblaze));
       return res;
    case 92: // EM_OR1K
       res = new tableRegNames(regnames_or1k, ARRAY_SIZE(regnames_or1k));
       return res;
    case ELFIO::EM_RL78:
       res = new tableRegNames(regnames_rl78, ARRAY_SIZE(regnames_rl78));
       return res;
    case ELFIO::EM_RX:
       res = new tableRegNames(regnames_rx, ARRAY_SIZE(regnames_rx));
       return res;
    case 0x9080: // EM_CYGNUS_V850
       res = new tableRegNames(regnames_v850, ARRAY_SIZE(regnames_v850));
       return res;
    case 0xabc7: // EM_XTENSA_OLD
    case ELFIO::EM_XTENSA:
       res = new tableRegNames(regnames_xtensa, ARRAY_SIZE(regnames_xtensa));
       return res;
    case ELFIO::EM_CUDA:
       res = new CudaRegNames();
       return res;

    case 258:
       res = new tableRegNames(regnames_loongarch, ARRAY_SIZE(regnames_loongarch));
       return res;
      break;
  } 
  return nullptr;
}