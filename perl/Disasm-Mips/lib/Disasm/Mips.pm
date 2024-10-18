package Disasm::Mips;

use 5.030000;
use strict;
use warnings;

require Exporter;
use AutoLoader qw(AUTOLOAD);

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Disasm::Mips ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

use constant {
 INVALID => 0,
 ABS_D => 1,
 ABS_PS => 2,
 ABS_S => 3,
 ADD_D => 4,
 ADD_PS => 5,
 ADD_S => 6,
 ADD => 7,
 ADDI => 8,
 ADDIU => 9,
 ADDR => 10,
 ADDU => 11,
 ALIGN => 12,
 ALNV_PS => 13,
 AND => 14,
 ANDI => 15,
 B => 16,
 BAL => 17,
 BC1ANY2 => 18,
 BC1ANY4 => 19,
 BC1EQZ => 20,
 BC1F => 21,
 BC1FL => 22,
 BC1NEZ => 23,
 BC1T => 24,
 BC1TL => 25,
 BC2EQZ => 26,
 BC2F => 27,
 BC2FL => 28,
 BC2NEZ => 29,
 BC2T => 30,
 BC2TL => 31,
 BCNEZ => 32,
 BEQ => 33,
 BEQL => 34,
 BEQZ => 35,
 BGEZ => 36,
 BGEZAL => 37,
 BGEZALL => 38,
 BGEZL => 39,
 BGTZ => 40,
 BGTZL => 41,
 BITSWAP => 42,
 BLEZ => 43,
 BLEZL => 44,
 BLTZ => 45,
 BLTZAL => 46,
 BLTZALL => 47,
 BLTZL => 48,
 BNE => 49,
 BNEL => 50,
 BNZ_B => 51,
 BNZ_D => 52,
 BNZ_H => 53,
 BNZ_W => 54,
 BREAK => 55,
 BSHFL => 56,
 BZ_B => 57,
 BZ_D => 58,
 BZ_H => 59,
 BZ_W => 60,
 C_DF_D => 61,
 C_EQ_D => 62,
 C_EQ_PS => 63,
 C_EQ_S => 64,
 C_EQ => 65,
 C_F_D => 66,
 C_F_PS => 67,
 C_F_S => 68,
 C_F => 69,
 C_LE_D => 70,
 C_LE_PS => 71,
 C_LE_S => 72,
 C_LE => 73,
 C_LT_D => 74,
 C_LT_PS => 75,
 C_LT_S => 76,
 C_LT => 77,
 C_NGE_D => 78,
 C_NGE_PS => 79,
 C_NGE_S => 80,
 C_NGE => 81,
 C_NGL_D => 82,
 C_NGL_PS => 83,
 C_NGL_S => 84,
 C_NGL => 85,
 C_NGLE_D => 86,
 C_NGLE_PS => 87,
 C_NGLE_S => 88,
 C_NGLE => 89,
 C_NGT_D => 90,
 C_NGT_PS => 91,
 C_NGT_S => 92,
 C_NGT => 93,
 C_OLE_D => 94,
 C_OLE_PS => 95,
 C_OLE_S => 96,
 C_OLE => 97,
 C_OLT_D => 98,
 C_OLT_PS => 99,
 C_OLT_S => 100,
 C_OLT => 101,
 C_SEQ_D => 102,
 C_SEQ_PS => 103,
 C_SEQ_S => 104,
 C_SEQ => 105,
 C_SF_D => 106,
 C_SF_PS => 107,
 C_SF_S => 108,
 C_SF => 109,
 C_UEQ_D => 110,
 C_UEQ_PS => 111,
 C_UEQ_S => 112,
 C_UEQ => 113,
 C_ULE_D => 114,
 C_ULE_PS => 115,
 C_ULE_S => 116,
 C_ULE => 117,
 C_ULT_D => 118,
 C_ULT_PS => 119,
 C_ULT_S => 120,
 C_ULT => 121,
 C_UN_D => 122,
 C_UN_PS => 123,
 C_UN_S => 124,
 C_UN => 125,
 C1 => 126,
 C2 => 127,
 CACHE => 128,
 CEIL_L_D => 129,
 CEIL_L_S => 130,
 CEIL_L => 131,
 CEIL_W_D => 132,
 CEIL_W_S => 133,
 CEIL_W => 134,
 CFC0 => 135,
 CFC1 => 136,
 CFC2 => 137,
 CLASS_D => 138,
 CLASS_S => 139,
 CLO => 140,
 CLZ => 141,
 COP0 => 142,
 COP1 => 143,
 COP1X => 144,
 COP2 => 145,
 COP3 => 146,
 CTC0 => 147,
 CTC1 => 148,
 CTC2 => 149,
 CVT_D_S => 150,
 CVT_D_W => 151,
 CVT_L_D => 152,
 CVT_L_S => 153,
 CVT_L => 154,
 CVT_PS_PW => 155,
 CVT_PS_S => 156,
 CVT_PS => 157,
 CVT_PW_PS => 158,
 CVT_S_D => 159,
 CVT_S_L => 160,
 CVT_S_PL => 161,
 CVT_S_PU => 162,
 CVT_S_W => 163,
 CVT_W_D => 164,
 CVT_W_S => 165,
 CVT_W => 166,
 DADD => 167,
 DADDI => 168,
 DADDIU => 169,
 DADDU => 170,
 DDIV => 171,
 DDIVU => 172,
 DERET => 173,
 DI => 174,
 DIV_D => 175,
 DIV_PS => 176,
 DIV_S => 177,
 DIV => 178,
 DIVU => 179,
 DMFC0 => 180,
 DMFC1 => 181,
 DMFC2 => 182,
 DMULT => 183,
 DMULTU => 184,
 DMTC0 => 185,
 DMTC1 => 186,
 DMTC2 => 187,
 DRET => 188,
 DSLL => 189,
 DSLL32 => 190,
 DSLLV => 191,
 DSLV => 192,
 DSRA => 193,
 DSRA32 => 194,
 DSRAV => 195,
 DSRL => 196,
 DSRL32 => 197,
 DSUB => 198,
 DSUBU => 199,
 EHB => 200,
 EI => 201,
 ERET => 202,
 EXT => 203,
 FLOOR_L_D => 204,
 FLOOR_L_S => 205,
 FLOOR_L => 206,
 FLOOR_W_D => 207,
 FLOOR_W_S => 208,
 FLOOR_W => 209,
 INS => 210,
 J => 211,
 JAL => 212,
 JALR_HB => 213,
 JALR => 214,
 JALX => 215,
 JR_HB => 216,
 JR => 217,
 LB => 218,
 LBU => 219,
 LBUX => 220,
 LD => 221,
 LDC1 => 222,
 LDC2 => 223,
 LDC3 => 224,
 LDL => 225,
 LDR => 226,
 LDXC1 => 227,
 LH => 228,
 LHI => 229,
 LHU => 230,
 LHX => 231,
 LI => 232,
 LL => 233,
 LLD => 234,
 LLO => 235,
 LUI => 236,
 LUXC1 => 237,
 LW => 238,
 LWC1 => 239,
 LWC2 => 240,
 LWC3 => 241,
 LWL => 242,
 LWR => 243,
 LWU => 244,
 LWX => 245,
 LWXC1 => 246,
 LX => 247,
 MADD_D => 248,
 MADD_PS => 249,
 MADD_S => 250,
 MADD => 251,
 MADDF_D => 252,
 MADDF_S => 253,
 MADDU => 254,
 MFC0 => 255,
 MFC1 => 256,
 MFC2 => 257,
 MFHC1 => 258,
 MFHC2 => 259,
 MFHI => 260,
 MFLO => 261,
 MOV_D => 262,
 MOV_PS => 263,
 MOV_S => 264,
 MOVCF => 265,
 MOVCI => 266,
 MOVE => 267,
 MOVF_D => 268,
 MOVF_PS => 269,
 MOVF_S => 270,
 MOVF => 271,
 MOVN_D => 272,
 MOVN_PS => 273,
 MOVN_S => 274,
 MOVN => 275,
 MOVT_D => 276,
 MOVT_PS => 277,
 MOVT_S => 278,
 MOVT => 279,
 MOVZ_D => 280,
 MOVZ_PS => 281,
 MOVZ_S => 282,
 MOVZ => 283,
 MSUB_D => 284,
 MSUB_PS => 285,
 MSUB_S => 286,
 MSUB => 287,
 MSUBF_D => 288,
 MSUBF_S => 289,
 MSUBU => 290,
 MTC0 => 291,
 MTC1 => 292,
 MTC2 => 293,
 MTHC1 => 294,
 MTHC2 => 295,
 MTHI => 296,
 MTLO => 297,
 MUL_D => 298,
 MUL_PS => 299,
 MUL_S => 300,
 MUL => 301,
 MULR => 302,
 MULT => 303,
 MULTU => 304,
 NEG_D => 305,
 NEG_PS => 306,
 NEG_S => 307,
 NEG => 308,
 NEGU => 309,
 NMADD_D => 310,
 NMADD_PS => 311,
 NMADD_S => 312,
 NMSUB_D => 313,
 NMSUB_PS => 314,
 NMSUB_S => 315,
 NOP => 316,
 NOR => 317,
 NOT => 318,
 OR => 319,
 ORI => 320,
 PAUSE => 321,
 PLL_PS => 322,
 PLU_PS => 323,
 PREF => 324,
 PREFX => 325,
 PUL_PS => 326,
 PUU_PS => 327,
 RDHWR => 328,
 RDPGPR => 329,
 RECIP_D => 330,
 RECIP_S => 331,
 RECIP => 332,
 RECIP1 => 333,
 RECIP2 => 334,
 RINT_D => 335,
 RINT_S => 336,
 ROTR => 337,
 ROTRV => 338,
 ROUND_L_D => 339,
 ROUND_L_S => 340,
 ROUND_L => 341,
 ROUND_W_D => 342,
 ROUND_W_S => 343,
 ROUND_W => 344,
 RSQRT_D => 345,
 RSQRT_S => 346,
 RSQRT => 347,
 RSQRT1 => 348,
 RSQRT2 => 349,
 SB => 350,
 SC => 351,
 SCD => 352,
 SD => 353,
 SDBBP => 354,
 SDC1 => 355,
 SDC2 => 356,
 SDC3 => 357,
 SDL => 358,
 SDR => 359,
 SDXC1 => 360,
 SEB => 361,
 SEH => 362,
 SEL_D => 363,
 SEL_S => 364,
 SH => 365,
 SLL => 366,
 SLLV => 367,
 SLT => 368,
 SLTI => 369,
 SLTIU => 370,
 SLTU => 371,
 SQRT_D => 372,
 SQRT_PS => 373,
 SQRT_S => 374,
 SRA => 375,
 SRAV => 376,
 SRL => 377,
 SRLV => 378,
 SSNOP => 379,
 SUB_D => 380,
 SUB_PS => 381,
 SUB_S => 382,
 SUB => 383,
 SUBU => 384,
 SUXC1 => 385,
 SW => 386,
 SWC1 => 387,
 SWC2 => 388,
 SWC3 => 389,
 SWL => 390,
 SWR => 391,
 SWXC1 => 392,
 SYNC => 393,
 SYNCI => 394,
 SYSCALL => 395,
 TEQ => 396,
 TEQI => 397,
 TGE => 398,
 TGEI => 399,
 TGEIU => 400,
 TGEU => 401,
 TLBP => 402,
 TLBR => 403,
 TLBWI => 404,
 TLBWR => 405,
 TLT => 406,
 TLTI => 407,
 TLTIU => 408,
 TLTU => 409,
 TNE => 410,
 TNEI => 411,
 TRAP => 412,
 TRUNC_L_D => 413,
 TRUNC_L_S => 414,
 TRUNC_L => 415,
 TRUNC_W_D => 416,
 TRUNC_W_S => 417,
 TRUNC_W => 418,
 WAIT => 419,
 WRPGPR => 420,
 WSBH => 421,
 XOR => 422,
 XORI => 423,
};
###
#
# E X P O R T E D   N A M E S
#
###
our @EXPORT = qw(
INVALID
ABS_D
ABS_PS
ABS_S
ADD_D
ADD_PS
ADD_S
ADD
ADDI
ADDIU
ADDR
ADDU
ALIGN
ALNV_PS
AND
ANDI
B
BAL
BC1ANY2
BC1ANY4
BC1EQZ
BC1F
BC1FL
BC1NEZ
BC1T
BC1TL
BC2EQZ
BC2F
BC2FL
BC2NEZ
BC2T
BC2TL
BCNEZ
BEQ
BEQL
BEQZ
BGEZ
BGEZAL
BGEZALL
BGEZL
BGTZ
BGTZL
BITSWAP
BLEZ
BLEZL
BLTZ
BLTZAL
BLTZALL
BLTZL
BNE
BNEL
BNZ_B
BNZ_D
BNZ_H
BNZ_W
BREAK
BSHFL
BZ_B
BZ_D
BZ_H
BZ_W
C_DF_D
C_EQ_D
C_EQ_PS
C_EQ_S
C_EQ
C_F_D
C_F_PS
C_F_S
C_F
C_LE_D
C_LE_PS
C_LE_S
C_LE
C_LT_D
C_LT_PS
C_LT_S
C_LT
C_NGE_D
C_NGE_PS
C_NGE_S
C_NGE
C_NGL_D
C_NGL_PS
C_NGL_S
C_NGL
C_NGLE_D
C_NGLE_PS
C_NGLE_S
C_NGLE
C_NGT_D
C_NGT_PS
C_NGT_S
C_NGT
C_OLE_D
C_OLE_PS
C_OLE_S
C_OLE
C_OLT_D
C_OLT_PS
C_OLT_S
C_OLT
C_SEQ_D
C_SEQ_PS
C_SEQ_S
C_SEQ
C_SF_D
C_SF_PS
C_SF_S
C_SF
C_UEQ_D
C_UEQ_PS
C_UEQ_S
C_UEQ
C_ULE_D
C_ULE_PS
C_ULE_S
C_ULE
C_ULT_D
C_ULT_PS
C_ULT_S
C_ULT
C_UN_D
C_UN_PS
C_UN_S
C_UN
C1
C2
CACHE
CEIL_L_D
CEIL_L_S
CEIL_L
CEIL_W_D
CEIL_W_S
CEIL_W
CFC0
CFC1
CFC2
CLASS_D
CLASS_S
CLO
CLZ
COP0
COP1
COP1X
COP2
COP3
CTC0
CTC1
CTC2
CVT_D_S
CVT_D_W
CVT_L_D
CVT_L_S
CVT_L
CVT_PS_PW
CVT_PS_S
CVT_PS
CVT_PW_PS
CVT_S_D
CVT_S_L
CVT_S_PL
CVT_S_PU
CVT_S_W
CVT_W_D
CVT_W_S
CVT_W
DADD
DADDI
DADDIU
DADDU
DDIV
DDIVU
DERET
DI
DIV_D
DIV_PS
DIV_S
DIV
DIVU
DMFC0
DMFC1
DMFC2
DMULT
DMULTU
DMTC0
DMTC1
DMTC2
DRET
DSLL
DSLL32
DSLLV
DSLV
DSRA
DSRA32
DSRAV
DSRL
DSRL32
DSUB
DSUBU
EHB
EI
ERET
EXT
FLOOR_L_D
FLOOR_L_S
FLOOR_L
FLOOR_W_D
FLOOR_W_S
FLOOR_W
INS
J
JAL
JALR_HB
JALR
JALX
JR_HB
JR
LB
LBU
LBUX
LD
LDC1
LDC2
LDC3
LDL
LDR
LDXC1
LH
LHI
LHU
LHX
LI
LL
LLD
LLO
LUI
LUXC1
LW
LWC1
LWC2
LWC3
LWL
LWR
LWU
LWX
LWXC1
LX
MADD_D
MADD_PS
MADD_S
MADD
MADDF_D
MADDF_S
MADDU
MFC0
MFC1
MFC2
MFHC1
MFHC2
MFHI
MFLO
MOV_D
MOV_PS
MOV_S
MOVCF
MOVCI
MOVE
MOVF_D
MOVF_PS
MOVF_S
MOVF
MOVN_D
MOVN_PS
MOVN_S
MOVN
MOVT_D
MOVT_PS
MOVT_S
MOVT
MOVZ_D
MOVZ_PS
MOVZ_S
MOVZ
MSUB_D
MSUB_PS
MSUB_S
MSUB
MSUBF_D
MSUBF_S
MSUBU
MTC0
MTC1
MTC2
MTHC1
MTHC2
MTHI
MTLO
MUL_D
MUL_PS
MUL_S
MUL
MULR
MULT
MULTU
NEG_D
NEG_PS
NEG_S
NEG
NEGU
NMADD_D
NMADD_PS
NMADD_S
NMSUB_D
NMSUB_PS
NMSUB_S
NOP
NOR
NOT
OR
ORI
PAUSE
PLL_PS
PLU_PS
PREF
PREFX
PUL_PS
PUU_PS
RDHWR
RDPGPR
RECIP_D
RECIP_S
RECIP
RECIP1
RECIP2
RINT_D
RINT_S
ROTR
ROTRV
ROUND_L_D
ROUND_L_S
ROUND_L
ROUND_W_D
ROUND_W_S
ROUND_W
RSQRT_D
RSQRT_S
RSQRT
RSQRT1
RSQRT2
SB
SC
SCD
SD
SDBBP
SDC1
SDC2
SDC3
SDL
SDR
SDXC1
SEB
SEH
SEL_D
SEL_S
SH
SLL
SLLV
SLT
SLTI
SLTIU
SLTU
SQRT_D
SQRT_PS
SQRT_S
SRA
SRAV
SRL
SRLV
SSNOP
SUB_D
SUB_PS
SUB_S
SUB
SUBU
SUXC1
SW
SWC1
SWC2
SWC3
SWL
SWR
SWXC1
SYNC
SYNCI
SYSCALL
TEQ
TEQI
TGE
TGEI
TGEIU
TGEU
TLBP
TLBR
TLBWI
TLBWR
TLT
TLTI
TLTIU
TLTU
TNE
TNEI
TRAP
TRUNC_L_D
TRUNC_L_S
TRUNC_L
TRUNC_W_D
TRUNC_W_S
TRUNC_W
WAIT
WRPGPR
WSBH
XOR
XORI
);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('Disasm::Mips', $VERSION);

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

Disasm::Mips - Perl extension for blah blah blah

=head1 SYNOPSIS

  use Disasm::Mips;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for Disasm::Mips, created by h2xs. It looks like the
author of the extension was negligent enough to leave the stub
unedited.

Blah blah blah.

=head2 EXPORT

None by default.



=head1 SEE ALSO

Mention other useful documentation such as the documentation of
related modules or operating system documentation (such as man pages
in UNIX), or any relevant external documentation such as RFCs or
standards.

If you have a mailing list set up for your module, mention it here.

If you have a web site set up for your module, mention it here.

=head1 AUTHOR

redp, E<lt>redp@E<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2024 by redp

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.30.0 or,
at your option, any later version of Perl 5 you may have available.


=cut
