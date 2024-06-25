#include "ElfFile.h"
#include <limits.h>

// from elf/common.h
#define EM_IAMCU	   6
#define EM_ARC_COMPACT     93  /* ARC International ARCompact processor */
#define EM_OR1K		   92  /* OpenRISC 1000 32-bit embedded processor */
#define EM_PJ_OLD	   99  /* Old value for picoJava.  Deprecated.  */
#define EM_TI_PRU	   144 /* Texas Instruments Programmable Realtime Unit */
#define EM_K1OM		   181 /* Intel K1OM */
#define EM_AVR_OLD	   0x1057
#define EM_MSP430_OLD	   0x1059
#define EM_CYGNUS_FR30     0x3330
#define EM_CYGNUS_D10V     0x7650
#define EM_CYGNUS_D30V     0x7676
#define EM_IP2K_OLD	   0x8217
#define EM_CYGNUS_V850	   0x9080
#define EM_SW_64       0x9916
#define EM_S390_OLD	   0xa390
#define EM_XTENSA_OLD	   0xabc7
#define EM_CYGNUS_MN10300  0xbeef
#define EM_CYGNUS_MN10200  0xdead

// elf/h8.h
#define R_H8_DIR16   17

// elf/msp430.h
#define EF_MSP430_MACH 		0xff
#define E_MSP430_MACH_MSP430X    45

unsigned int
ElfFile::get_reloc_type(unsigned int reloc_info)
{
  if ( reader.get_class() == ELFCLASS32 ) return reloc_info;
  switch( reader.get_machine() )
  {
    case EM_SPARCV9:
    case EM_MIPS: return reloc_info & 0xff;
  }
  return reloc_info;
}

static bool
is_32bit_abs_reloc (Elf_Half machine, unsigned int reloc_type, Elf_Half &prev_warn, ErrLog *e_)
{
  /* Please keep this table alpha-sorted for ease of visual lookup.  */
  switch (machine)
    {
    case EM_386:
    case EM_IAMCU:
      return reloc_type == 1; /* R_386_32.  */
    case EM_68K:
      return reloc_type == 1; /* R_68K_32.  */
    case EM_860:
      return reloc_type == 1; /* R_860_32.  */
    case EM_960:
      return reloc_type == 2; /* R_960_32.  */
    case EM_AARCH64:
      return (reloc_type == 258
	      || reloc_type == 1); /* R_AARCH64_ABS32 || R_AARCH64_P32_ABS32 */
    case EM_BPF:
      return reloc_type == 11; /* R_BPF_DATA_32 */
    case EM_ADAPTEVA_EPIPHANY:
      return reloc_type == 3;
    case EM_ALPHA:
      return reloc_type == 1; /* R_ALPHA_REFLONG.  */
    case EM_ARC:
      return reloc_type == 1; /* R_ARC_32.  */
    case EM_ARC_COMPACT:
    case EM_ARC_COMPACT2:
    case EM_ARC_COMPACT3:
    case EM_ARC_COMPACT3_64:
      return reloc_type == 4; /* R_ARC_32.  */
    case EM_ARM:
      return reloc_type == 2; /* R_ARM_ABS32 */
    case EM_AVR_OLD:
    case EM_AVR:
      return reloc_type == 1;
    case EM_BLACKFIN:
      return reloc_type == 0x12; /* R_byte4_data.  */
    case EM_CRIS:
      return reloc_type == 3; /* R_CRIS_32.  */
    case EM_CR16:
      return reloc_type == 3; /* R_CR16_NUM32.  */
    case EM_CRX:
      return reloc_type == 15; /* R_CRX_NUM32.  */
    case EM_CSKY:
      return reloc_type == 1; /* R_CKCORE_ADDR32.  */
    case EM_CYGNUS_FRV:
      return reloc_type == 1;
    case EM_CYGNUS_D10V:
    case EM_D10V:
      return reloc_type == 6; /* R_D10V_32.  */
    case EM_CYGNUS_D30V:
    case EM_D30V:
      return reloc_type == 12; /* R_D30V_32_NORMAL.  */
    case EM_DLX:
      return reloc_type == 3; /* R_DLX_RELOC_32.  */
    case EM_CYGNUS_FR30:
    case EM_FR30:
      return reloc_type == 3; /* R_FR30_32.  */
    case EM_FT32:
      return reloc_type == 1; /* R_FT32_32.  */
    case EM_H8S:
    case EM_H8_300:
    case EM_H8_300H:
      return reloc_type == 1; /* R_H8_DIR32.  */
    case EM_IA_64:
      return (reloc_type == 0x64    /* R_IA64_SECREL32MSB.  */
	      || reloc_type == 0x65 /* R_IA64_SECREL32LSB.  */
	      || reloc_type == 0x24 /* R_IA64_DIR32MSB.  */
	      || reloc_type == 0x25 /* R_IA64_DIR32LSB.  */);
    case EM_IP2K_OLD:
    case EM_IP2K:
      return reloc_type == 2; /* R_IP2K_32.  */
    case EM_IQ2000:
      return reloc_type == 2; /* R_IQ2000_32.  */
    case EM_KVX:
      return reloc_type == 2; /* R_KVX_32.  */
    case EM_LATTICEMICO32:
      return reloc_type == 3; /* R_LM32_32.  */
    case EM_LOONGARCH:
      return reloc_type == 1; /* R_LARCH_32. */
    case EM_M32C_OLD:
    case EM_M32C:
      return reloc_type == 3; /* R_M32C_32.  */
    case EM_M32R:
      return reloc_type == 34; /* R_M32R_32_RELA.  */
    case EM_68HC11:
    case EM_68HC12:
      return reloc_type == 6; /* R_M68HC11_32.  */
    case EM_S12Z:
      return reloc_type == 7 || /* R_S12Z_EXT32 */
	reloc_type == 6;        /* R_S12Z_CW32.  */
    case EM_MCORE:
      return reloc_type == 1; /* R_MCORE_ADDR32.  */
    case EM_CYGNUS_MEP:
      return reloc_type == 4; /* R_MEP_32.  */
    case EM_METAG:
      return reloc_type == 2; /* R_METAG_ADDR32.  */
    case EM_MICROBLAZE:
      return reloc_type == 1; /* R_MICROBLAZE_32.  */
    case EM_MIPS:
      return reloc_type == 2; /* R_MIPS_32.  */
    case EM_MMIX:
      return reloc_type == 4; /* R_MMIX_32.  */
    case EM_CYGNUS_MN10200:
    case EM_MN10200:
      return reloc_type == 1; /* R_MN10200_32.  */
    case EM_CYGNUS_MN10300:
    case EM_MN10300:
      return reloc_type == 1; /* R_MN10300_32.  */
    case EM_MOXIE:
      return reloc_type == 1; /* R_MOXIE_32.  */
    case EM_MSP430_OLD:
    case EM_MSP430:
      return reloc_type == 1; /* R_MSP430_32 or R_MSP320_ABS32.  */
    case EM_MT:
      return reloc_type == 2; /* R_MT_32.  */
    case EM_NDS32:
      return reloc_type == 20; /* R_NDS32_32_RELA.  */
    case EM_ALTERA_NIOS2:
      return reloc_type == 12; /* R_NIOS2_BFD_RELOC_32.  */
    case EM_NIOS32:
      return reloc_type == 1; /* R_NIOS_32.  */
    case EM_OR1K:
      return reloc_type == 1; /* R_OR1K_32.  */
    case EM_PARISC:
      return (reloc_type == 1 /* R_PARISC_DIR32.  */
	      || reloc_type == 2 /* R_PARISC_DIR21L.  */
	      || reloc_type == 41); /* R_PARISC_SECREL32.  */
    case EM_PJ:
    case EM_PJ_OLD:
      return reloc_type == 1; /* R_PJ_DATA_DIR32.  */
    case EM_PPC64:
      return reloc_type == 1; /* R_PPC64_ADDR32.  */
    case EM_PPC:
      return reloc_type == 1; /* R_PPC_ADDR32.  */
    case EM_TI_PRU:
      return reloc_type == 11; /* R_PRU_BFD_RELOC_32.  */
    case EM_RISCV:
      return reloc_type == 1; /* R_RISCV_32.  */
    case EM_RL78:
      return reloc_type == 1; /* R_RL78_DIR32.  */
    case EM_RX:
      return reloc_type == 1; /* R_RX_DIR32.  */
    case EM_S370:
      return reloc_type == 1; /* R_I370_ADDR31.  */
    case EM_S390_OLD:
    case EM_S390:
      return reloc_type == 4; /* R_S390_32.  */
    case EM_SCORE:
      return reloc_type == 8; /* R_SCORE_ABS32.  */
    case EM_SH:
      return reloc_type == 1; /* R_SH_DIR32.  */
    case EM_SPARC32PLUS:
    case EM_SPARCV9:
    case EM_SPARC:
      return reloc_type == 3 /* R_SPARC_32.  */
	|| reloc_type == 23; /* R_SPARC_UA32.  */
    case EM_SPU:
      return reloc_type == 6; /* R_SPU_ADDR32 */
    case EM_SW_64:
      return reloc_type == 1; /* R_SW64_REFLONG */
    case EM_TI_C6000:
      return reloc_type == 1; /* R_C6000_ABS32.  */
    case EM_TILEGX:
      return reloc_type == 2; /* R_TILEGX_32.  */
    case EM_TILEPRO:
      return reloc_type == 1; /* R_TILEPRO_32.  */
    case EM_CYGNUS_V850:
    case EM_V850:
      return reloc_type == 6; /* R_V850_ABS32.  */
    case EM_V800:
      return reloc_type == 0x33; /* R_V810_WORD.  */
    case EM_VAX:
      return reloc_type == 1; /* R_VAX_32.  */
    case EM_VISIUM:
      return reloc_type == 3;  /* R_VISIUM_32. */
    case EM_WEBASSEMBLY:
      return reloc_type == 1;  /* R_WASM32_32.  */
    case EM_X86_64:
    case EM_L1OM:
    case EM_K1OM:
      return reloc_type == 10; /* R_X86_64_32.  */
    case EM_XGATE:
      return reloc_type == 4; /* R_XGATE_32.  */
    case EM_XSTORMY16:
      return reloc_type == 1; /* R_XSTROMY16_32.  */
    case EM_XTENSA_OLD:
    case EM_XTENSA:
      return reloc_type == 1; /* R_XTENSA_32.  */
    case EM_Z80:
      return reloc_type == 6; /* R_Z80_32.  */
    default:

	/* Avoid repeating the same warning multiple times.  */
	if (prev_warn != machine)
	  e_->error("Missing knowledge of 32-bit reloc types used in DWARF sections of machine number %d\n",
		 machine);
	prev_warn = machine;
	return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 32-bit pc-relative RELA relocation used in DWARF debug sections.  */

static bool
is_32bit_pcrel_reloc (Elf_Half machine, unsigned int reloc_type)
{
  switch (machine)
  /* Please keep this table alpha-sorted for ease of visual lookup.  */
    {
    case EM_386:
    case EM_IAMCU:
      return reloc_type == 2;  /* R_386_PC32.  */
    case EM_68K:
      return reloc_type == 4;  /* R_68K_PC32.  */
    case EM_AARCH64:
      return reloc_type == 261; /* R_AARCH64_PREL32 */
    case EM_ADAPTEVA_EPIPHANY:
      return reloc_type == 6;
    case EM_ALPHA:
      return reloc_type == 10; /* R_ALPHA_SREL32.  */
    case EM_ARC_COMPACT:
    case EM_ARC_COMPACT2:
    case EM_ARC_COMPACT3:
    case EM_ARC_COMPACT3_64:
      return reloc_type == 49; /* R_ARC_32_PCREL.  */
    case EM_ARM:
      return reloc_type == 3;  /* R_ARM_REL32 */
    case EM_AVR_OLD:
    case EM_AVR:
      return reloc_type == 36; /* R_AVR_32_PCREL.  */
    case EM_LOONGARCH:
      return reloc_type == 99;  /* R_LARCH_32_PCREL.  */
    case EM_MICROBLAZE:
      return reloc_type == 2;  /* R_MICROBLAZE_32_PCREL.  */
    case EM_OR1K:
      return reloc_type == 9; /* R_OR1K_32_PCREL.  */
    case EM_PARISC:
      return reloc_type == 9;  /* R_PARISC_PCREL32.  */
    case EM_PPC:
      return reloc_type == 26; /* R_PPC_REL32.  */
    case EM_PPC64:
      return reloc_type == 26; /* R_PPC64_REL32.  */
    case EM_RISCV:
      return reloc_type == 57;	/* R_RISCV_32_PCREL.  */
    case EM_S390_OLD:
    case EM_S390:
      return reloc_type == 5;  /*  R_390_PC32.  */
    case EM_SH:
      return reloc_type == 2;  /* R_SH_REL32.  */
    case EM_SPARC32PLUS:
    case EM_SPARCV9:
    case EM_SPARC:
      return reloc_type == 6;  /* R_SPARC_DISP32.  */
    case EM_SPU:
      return reloc_type == 13; /* R_SPU_REL32.  */
    case EM_SW_64:
      return 10 == reloc_type; /* R_SW64_SREL32 */
    case EM_TILEGX:
      return reloc_type == 6; /* R_TILEGX_32_PCREL.  */
    case EM_TILEPRO:
      return reloc_type == 4; /* R_TILEPRO_32_PCREL.  */
    case EM_VISIUM:
      return reloc_type == 6;  /* R_VISIUM_32_PCREL */
    case EM_X86_64:
    case EM_L1OM:
    case EM_K1OM:
      return reloc_type == 2;  /* R_X86_64_PC32.  */
    case EM_VAX:
      return reloc_type == 4;  /* R_VAX_PCREL32.  */
    case EM_XTENSA_OLD:
    case EM_XTENSA:
      return reloc_type == 14; /* R_XTENSA_32_PCREL.  */
    case EM_KVX:
      return reloc_type == 7; /* R_KVX_32_PCREL */
    default:
      /* Do not abort or issue an error message here.  Not all targets use
	 pc-relative 32-bit relocs in their DWARF debug information and we
	 have already tested for target coverage in is_32bit_abs_reloc.  A
	 more helpful warning message will be generated by apply_relocations
	 anyway, so just return.  */
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 64-bit absolute RELA relocation used in DWARF debug sections.  */

static bool
is_64bit_abs_reloc (Elf_Half machine, unsigned int reloc_type)
{
  switch (machine)
    {
    case EM_AARCH64:
      return reloc_type == 257;	/* R_AARCH64_ABS64.  */
    case EM_ARC_COMPACT3_64:
      return reloc_type == 5; /* R_ARC_64.  */
    case EM_ALPHA:
      return reloc_type == 2; /* R_ALPHA_REFQUAD.  */
    case EM_IA_64:
      return (reloc_type == 0x26    /* R_IA64_DIR64MSB.  */
	      || reloc_type == 0x27 /* R_IA64_DIR64LSB.  */);
    case EM_LOONGARCH:
      return reloc_type == 2;      /* R_LARCH_64 */
    case EM_PARISC:
      return reloc_type == 80; /* R_PARISC_DIR64.  */
    case EM_PPC64:
      return reloc_type == 38; /* R_PPC64_ADDR64.  */
    case EM_RISCV:
      return reloc_type == 2; /* R_RISCV_64.  */
    case EM_SPARC32PLUS:
    case EM_SPARCV9:
    case EM_SPARC:
      return reloc_type == 32 /* R_SPARC_64.  */
	|| reloc_type == 54; /* R_SPARC_UA64.  */
    case EM_X86_64:
    case EM_L1OM:
    case EM_K1OM:
      return reloc_type == 1; /* R_X86_64_64.  */
    case EM_S390_OLD:
    case EM_S390:
      return reloc_type == 22;	/* R_S390_64.  */
    case EM_SW_64:
      return 2 == reloc_type; /* R_SW64_REFQUAD */
    case EM_TILEGX:
      return reloc_type == 1; /* R_TILEGX_64.  */
    case EM_MIPS:
      return reloc_type == 18;	/* R_MIPS_64.  */
    case EM_KVX:
      return reloc_type == 3; /* R_KVX_64 */
    default:
      return false;
    }
}

/* Like is_32bit_pcrel_reloc except that it returns TRUE iff RELOC_TYPE is
   a 64-bit pc-relative RELA relocation used in DWARF debug sections.  */

static bool
is_64bit_pcrel_reloc (Elf_Half machine, unsigned int reloc_type)
{
  switch (machine)
    {
    case EM_AARCH64:
      return reloc_type == 260;	/* R_AARCH64_PREL64.  */
    case EM_ALPHA:
      return reloc_type == 11; /* R_ALPHA_SREL64.  */
    case EM_IA_64:
      return (reloc_type == 0x4e    /* R_IA64_PCREL64MSB.  */
	      || reloc_type == 0x4f /* R_IA64_PCREL64LSB.  */);
    case EM_PARISC:
      return reloc_type == 72; /* R_PARISC_PCREL64.  */
    case EM_PPC64:
      return reloc_type == 44; /* R_PPC64_REL64.  */
    case EM_SPARC32PLUS:
    case EM_SPARCV9:
    case EM_SPARC:
      return reloc_type == 46; /* R_SPARC_DISP64.  */
    case EM_X86_64:
    case EM_L1OM:
    case EM_K1OM:
      return reloc_type == 24; /* R_X86_64_PC64.  */
    case EM_S390_OLD:
    case EM_S390:
      return reloc_type == 23;	/* R_S390_PC64.  */
    case EM_SW_64:
      return 11 == reloc_type; /* R_SW64_SREL64 */
    case EM_TILEGX:
      return reloc_type == 5;  /* R_TILEGX_64_PCREL.  */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 24-bit absolute RELA relocation used in DWARF debug sections.  */

static bool
is_24bit_abs_reloc (Elf_Half machine, unsigned int reloc_type)
{
  switch (machine)
    {
    case EM_CYGNUS_MN10200:
    case EM_MN10200:
      return reloc_type == 4; /* R_MN10200_24.  */
    case EM_FT32:
      return reloc_type == 5; /* R_FT32_20.  */
    case EM_Z80:
      return reloc_type == 5; /* R_Z80_24. */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 16-bit absolute RELA relocation used in DWARF debug sections.  */

static bool
is_16bit_abs_reloc (Elf_Half machine, unsigned int reloc_type, bool uses_msp430x_relocs)
{
  /* Please keep this table alpha-sorted for ease of visual lookup.  */
  switch (machine)
    {
    case EM_ARC:
    case EM_ARC_COMPACT:
    case EM_ARC_COMPACT2:
    case EM_ARC_COMPACT3:
    case EM_ARC_COMPACT3_64:
      return reloc_type == 2; /* R_ARC_16.  */
    case EM_ADAPTEVA_EPIPHANY:
      return reloc_type == 5;
    case EM_AVR_OLD:
    case EM_AVR:
      return reloc_type == 4; /* R_AVR_16.  */
    case EM_CYGNUS_D10V:
    case EM_D10V:
      return reloc_type == 3; /* R_D10V_16.  */
    case EM_FT32:
      return reloc_type == 2; /* R_FT32_16.  */
    case EM_H8S:
    case EM_H8_300:
    case EM_H8_300H:
      return reloc_type == R_H8_DIR16;
    case EM_IP2K_OLD:
    case EM_IP2K:
      return reloc_type == 1; /* R_IP2K_16.  */
    case EM_M32C_OLD:
    case EM_M32C:
      return reloc_type == 1; /* R_M32C_16 */
    case EM_CYGNUS_MN10200:
    case EM_MN10200:
      return reloc_type == 2; /* R_MN10200_16.  */
    case EM_CYGNUS_MN10300:
    case EM_MN10300:
      return reloc_type == 2; /* R_MN10300_16.  */
    case EM_KVX:
      return reloc_type == 1; /* R_KVX_16 */
    case EM_MSP430:
      if (uses_msp430x_relocs)
	return reloc_type == 2; /* R_MSP430_ABS16.  */
      /* Fall through.  */
    case EM_MSP430_OLD:
      return reloc_type == 5; /* R_MSP430_16_BYTE.  */
    case EM_NDS32:
      return reloc_type == 19; /* R_NDS32_16_RELA.  */
    case EM_ALTERA_NIOS2:
      return reloc_type == 13; /* R_NIOS2_BFD_RELOC_16.  */
    case EM_NIOS32:
      return reloc_type == 9; /* R_NIOS_16.  */
    case EM_OR1K:
      return reloc_type == 2; /* R_OR1K_16.  */
    case EM_RISCV:
      return reloc_type == 55; /* R_RISCV_SET16.  */
    case EM_TI_PRU:
      return reloc_type == 8; /* R_PRU_BFD_RELOC_16.  */
    case EM_TI_C6000:
      return reloc_type == 2; /* R_C6000_ABS16.  */
    case EM_VISIUM:
      return reloc_type == 2; /* R_VISIUM_16. */
    case EM_XGATE:
      return reloc_type == 3; /* R_XGATE_16.  */
    case EM_Z80:
      return reloc_type == 4; /* R_Z80_16.  */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 8-bit absolute RELA relocation used in DWARF debug sections.  */

static bool
is_8bit_abs_reloc (Elf_Half machine, unsigned int reloc_type)
{
  switch (machine)
    {
    case EM_RISCV:
      return reloc_type == 54; /* R_RISCV_SET8.  */
    case EM_Z80:
      return reloc_type == 1;  /* R_Z80_8.  */
    case EM_MICROBLAZE:
      return (reloc_type == 33 /* R_MICROBLAZE_32_NONE.  */
	      || reloc_type == 0 /* R_MICROBLAZE_NONE.  */
	      || reloc_type == 9 /* R_MICROBLAZE_64_NONE.  */);
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 6-bit absolute RELA relocation used in DWARF debug sections.  */

static bool
is_6bit_abs_reloc (Elf_Half machine, unsigned int reloc_type)
{
  switch (machine)
    {
    case EM_RISCV:
      return reloc_type == 53; /* R_RISCV_SET6.  */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 32-bit inplace add RELA relocation used in DWARF debug sections.  */

static bool
is_32bit_inplace_add_reloc (Elf_Half machine, unsigned int reloc_type)
{
  /* Please keep this table alpha-sorted for ease of visual lookup.  */
  switch (machine)
    {
    case EM_LOONGARCH:
      return reloc_type == 50; /* R_LARCH_ADD32.  */
    case EM_RISCV:
      return reloc_type == 35; /* R_RISCV_ADD32.  */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 32-bit inplace sub RELA relocation used in DWARF debug sections.  */

static bool
is_32bit_inplace_sub_reloc (Elf_Half machine, unsigned int reloc_type)
{
  /* Please keep this table alpha-sorted for ease of visual lookup.  */
  switch (machine)
    {
    case EM_LOONGARCH:
      return reloc_type == 55; /* R_LARCH_SUB32.  */
    case EM_RISCV:
      return reloc_type == 39; /* R_RISCV_SUB32.  */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 64-bit inplace add RELA relocation used in DWARF debug sections.  */

static bool
is_64bit_inplace_add_reloc (Elf_Half machine, unsigned int reloc_type)
{
  /* Please keep this table alpha-sorted for ease of visual lookup.  */
  switch (machine)
    {
    case EM_LOONGARCH:
      return reloc_type == 51; /* R_LARCH_ADD64.  */
    case EM_RISCV:
      return reloc_type == 36; /* R_RISCV_ADD64.  */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 64-bit inplace sub RELA relocation used in DWARF debug sections.  */

static bool
is_64bit_inplace_sub_reloc (Elf_Half machine, unsigned int reloc_type)
{
  /* Please keep this table alpha-sorted for ease of visual lookup.  */
  switch (machine)
    {
    case EM_LOONGARCH:
      return reloc_type == 56; /* R_LARCH_SUB64.  */
    case EM_RISCV:
      return reloc_type == 40; /* R_RISCV_SUB64.  */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 16-bit inplace add RELA relocation used in DWARF debug sections.  */

static bool
is_16bit_inplace_add_reloc (Elf_Half machine, unsigned int reloc_type)
{
  /* Please keep this table alpha-sorted for ease of visual lookup.  */
  switch (machine)
    {
    case EM_LOONGARCH:
      return reloc_type == 48; /* R_LARCH_ADD16.  */
    case EM_RISCV:
      return reloc_type == 34; /* R_RISCV_ADD16.  */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 16-bit inplace sub RELA relocation used in DWARF debug sections.  */

static bool
is_16bit_inplace_sub_reloc (Elf_Half machine, unsigned int reloc_type)
{
  /* Please keep this table alpha-sorted for ease of visual lookup.  */
  switch (machine)
    {
    case EM_LOONGARCH:
      return reloc_type == 53; /* R_LARCH_SUB16.  */
    case EM_RISCV:
      return reloc_type == 38; /* R_RISCV_SUB16.  */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 8-bit inplace add RELA relocation used in DWARF debug sections.  */

static bool
is_8bit_inplace_add_reloc (Elf_Half machine, unsigned int reloc_type)
{
  /* Please keep this table alpha-sorted for ease of visual lookup.  */
  switch (machine)
    {
    case EM_LOONGARCH:
      return reloc_type == 47; /* R_LARCH_ADD8.  */
    case EM_RISCV:
      return reloc_type == 33; /* R_RISCV_ADD8.  */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 8-bit inplace sub RELA relocation used in DWARF debug sections.  */

static bool
is_8bit_inplace_sub_reloc (Elf_Half machine, unsigned int reloc_type)
{
  /* Please keep this table alpha-sorted for ease of visual lookup.  */
  switch (machine)
    {
    case EM_LOONGARCH:
      return reloc_type == 52; /* R_LARCH_SUB8.  */
    case EM_RISCV:
      return reloc_type == 37; /* R_RISCV_SUB8.  */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 6-bit inplace add RELA relocation used in DWARF debug sections.  */

static bool
is_6bit_inplace_add_reloc (Elf_Half machine, unsigned int reloc_type)
{
  switch (machine)
    {
    case EM_LOONGARCH:
      return reloc_type == 105; /* R_LARCH_ADD6.  */
    default:
      return false;
    }
}

/* Like is_32bit_abs_reloc except that it returns TRUE iff RELOC_TYPE is
   a 6-bit inplace sub RELA relocation used in DWARF debug sections.  */

static bool
is_6bit_inplace_sub_reloc (Elf_Half machine, unsigned int reloc_type)
{
  switch (machine)
    {
    case EM_LOONGARCH:
      return reloc_type == 106; /* R_LARCH_SUB6.  */
    case EM_RISCV:
      return reloc_type == 52; /* R_RISCV_SUB6.  */
    default:
      return false;
    }
}

/* Returns TRUE iff RELOC_TYPE is a NONE relocation used for discarded
   relocation entries (possibly formerly used for SHT_GROUP sections).  */

static bool
is_none_reloc (Elf_Half machine, unsigned int reloc_type)
{
  switch (machine)
    {
    case EM_386:     /* R_386_NONE.  */
    case EM_68K:     /* R_68K_NONE.  */
    case EM_ADAPTEVA_EPIPHANY:
    case EM_ALPHA:   /* R_ALPHA_NONE.  */
    case EM_ALTERA_NIOS2: /* R_NIOS2_NONE.  */
    case EM_ARC:     /* R_ARC_NONE.  */
    case EM_ARC_COMPACT2: /* R_ARC_NONE.  */
    case EM_ARC_COMPACT: /* R_ARC_NONE.  */
    case EM_ARC_COMPACT3: /* R_ARC_NONE.  */
    case EM_ARC_COMPACT3_64: /* R_ARC_NONE.  */
    case EM_ARM:     /* R_ARM_NONE.  */
    case EM_CRIS:    /* R_CRIS_NONE.  */
    case EM_FT32:    /* R_FT32_NONE.  */
    case EM_IA_64:   /* R_IA64_NONE.  */
    case EM_K1OM:    /* R_X86_64_NONE.  */
    case EM_KVX:      /* R_KVX_NONE.  */
    case EM_L1OM:    /* R_X86_64_NONE.  */
    case EM_M32R:    /* R_M32R_NONE.  */
    case EM_MIPS:    /* R_MIPS_NONE.  */
    case EM_MN10300: /* R_MN10300_NONE.  */
    case EM_MOXIE:   /* R_MOXIE_NONE.  */
    case EM_NIOS32:  /* R_NIOS_NONE.  */
    case EM_OR1K:    /* R_OR1K_NONE. */
    case EM_PARISC:  /* R_PARISC_NONE.  */
    case EM_PPC64:   /* R_PPC64_NONE.  */
    case EM_PPC:     /* R_PPC_NONE.  */
    case EM_RISCV:   /* R_RISCV_NONE.  */
    case EM_S390:    /* R_390_NONE.  */
    case EM_S390_OLD:
    case EM_SH:      /* R_SH_NONE.  */
    case EM_SPARC32PLUS:
    case EM_SPARC:   /* R_SPARC_NONE.  */
    case EM_SPARCV9:
    case EM_SW_64:
    case EM_TILEGX:  /* R_TILEGX_NONE.  */
    case EM_TILEPRO: /* R_TILEPRO_NONE.  */
    case EM_TI_C6000:/* R_C6000_NONE.  */
    case EM_X86_64:  /* R_X86_64_NONE.  */
    case EM_Z80:     /* R_Z80_NONE. */
    case EM_WEBASSEMBLY: /* R_WASM32_NONE.  */
      return reloc_type == 0;

    case EM_AARCH64:
      return reloc_type == 0 || reloc_type == 256;
    case EM_AVR_OLD:
    case EM_AVR:
      return (reloc_type == 0 /* R_AVR_NONE.  */
	      || reloc_type == 30 /* R_AVR_DIFF8.  */
	      || reloc_type == 31 /* R_AVR_DIFF16.  */
	      || reloc_type == 32 /* R_AVR_DIFF32.  */);
    case EM_METAG:
      return reloc_type == 3; /* R_METAG_NONE.  */
    case EM_NDS32:
      return (reloc_type == 0       /* R_NDS32_NONE.  */
	      || reloc_type == 205  /* R_NDS32_DIFF8.  */
	      || reloc_type == 206  /* R_NDS32_DIFF16.  */
	      || reloc_type == 207  /* R_NDS32_DIFF32.  */
	      || reloc_type == 208  /* R_NDS32_DIFF_ULEB128.  */);
    case EM_TI_PRU:
      return (reloc_type == 0       /* R_PRU_NONE.  */
	      || reloc_type == 65   /* R_PRU_DIFF8.  */
	      || reloc_type == 66   /* R_PRU_DIFF16.  */
	      || reloc_type == 67   /* R_PRU_DIFF32.  */);
    case EM_XTENSA_OLD:
    case EM_XTENSA:
      return (reloc_type == 0      /* R_XTENSA_NONE.  */
	      || reloc_type == 17  /* R_XTENSA_DIFF8.  */
	      || reloc_type == 18  /* R_XTENSA_DIFF16.  */
	      || reloc_type == 19  /* R_XTENSA_DIFF32.  */
	      || reloc_type == 57  /* R_XTENSA_PDIFF8.  */
	      || reloc_type == 58  /* R_XTENSA_PDIFF16.  */
	      || reloc_type == 59  /* R_XTENSA_PDIFF32.  */
	      || reloc_type == 60  /* R_XTENSA_NDIFF8.  */
	      || reloc_type == 61  /* R_XTENSA_NDIFF16.  */
	      || reloc_type == 62  /* R_XTENSA_NDIFF32.  */);
    }
  return false;
}

uint64_t ElfFile::read_leb128(Elf64_Addr offset, bool sign,
    unsigned int &num_read, int &status)
{
  const unsigned char *data = apply_to->s_ + offset,
   *end = apply_to->s_ + apply_to->size_;
  uint64_t result = 0;
  unsigned int shift = 0;
  num_read = 0;
  status = 1;
  while (data < end)
    {
      unsigned char byte = *data++;
      unsigned char lost, mask;

      num_read++;

      if (shift < CHAR_BIT * sizeof (result))
        {
          result |= ((uint64_t) (byte & 0x7f)) << shift;
          /* These bits overflowed.  */
          lost = byte ^ (result >> shift);
          /* And this is the mask of possible overflow bits.  */
          mask = 0x7f ^ ((uint64_t) 0x7f << shift >> shift);
          shift += 7;
        }
      else
        {
          lost = byte;
          mask = 0x7f;
        }
      if ((lost & mask) != (sign && (int64_t) result < 0 ? mask : 0))
        status |= 2;

      if ((byte & 0x80) == 0)
        {
          status &= ~1;
          if (sign && shift < CHAR_BIT * sizeof (result) && (byte & 0x40))
            result |= -((uint64_t) 1 << shift);
          break;
        }
    }
  return result;

}

bool ElfFile::target_specific_reloc_handling(Elf_Half machine, Elf64_Addr offset,
 Elf_Word sym_index, unsigned reloc_type, Elf_Sxword add)
{
  unsigned int reloc_size = 0;
  int leb_ret = 0;
  uint64_t value = 0;
  switch (machine)
    {
    case EM_LOONGARCH:
      {
	switch (reloc_type)
	  {
	    /* For .uleb128 .LFE1-.LFB1, loongarch write 0 to object file
	       at assembly time.  */
	    case 107: /* R_LARCH_ADD_ULEB128.  */
	    case 108: /* R_LARCH_SUB_ULEB128.  */
	      {
		if (offset < (size_t)apply_to->size_ )
		  value = read_leb128 (offset, false, reloc_size, leb_ret);
		if ( leb_ret != 0 || reloc_size == 0 || reloc_size > 8 )
		  tree_builder->e_->error("LoongArch ULEB128 field at 0x%lx contains invalid "
			   "ULEB128 value\n", offset);

		else if (sym_index >= m_symbols.size())
		  tree_builder->e_->error("%s reloc contains invalid symbol index %d\n",
			 (reloc_type == 107
			  ? "R_LARCH_ADD_ULEB128"
			  : "R_LARCH_SUB_ULEB128"),
			 sym_index);
		else
		  {
		    if (reloc_type == 107)
		      value += add + m_symbols[sym_index].addr;
		    else
		      value -= add + m_symbols[sym_index].addr;

		    /* Write uleb128 value to p.  */
		    unsigned char *p = (unsigned char *)apply_to->s_ + offset;
		    do
		      {
			unsigned char c = value & 0x7f;
			value >>= 7;
			if (--reloc_size != 0)
			  c |= 0x80;
			*p++ = c;
		      }
		    while (reloc_size);
		  }

		return true;
	      }
	  }
	break;
      }

    case EM_MSP430:
    case EM_MSP430_OLD:
      {
	
	switch (reloc_type)
	  {
	  case 10: /* R_MSP430_SYM_DIFF */
	  case 12: /* R_MSP430_GNU_SUB_ULEB128 */
	    if (uses_msp430x_relocs )
	      break;
	    /* Fall through.  */
	  case 21: /* R_MSP430X_SYM_DIFF */
	  case 23: /* R_MSP430X_GNU_SUB_ULEB128 */
	    /* PR 21139.  */
	    if (sym_index >= m_symbols.size())
	      tree_builder->e_->error("%s reloc contains invalid symbol index "
		       "%d\n", "MSP430 SYM_DIFF", sym_index);
	    else
	      saved_sym = &m_symbols[sym_index];
	    return true;

	  case 1: /* R_MSP430_32 or R_MSP430_ABS32 */
	  case 3: /* R_MSP430_16 or R_MSP430_ABS8 */
	    goto handle_sym_diff;

	  case 5: /* R_MSP430_16_BYTE */
	  case 9: /* R_MSP430_8 */
	  case 11: /* R_MSP430_GNU_SET_ULEB128 */
	    if (uses_msp430x_relocs)
	      break;
	    goto handle_sym_diff;

	  case 2: /* R_MSP430_ABS16 */
	  case 15: /* R_MSP430X_ABS16 */
	  case 22: /* R_MSP430X_GNU_SET_ULEB128 */
	    if (! uses_msp430x_relocs )
	      break;
	    goto handle_sym_diff;

	  handle_sym_diff:
	    if (saved_sym != NULL)
	      {
		switch (reloc_type)
		  {
		  case 1: /* R_MSP430_32 or R_MSP430_ABS32 */
		    reloc_size = 4;
		    break;
		  case 11: /* R_MSP430_GNU_SET_ULEB128 */
		  case 22: /* R_MSP430X_GNU_SET_ULEB128 */
		    if (offset < apply_to->size_)
		      read_leb128 (offset, false, reloc_size, leb_ret);
		    break;
		  default:
		    reloc_size = 2;
		    break;
		  }

		if (leb_ret != 0 || reloc_size == 0 || reloc_size > 8)
		  tree_builder->e_->error("MSP430 ULEB128 field at %lX contains invalid ULEB128 value\n",
			 offset);
		else if (sym_index >= m_symbols.size())
		  tree_builder->e_->error("%s reloc contains invalid symbol index "
			   "%d \n", "MSP430", sym_index);
		else
		  {
		    value = add + (m_symbols[sym_index].addr - saved_sym->addr);

		    if (apply_to->in_section(offset, reloc_size))
		      byte_put (apply_to->s_ + offset, value, reloc_size);
		    else
		      /* PR 21137 */
		      tree_builder->e_->error("MSP430 sym diff reloc contains invalid offset: "
			       "%lX\n", offset);
		  }

		saved_sym = NULL;
		return true;
	      }
	    break;

	  default:
	    if (saved_sym != NULL)
	      tree_builder->e_->error("Unhandled MSP430 reloc type found after SYM_DIFF reloc\n");
	    break;
	  }
	break;
      }

    case EM_MN10300:
    case EM_CYGNUS_MN10300:
      {
	
	switch (reloc_type)
	  {
	  case 34: /* R_MN10300_ALIGN */
	    return true;
	  case 33: /* R_MN10300_SYM_DIFF */
	    if (sym_index >= m_symbols.size())
	      tree_builder->e_->error("%s reloc contains invalid symbol index "
		       "%d\n", "MN10300_SYM_DIFF", sym_index);
	    else
	      saved_sym = &m_symbols[sym_index];
	    return true;

	  case 1: /* R_MN10300_32 */
	  case 2: /* R_MN10300_16 */
	    if (saved_sym != NULL)
	      {
		reloc_size = reloc_type == 1 ? 4 : 2;

		if (sym_index >= m_symbols.size())
		  tree_builder->e_->error("%s reloc contains invalid symbol index "
			   "%d\n", "MN10300", sym_index);
		else
		  {
		    value = add + (m_symbols[sym_index].addr - saved_sym->addr);

		    if ( apply_to->in_section(offset, reloc_size))
		      byte_put (apply_to->s_ + offset, value, reloc_size);
		    else
		      tree_builder->e_->error("MN10300 sym diff reloc contains invalid offset:"
			       " %lX\n", offset);
		  }

		saved_sym = NULL;
		return true;
	      }
	    break;
	  default:
	    if (saved_sym != NULL)
	      tree_builder->e_->error("Unhandled MN10300 reloc type %d found after SYM_DIFF reloc\n", reloc_type);
	    break;
	  }
	break;
      }

    case EM_RL78:
      {

	switch (reloc_type)
	  {
	  case 0x80: /* R_RL78_SYM.  */
	    saved_sym1 = saved_sym2;
	    if (sym_index >= m_symbols.size())
	      tree_builder->e_->error("%s reloc contains invalid symbol index %d\n", "RL78_SYM", sym_index);
	    else
	      {
		saved_sym2 = m_symbols[sym_index].addr;
		saved_sym2 += add;
	      }
	    return true;

	  case 0x83: /* R_RL78_OPsub.  */
	    value = saved_sym1 - saved_sym2;
	    saved_sym2 = saved_sym1 = 0;
	    return true;
	    break;

	  case 0x41: /* R_RL78_ABS32.  */
	    if ( apply_to->in_section(offset, 4))
	      byte_put (apply_to->s_ + offset, value, 4);
	    else
	      tree_builder->e_->error("RL78 sym diff reloc contains invalid offset: %lX\n", offset);
	    value = 0;
	    return true;

	  case 0x43: /* R_RL78_ABS16.  */
	    if ( apply_to->in_section (offset, 2))
	      byte_put (apply_to->s_ + offset, value, 2);
	    else
	      tree_builder->e_->error("RL78 sym diff reloc contains invalid offset: "
		       "%lX\n", offset);
	    value = 0;
	    return true;

	  default:
	    break;
	  }
	break;
      }
    }

  return false;
}

void ElfFile::byte_put(const unsigned char *c, uint64_t value, unsigned int size)
{
  if ( size > sizeof(uint64_t)) {
    tree_builder->e_->error("byte_put: bad size %d\n", size);
    return;
  }
  unsigned char *field = (unsigned char *)c;
  if ( m_lsb )
   while( size-- )
   {
    *field++ = value & 0xff;
    value >>= 8;
   }
  else while ( size-- )
  {
    field[size] = value & 0xff;
    value >>= 8;
  }
}

bool ElfFile::try_apply_debug_relocs()
{
 // fill map with loaded debug sections
 // .debug_abbrev & .debug_str should be ignored 
 // see last bool field in dwaft.c table debug_displays
 std::map<Elf_Half, dwarf_section *> rmaps;
 if ( !debug_info_.empty() )
   rmaps[debug_info_.idx] = &debug_info_;
 if ( !debug_loclists_.empty() )
   rmaps[debug_loclists_.idx] = &debug_loclists_;
 if ( !debug_addr_.empty() )
   rmaps[debug_addr_.idx] = &debug_addr_;
 if ( !debug_frame_.empty() )
   rmaps[debug_frame_.idx] = &debug_frame_;
 if ( !debug_ranges_.empty() )
   rmaps[debug_ranges_.idx] = &debug_ranges_;
 if ( !debug_rnglists_.empty() )
   rmaps[debug_rnglists_.idx] = &debug_rnglists_;
 if ( !debug_loc_.empty() )
   rmaps[debug_loc_.idx] = &debug_loc_;
 if ( !debug_line_.empty() )
   rmaps[debug_line_.idx] = &debug_line_;
 if ( !debug_line_str_.empty() )
   rmaps[debug_line_str_.idx] = &debug_line_str_;
 if ( rmaps.empty() ) return true;
 // ok, enum reloc sections
 std::list<Elf_Half> rs;
 Elf_Half n = reader.sections.size();
 section *sym_sec = nullptr;
 for ( Elf_Half i = 0; i < n; i++) {
   section *s = reader.sections[i];
   if ( s->get_type() == SHT_SYMTAB ) { sym_sec = s; continue; }
   if ( s->get_type() == SHT_REL || s->get_type() == SHT_RELA )
   {
     auto inf = s->get_info();
     auto si = rmaps.find(inf);
     if ( si == rmaps.end() ) continue;
     if ( g_opt_v )
       printf("section(%d) %s has relocs\n", inf, reader.sections[inf]->get_name().c_str());
     rs.push_back(i);
   }
 }
 if ( rs.empty() ) return true;
 if ( !sym_sec ) {
   tree_builder->e_->error("Cannot find symtab\n");
   return false;
 }
 had_relocs = true;
 auto machine = reader.get_machine();
 if ( machine == EM_MSP430 )
   uses_msp430x_relocs = (reader.get_flags() & EF_MSP430_MACH) == E_MSP430_MACH_MSP430X;
 // fill symbols
 symbol_section_accessor symbols( reader, sym_sec );
 Elf_Xword sym_no = symbols.get_symbols_num();
 if ( !sym_no ) {
   tree_builder->e_->error("no symbols\n");
   return false;
 } else {
    m_symbols.resize(sym_no);
    for ( Elf_Xword i = 0; i < sym_no; ++i )
    {
      std::string name;
      auto &curr_sym = m_symbols[i];
      symbols.get_symbol( i, name,
        curr_sym.addr, curr_sym.size, curr_sym.bind, curr_sym.type, curr_sym.section, curr_sym.other);
    }
  }
 unsigned int prev_reloc = 0; 
 // apply all reloc sections from rs list
 for ( auto irs: rs )
 {
   section *cr = reader.sections[irs];
   auto inf = cr->get_info();
   auto si = rmaps.find(inf);
   if ( si == rmaps.end() ) continue;
   apply_to = si->second;
   bool is_rela = cr->get_type() == SHT_RELA;
   if ( machine == EM_SH) is_rela = false;
   relocation_section_accessor ac(reader, cr);
   int num = ac.get_entries_num();
   if ( g_opt_d )
     printf("reloc section %d %s has %d entries, dest %d (%s)\n", irs, cr->get_name().c_str(),
       num, inf, reader.sections[inf]->get_name().c_str());
   Elf_Half prev_warn = 0;
   for ( int i = 0; i < num; ++i )
   {
     Elf64_Addr offset = 0;
     Elf_Word sym_idx = 0;
     unsigned rtype = 0;
     Elf_Sxword add = 0;
     ac.get_entry(i, offset, sym_idx, rtype, add);
     unsigned int reloc_size = 0;
     bool reloc_inplace = false;
     bool reloc_subtract = false;
     unsigned int reloc_type = get_reloc_type(rtype);
     if (target_specific_reloc_handling (machine, offset, sym_idx, rtype, add))
       continue;
	   else if (is_none_reloc (machine, reloc_type))
	    continue;
	   else if (is_32bit_abs_reloc (machine, reloc_type, prev_warn, tree_builder->e_)
		   || is_32bit_pcrel_reloc (machine, reloc_type))
	    reloc_size = 4;
	   else if (is_64bit_abs_reloc (machine, reloc_type)
		   || is_64bit_pcrel_reloc (machine, reloc_type))
	    reloc_size = 8;
	   else if (is_24bit_abs_reloc (machine, reloc_type))
	    reloc_size = 3;
	   else if (is_16bit_abs_reloc (machine, reloc_type, uses_msp430x_relocs))
	    reloc_size = 2;
	   else if (is_8bit_abs_reloc (machine, reloc_type)
		   || is_6bit_abs_reloc (machine, reloc_type))
	    reloc_size = 1;
	   else if ((reloc_subtract = is_32bit_inplace_sub_reloc (machine, reloc_type))
		   || is_32bit_inplace_add_reloc (machine, reloc_type))
	    {
	      reloc_size = 4;
	      reloc_inplace = true;
	    }
	  else if ((reloc_subtract = is_64bit_inplace_sub_reloc (machine, reloc_type))
		   || is_64bit_inplace_add_reloc (machine, reloc_type))
	    {
	      reloc_size = 8;
	      reloc_inplace = true;
	    }
	  else if ((reloc_subtract = is_16bit_inplace_sub_reloc (machine, reloc_type))
		   || is_16bit_inplace_add_reloc (machine, reloc_type))
	    {
	      reloc_size = 2;
	      reloc_inplace = true;
	    }
	  else if ((reloc_subtract = is_8bit_inplace_sub_reloc (machine, reloc_type))
		   || is_8bit_inplace_add_reloc (machine, reloc_type))
	    {
	      reloc_size = 1;
	      reloc_inplace = true;
	    }
	  else if ((reloc_subtract = is_6bit_inplace_sub_reloc (machine, reloc_type))
		   || is_6bit_inplace_add_reloc (machine, reloc_type))
	    {
	      reloc_size = 1;
	      reloc_inplace = true;
	    }
	  else
	    {
	      if (reloc_type != prev_reloc)
	        tree_builder->e_->error("unable to apply unsupported reloc idx %d type %d to section %s\n",
		      reloc_type, i, reader.sections[inf]->get_name().c_str());
	      prev_reloc = reloc_type;
	      continue;
	    }
    const unsigned char *rloc = apply_to->s_ + offset;
	  if ( !apply_to->in_section(offset, reloc_size) )
	    {
	      tree_builder->e_->error("skipping invalid relocation offset %lX in section %s\n",
		      offset, reader.sections[inf]->get_name().c_str());
	      continue;
	    }

	  if (sym_idx >= sym_no)
	    {
	      tree_builder->e_->error("skipping invalid relocation symbol index %d in section %s\n",
		      sym_idx, reader.sections[inf]->get_name().c_str());
	      continue;
	    }
    if ( m_symbols[sym_idx].type != STT_COMMON &&
         m_symbols[sym_idx].type > STT_SECTION
       )
      {
        tree_builder->e_->error("skipping unexpected symbol type %d in section %s relocation %d\n",
          m_symbols[sym_idx].type, reader.sections[inf]->get_name().c_str(), i);
        continue;
      }
    uint64_t addend = 0;
	  if (is_rela)
	    addend += add;
	  /* R_XTENSA_32, R_PJ_DATA_DIR32 and R_D30V_32_NORMAL are
	     partial_inplace.  */
	  if (!is_rela
	      || (machine == EM_XTENSA && reloc_type == 1)
	      || ((machine == EM_PJ || machine == EM_PJ_OLD) && reloc_type == 1)
	      || ((machine == EM_D30V || machine == EM_CYGNUS_D30V) && reloc_type == 12)
	      || reloc_inplace)
	    {
	      if (is_6bit_inplace_sub_reloc (machine, reloc_type))
	        addend += byte_get (rloc, reloc_size) & 0x3f;
	      else
	        addend += byte_get (rloc, reloc_size);
	    }
     if (is_32bit_pcrel_reloc (machine, reloc_type)
	      || is_64bit_pcrel_reloc (machine, reloc_type))
	    {
	      /* On HPPA, all pc-relative relocations are biased by 8.  */
	      if (machine == EM_PARISC)
	        addend -= 8;
	      byte_put (rloc, (addend + m_symbols[sym_idx].addr) - offset, reloc_size);
	    }
	  else if (is_6bit_abs_reloc (machine, reloc_type)
		   || is_6bit_inplace_sub_reloc (machine, reloc_type)
		   || is_6bit_inplace_add_reloc (machine, reloc_type))
	    {
	      if (reloc_subtract)
	        addend -= m_symbols[sym_idx].addr;
	      else
	        addend += m_symbols[sym_idx].addr;
	      addend = (addend & 0x3f) | (byte_get (rloc, reloc_size) & 0xc0);
	      byte_put (rloc, addend, reloc_size);
	    }
	  else if (reloc_subtract)
	    byte_put (rloc, addend - m_symbols[sym_idx].addr, reloc_size);
	  else
	    byte_put (rloc, addend + m_symbols[sym_idx].addr, reloc_size);
   }
   m_symbols.clear();
   apply_to = nullptr;
   reset_target_specific_reloc();
 }
 return true;
}