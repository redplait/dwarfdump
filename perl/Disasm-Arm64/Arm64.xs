// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "elfio/elfio.hpp"
#include "../elf.inc"
#include "armadillo.h"

void my_warn(const char * pat, ...) {
 va_list args;
 va_start(args, pat);
 vwarn(pat, &args);
}

struct arm_reg
{
  long val;
  int ldr; // base register for ldr[bh] reg, [base + xxx]

  arm_reg()
   : val(0),
     ldr(-1)
   { }
  inline void reset()
  {
    val = 0;
    ldr = -1;
  }
};

static const char *s_regpad = "Disasm::Arm64::Regpad";

struct regs_pad {
   regs_pad() = default;
   regs_pad(const regs_pad &) = default;
   void reset()
   {
     for ( int i = 0; i < AD_REG_SP; i++ ) m_regs[i].reset();
   }
   // mark x0..x7 ldr - see https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst#machine-registers
   void mark_abi() {
    for ( int i = 0; i < 8; i++ )
      m_regs[i].ldr = i;
   }
   inline void zero(int reg)
   {
     if ( reg >= AD_REG_SP || reg < 0 )
       return;
     m_regs[reg].reset();
   }
   inline long get(int reg)
   {
     if ( reg >= AD_REG_SP || reg < 0 ) // hm
       return 0;
     return m_regs[reg].val;
   }
   inline int ldr(int reg) {
     if ( reg >= AD_REG_SP || reg < 0 ) // hm
       return -1;
     return m_regs[reg].ldr;
   }
   // http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0802a/ADRP.html
   long adrp(int reg, long val)
   {
     if ( reg >= AD_REG_SP || reg < 0 )
       return 0;
     m_regs[reg].val = val;
     m_regs[reg].ldr = -1;
     return val;
   }
   // http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0802a/a64_general_alpha.html
   long add(int reg1, int reg2, long val)
   {
     if ( (reg1 >= AD_REG_SP) || (reg1 < 0) || (reg2 >= AD_REG_SP) || (reg2 < 0) )
       return 0;
     m_regs[reg1].ldr = -1;
     if ( !m_regs[reg2].val )
       return 0;
     m_regs[reg1].val = m_regs[reg2].val + val;
     if ( reg1 != reg2 )
       m_regs[reg2].reset();
     return m_regs[reg1].val;
   }
   int mov(int reg1, int reg2)
   {
     if ( (reg1 >= AD_REG_SP) || (reg1 < 0) || (reg2 >= AD_REG_SP) || (reg2 < 0) )
       return 0;
     m_regs[reg1].ldr = -1;
     if ( !m_regs[reg2].val )
     {
       m_regs[reg1].ldr = m_regs[reg2].ldr;
       return 0;
     }
     m_regs[reg1] = m_regs[reg2];
     return 1;
   }
   // data
   arm_reg m_regs[AD_REG_SP];
};

struct adis: public ad_insn {
  const char *psp, *end;
  unsigned long addr, start;
  IElf *e;
  bool succ;
  adis(IElf *_e): e(_e)
  { start = addr = 0;
    psp = end = nullptr;
    succ = false;
  }
  ~adis() {
    if ( e ) e->release();
  }
  bool inline empty() const {
    return !psp || !succ;
  }
  bool inline setuped() const {
    return psp;
  }
  // setups
  int setup(unsigned long addr_)
  {
   psp = end = nullptr;
   start = addr = addr_;
   auto *s = find_section(e, addr_);
   if ( !s ) {
     my_warn("cannot find section for address %lX", addr_);
     return 0;
   }
   size_t diff = addr - s->get_address();
   psp = s->get_data() + diff;
   end = s->get_data() + s->get_size();
   succ = false;
   return 1;
  }
  // like setup but also with explicit len, anyway need to check if this len can fit into some section
  int setup(unsigned long addr_, unsigned long len_)
  {
   psp = end = nullptr;
   start = addr = addr_;
   auto *s = find_section(e, addr_);
   if ( !s ) {
     my_warn("cannot find section for address %lX", addr_);
     return 0;
   }
   size_t diff = addr - s->get_address();
   unsigned long rem_len = s->get_size() - diff;
   rem_len = std::min(rem_len, len_);
   psp = s->get_data() + diff;
   end = psp + rem_len;
   succ = false; // empty after setup
   return 1;
  }
  // main method, return 1 if instruction was disassembled
  int disasm() {
    if ( !psp ) return 0;
    if ( psp >= end ) return 0;
    unsigned int value = *(unsigned int *)psp;
    if ( e->needswap ) value = __builtin_bswap32(value);
    if ( ArmadilloDisassemble(value, (uint64)addr, this) ) {
      succ = false;
      return 0;
    }
    succ = true;
    psp += 4;
    addr += 4;
    // check for end
    if ( instr_id == AD_INSTR_UDF ||
         instr_id == AD_INSTR_BRK ||
         instr_id == AD_INSTR_RET ||
         instr_id == AD_INSTR_BR  || is_braa() ||
         (instr_id == AD_INSTR_B && cc == AD_NONE)
       )
    {
      end = psp;
    }
    return 1;
  }
  // Branch to Register, with pointer authentication
  // details https://developer.arm.com/documentation/dui0801/l/A64-General-Instructions/BRAA--BRAAZ--BRAB--BRABZ--A64-?lang=en
  int is_braa() const
  {
    return (instr_id == AD_INSTR_BRAA) ||
     (instr_id == AD_INSTR_BRAAZ) ||
     (instr_id == AD_INSTR_BRAB) ||
     (instr_id == AD_INSTR_BRABZ)
    ;
  }
  // boring stuff
  int is_b_jimm(unsigned long &addr) const
  {
    if ( instr_id == AD_INSTR_B && cc == AD_NONE && num_operands == 1 && operands[0].type == AD_OP_IMM )
    {
      addr = operands[0].op_imm.bits;
      return 1;
    }
    return 0;
  }
  int is_tbz_jimm(unsigned long &addr) const
  {
    if ( instr_id == AD_INSTR_TBZ && num_operands == 3 && operands[2].type == AD_OP_IMM )
    {
      addr = operands[2].op_imm.bits;
      return 1;
    }
    return 0;
  }
  int is_tbnz_jimm(unsigned long &addr) const
  {
    if ( instr_id == AD_INSTR_TBNZ && num_operands == 3 && operands[2].type == AD_OP_IMM )
    {
      addr = operands[2].op_imm.bits;
      return 1;
    }
    return 0;
  }
  int is_cbz_jimm(unsigned long &addr) const
  {
    if ( instr_id == AD_INSTR_CBZ && num_operands == 2 && operands[1].type == AD_OP_IMM )
    {
      addr = operands[1].op_imm.bits;
      return 1;
    }
    return 0;
  }
  int is_cbnz_jimm(unsigned long &addr) const
  {
    if ( instr_id == AD_INSTR_CBNZ && num_operands == 2 && operands[1].type == AD_OP_IMM )
    {
      addr = operands[1].op_imm.bits;
      return 1;
    }
    return 0;
  }
  int is_bcc_jimm(unsigned long &addr) const
  {
    if ( instr_id == AD_INSTR_B && cc != AD_NONE && num_operands == 1 && operands[0].type == AD_OP_IMM )
    {
      addr = operands[0].op_imm.bits;
      return 1;
    } else
    return 0;
  }
  int is_jxx(unsigned long &addr) const
  {
    return is_b_jimm(addr) ||
     is_bcc_jimm(addr) ||
     is_tbz_jimm(addr) ||
     is_tbnz_jimm(addr) ||
     is_cbz_jimm(addr) ||
     is_cbnz_jimm(addr);
  }
  int is_bl_jimm() const
  {
     return ( instr_id == AD_INSTR_BL && num_operands == 1 && operands[0].type == AD_OP_IMM );
  }
  // reg machinery
  inline int is_add_r() const
  {
    return (instr_id == AD_INSTR_ADD) && (operands[0].type == AD_OP_REG);
  }
  inline int is_mov_rr() const
  {
     return (instr_id == AD_INSTR_MOV && num_operands == 2 && operands[0].type == AD_OP_REG && operands[1].type == AD_OP_REG);
  }
  inline int is_mov_rimm() const
  {
    return (instr_id == AD_INSTR_MOV && num_operands == 2 && operands[0].type == AD_OP_REG && operands[1].type == AD_OP_IMM);
  }
  inline int is_ldr0() const
  {
      return ( (instr_id == AD_INSTR_LDRAA || instr_id == AD_INSTR_LDRAB || instr_id == AD_INSTR_LDR)
               && num_operands == 2 && operands[0].type == AD_OP_REG && operands[1].type == AD_OP_REG
             );
  }
  int is_adr() const
  {
    return (instr_id == AD_INSTR_ADR) &&
           (num_operands == 2) &&
           (operands[0].type == AD_OP_REG) &&
           (operands[1].type == AD_OP_IMM)
    ;
  }
  int is_adrp() const
  {
    return (instr_id == AD_INSTR_ADRP) &&
           (num_operands == 2) &&
           (operands[0].type == AD_OP_REG) &&
           (operands[1].type == AD_OP_IMM)
    ;
  }
  int is_add() const
  {
    return (instr_id == AD_INSTR_ADD) && 
           (num_operands == 3) &&
           (operands[0].type == AD_OP_REG) &&
           (operands[1].type == AD_OP_REG) &&
           (operands[2].type == AD_OP_IMM)
    ;
  }
  int is_ldr_lsl() const
  {
    return (instr_id == AD_INSTR_LDR) && 
           (num_operands == 4) &&
           (operands[0].type == AD_OP_REG) &&
           (operands[1].type == AD_OP_REG) &&
           (operands[2].type == AD_OP_REG) &&
           (operands[3].type == AD_OP_IMM)
    ;
   }
   inline int is_rri() const
   {
     return (num_operands == 3) &&
            (operands[0].type == AD_OP_REG) &&
            (operands[1].type == AD_OP_REG) &&
            (operands[2].type == AD_OP_IMM)
     ;
   }
   inline int is_rr() const
   {
     return (num_operands >= 2) &&
            (operands[0].type == AD_OP_REG) &&
            (operands[1].type == AD_OP_REG)
     ;
   }
   int is_ldr() const
   {
     return (instr_id == AD_INSTR_LDR) && is_rri();
   }
   // 146% I forgot many of them
   inline int is_dst_reg() const
    {
      if ( operands[0].type != AD_OP_REG )
        return 0;
      switch(instr_id)
      {
        case AD_INSTR_LDTR:
        case AD_INSTR_LDTRB:
        case AD_INSTR_LDTRH:
        case AD_INSTR_LDRSW:
        case AD_INSTR_LDRSH:
        case AD_INSTR_LDAR:
        case AD_INSTR_LDARB:
        case AD_INSTR_LDARH:
        case AD_INSTR_LDRB:
        case AD_INSTR_LDRSB:
        case AD_INSTR_LDRH:
        case AD_INSTR_LDP:
        case AD_INSTR_LDUR:
        case AD_INSTR_LDURB:
        case AD_INSTR_LDURH:
        case AD_INSTR_LDURSW:
        case AD_INSTR_ADRP:
        case AD_INSTR_EON:
        case AD_INSTR_EOR:
        case AD_INSTR_ORR:
        case AD_INSTR_ORN:
        case AD_INSTR_AND:
        case AD_INSTR_ANDS:
        case AD_INSTR_MSUB:
        case AD_INSTR_SUB:
        case AD_INSTR_UMSUBL:
        case AD_INSTR_SUBS:
        case AD_INSTR_MOVK:
        case AD_INSTR_MADD:
        case AD_INSTR_ADDS:
        case AD_INSTR_ADC:
        case AD_INSTR_ADCS:
        case AD_INSTR_CMN:
        case AD_INSTR_STADD:
        case AD_INSTR_UMADDL:
        case AD_INSTR_SMADDL:
        case AD_INSTR_UDIV:
        case AD_INSTR_SDIV:
        case AD_INSTR_MUL:
        case AD_INSTR_UMULL:
        case AD_INSTR_UMULH:
        case AD_INSTR_SMULL:
        case AD_INSTR_SMULH:
        case AD_INSTR_SBFX:
        case AD_INSTR_SXTW:
        case AD_INSTR_SXTB:
        case AD_INSTR_SXTH:
        case AD_INSTR_CSEL:
        case AD_INSTR_MRS:
        case AD_INSTR_LSL:
        case AD_INSTR_LSLV:
        case AD_INSTR_LSR:
        case AD_INSTR_LSRV:
        case AD_INSTR_CSET:
        case AD_INSTR_CSETM:
        case AD_INSTR_UBFIZ:
        case AD_INSTR_SBFIZ:
        case AD_INSTR_BIC:
        case AD_INSTR_ASR:
        case AD_INSTR_ASRV:
        case AD_INSTR_BFI:
        case AD_INSTR_CNEG:
        case AD_INSTR_NEG:
        case AD_INSTR_NEGS:
        case AD_INSTR_CSNEG:
        case AD_INSTR_CSINC:
        case AD_INSTR_CINC:
        case AD_INSTR_CSINV:
        case AD_INSTR_CINV:
        case AD_INSTR_UBFX:
        case AD_INSTR_BFXIL:
        case AD_INSTR_ROR:
        case AD_INSTR_MVN:
        case AD_INSTR_CLS:
        case AD_INSTR_CLZ:
        case AD_INSTR_RBIT:
        case AD_INSTR_REV:
        case AD_INSTR_REV16:
        case AD_INSTR_EXTR:
        case AD_INSTR_LDADDAL:
        case AD_INSTR_LDADDL:
         return 1;
      }
      return 0;
    }
    inline int get_reg(int idx) const
    {
      return operands[idx].op_reg.rn;
    }
    unsigned long apply(regs_pad *rp) {
      if ( is_adr() ) {
        return rp->adrp(get_reg(0), operands[1].op_imm.bits);
      }
      if ( is_adrp() ) {
        return rp->adrp(get_reg(0), operands[1].op_imm.bits);
      }
      if ( is_add() ) {
        return rp->add(get_reg(0), get_reg(1), operands[2].op_imm.bits);
      }
      if ( is_mov_rr() ) {
        rp->mov(get_reg(0), get_reg(1));
        return 0;
      }
      // must be last
      if ( is_rr() && instr_id == AD_INSTR_LDP ) {
        rp->zero(get_reg(0)); rp->zero(get_reg(1));
        return 0;
      }
      if ( (num_operands > 1) && is_dst_reg() ) rp->zero(get_reg(0));
      return 0;
    }
    // return 1 if instr is ldrXX reg, [base + off],
    //        2 if instr is strXX reg, [base + off],
    // 0 otherwise
    int is_ls(regs_pad *rp, int &base, long &off)
    {
      int res = 0;
      if ( is_rri() ) {
        switch(instr_id) {
          case AD_INSTR_LDR:
          case AD_INSTR_LDRB:
          case AD_INSTR_LDRSB:
          case AD_INSTR_LDRH:
          case AD_INSTR_LDRSH:
          case AD_INSTR_LDRSW:
          case AD_INSTR_LDRAA:
          case AD_INSTR_LDRAB:
           res = 1;
           base = rp->ldr(get_reg(1));
           off = operands[2].op_imm.bits;
           rp->zero(get_reg(0));
           break;
          case AD_INSTR_STR:
          case AD_INSTR_STRB:
          case AD_INSTR_STRH:
           base = rp->ldr(get_reg(1));
           off = operands[2].op_imm.bits;
           res = 2;
           break;
        }
      } else if ( is_ldr0() ) {
        res = 1;
        base = rp->ldr(get_reg(1));
        off = 0;
        rp->zero(get_reg(0));
      }
      return res;
    }
};

template <typename T>
static int arm64_magic_free(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        auto *m = (T *)mg->mg_ptr;
        delete m;
        mg->mg_ptr= NULL;
    }
    return 0; // ignored anyway
}

// magic table for Disasm::Arm64
static MGVTBL Arm64_magic_vt = {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        arm64_magic_free<adis>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

static U32
rpad_magic_sizepack(pTHX_ SV *sv, MAGIC *mg)
{
  U32 res = 0;
  if (mg->mg_ptr) {
    res = AD_REG_SP;
  }
  return res;
}

// magic table for Disasm::Arm64::Regpad
static MGVTBL regpad_magic_vt = {
        0, /* get */
        0, /* write */
        rpad_magic_sizepack, /* length */
        0, /* clear */
        arm64_magic_free<regs_pad>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

adis *adis_get(SV *obj)
{
  SV *sv;
  MAGIC* magic;
 
  if (!sv_isobject(obj)) {
     if (die)
        croak("Not an object");
        return NULL;
  }
  sv= SvRV(obj);
  if (SvMAGICAL(sv)) {
     /* Iterate magic attached to this scalar, looking for one with our vtable */
     for (magic= SvMAGIC(sv); magic; magic = magic->mg_moremagic)
        if (magic->mg_type == PERL_MAGIC_ext && magic->mg_virtual == &Arm64_magic_vt)
          /* If found, the mg_ptr points to the fields structure. */
            return (adis*) magic->mg_ptr;
    }
  return NULL;
}

regs_pad *regpad_get(SV *obj)
{
  SV *sv;
  MAGIC* magic;
 
  if (!sv_isobject(obj)) {
     if (die)
        croak("Not an object");
        return NULL;
  }
  sv= SvRV(obj);
  if (SvMAGICAL(sv)) {
     /* Iterate magic attached to this scalar, looking for one with our vtable */
     for (magic= SvMAGIC(sv); magic; magic = magic->mg_moremagic)
        if (magic->mg_type == PERL_MAGIC_tied && magic->mg_virtual == &regpad_magic_vt)
          /* If found, the mg_ptr points to the fields structure. */
            return (regs_pad*) magic->mg_ptr;
    }
  return NULL;
}


#define EXPORT_ENUM(name, x) newCONSTSUB(stash, name, new_enum_dualvar(aTHX_ x, newSVpvs_share(name)));
static SV * new_enum_dualvar(pTHX_ IV ival, SV *name) {
        SvUPGRADE(name, SVt_PVNV);
        SvIV_set(name, ival);
        SvIOK_on(name);
        SvREADONLY_on(name);
        return name;
}


MODULE = Disasm::Arm64		PACKAGE = Disasm::Arm64

void
new(obj_or_pkg, SV *elsv)
  SV *obj_or_pkg
 INIT:
  HV *pkg = NULL;
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  struct IElf *e= extract(elsv);
  ELFIO::Elf_Half machine;
  adis *res = nullptr;
 PPCODE:
  // check what we have
  machine = e->rdr->get_machine();
  if ( machine != ELFIO::EM_AARCH64 ) {
    my_warn("new: not AArch64 elf");
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  if (SvPOK(obj_or_pkg) && (pkg= gv_stashsv(obj_or_pkg, 0))) {
    if (!sv_derived_from(obj_or_pkg, "Disasm::Arm64"))
        croak("Package %s does not derive from Disasm::Arm64", SvPV_nolen(obj_or_pkg));
    msv = newSViv(0);
    objref= sv_2mortal(newRV_noinc(msv));
    sv_bless(objref, pkg);
    ST(0)= objref;
  } else
    croak("new: first arg must be package name or blessed object");
  // make real disasm object
  e->add_ref();
  res = new adis( e );
// attach magic
  magic = sv_magicext(msv, NULL, PERL_MAGIC_ext, &Arm64_magic_vt, (const char*)res, 0);
#ifdef USE_ITHREADS
    magic->mg_flags |= MGf_DUP;
#endif
  XSRETURN(1);

void
setup(SV *sv, unsigned long addr)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   ST(0) = sv_2mortal( newSVuv( d->setup(addr) ) );
   XSRETURN(1);

void
setup2(SV *sv, unsigned long addr, unsigned long len)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   ST(0) = sv_2mortal( newSVuv( d->setup(addr, len) ) );
   XSRETURN(1);

void
disasm(SV *sv)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   ST(0) = sv_2mortal( newSVuv( d->disasm() ) );
   XSRETURN(1);

void
addr(SV *sv)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() )
     ST(0) = &PL_sv_undef;
   else
     ST(0) = sv_2mortal( newSVuv( d->addr - 4 ) );
   XSRETURN(1);

void
text(SV *sv)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else {
    ST(0) = sv_2mortal( newSVpv(d->decoded, strlen(d->decoded)) );
  }
  XSRETURN(1);

void
op(SV *sv)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else {
    ST(0) = sv_2mortal( newSViv(d->instr_id) );
  }
  XSRETURN(1);

void
op_num(SV *sv)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else {
    ST(0) = sv_2mortal( newSViv(d->num_operands) );
  }
  XSRETURN(1);

void
cc(SV *sv)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() || d->cc == AD_NONE )
    ST(0) = &PL_sv_undef;
  else {
    ST(0) = sv_2mortal( newSVuv(d->cc) );
  }
  XSRETURN(1);

void
idx(SV *sv)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else {
    ST(0) = sv_2mortal( newSViv(d->idx_kind) );
  }
  XSRETURN(1);

void
reg_name(SV *sv, int idx)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else if ( idx >= d->num_operands ) {
    warn("reg_name(%d) when only %d operands", idx, d->num_operands);
    ST(0) = &PL_sv_undef;
  } else if ( d->operands[idx].type != AD_OP_REG )
    ST(0) = &PL_sv_undef;
  else {
    const char *rname = d->operands[idx].op_reg.rtbl[d->operands[idx].op_reg.rn];
    if ( !rname )
      ST(0) = &PL_sv_undef;
    else
      sv_2mortal( newSVpv(rname, strlen(rname)) );
  }
  XSRETURN(1);

void
reg(SV *sv, int idx)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else if ( idx >= d->num_operands ) {
    warn("reg(%d) when only %d operands", idx, d->num_operands);
    ST(0) = &PL_sv_undef;
  } else if ( d->operands[idx].type != AD_OP_REG || d->operands[idx].op_reg.rn == AD_NONE)
    ST(0) = &PL_sv_undef;
  else
   sv_2mortal( newSViv(d->operands[idx].op_reg.rn) );
  XSRETURN(1);


void
fpreg(SV *sv, int idx)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else if ( idx >= d->num_operands ) {
    warn("fpreg(%d) when only %d operands", idx, d->num_operands);
    ST(0) = &PL_sv_undef;
  } else if ( d->operands[idx].type != AD_OP_REG || d->operands[idx].op_reg.fp == AD_NONE )
    ST(0) = &PL_sv_undef;
  else
   sv_2mortal( newSViv(d->operands[idx].op_reg.fp) );
  XSRETURN(1);

void
sysreg(SV *sv, int idx)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else if ( idx >= d->num_operands ) {
    warn("sysreg(%d) when only %d operands", idx, d->num_operands);
    ST(0) = &PL_sv_undef;
  } else if ( d->operands[idx].type != AD_OP_REG || d->operands[idx].op_reg.sysreg == AD_NONE )
    ST(0) = &PL_sv_undef;
  else
   sv_2mortal( newSViv(d->operands[idx].op_reg.sysreg) );
  XSRETURN(1);

void
reg_sz(SV *sv, int idx)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else if ( idx >= d->num_operands ) {
    warn("reg(%d) when only %d operands", idx, d->num_operands);
    ST(0) = &PL_sv_undef;
  } else if ( d->operands[idx].type != AD_OP_REG )
    ST(0) = &PL_sv_undef;
  else
   sv_2mortal( newSViv(d->operands[idx].op_reg.sz) );
  XSRETURN(1);

void
op_type(SV *sv, int idx)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
   else if ( idx >= d->num_operands ) {
    warn("op_type(%d) when only %d operands", idx, d->num_operands);
    ST(0) = &PL_sv_undef;
   } else
    ST(0) = sv_2mortal( newSViv(d->operands[idx].type) );
  XSRETURN(1);

void
op_shift(SV *sv, int idx)
 INIT:
   adis *d = adis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
   else if ( idx >= d->num_operands ) {
    warn("op_shift(%d) when only %d operands", idx, d->num_operands);
    ST(0) = &PL_sv_undef;
   } else if ( d->operands[idx].type != AD_OP_SHIFT )
    ST(0) = &PL_sv_undef;
  else {
    AV *av = newAV();
    mXPUSHs(newRV_noinc((SV*)av));
    av_push(av, newSViv( d->operands[idx].op_shift.type ));
    av_push(av, newSViv( d->operands[idx].op_shift.amt ));
  }
  XSRETURN(1);

void
op_imm(SV *sv, int idx)
 INIT:
   adis *d = adis_get(sv);
   float *f;
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
   else if ( idx >= d->num_operands ) {
    warn("op_imm(%d) when only %d operands", idx, d->num_operands);
    ST(0) = &PL_sv_undef;
   } else if ( d->operands[idx].type != AD_OP_SHIFT )
    ST(0) = &PL_sv_undef;
  else {
    switch(d->operands[idx].op_imm.type) {
      case AD_IMM_INT:
      case AD_IMM_LONG:
       ST(0) = newSViv((long)d->operands[idx].op_imm.bits);
       break;
      case AD_IMM_UINT:
      case AD_IMM_ULONG:
       ST(0) = newSVuv(d->operands[idx].op_imm.bits);
       break;
      case AD_IMM_FLOAT:
       f = (float *)&d->operands[idx].op_imm.bits;
       ST(0) = newSVnv( (double)*f );
       break;
      default:
       warn("op_imm(%d) unknown type %d", idx, d->operands[idx].op_imm.type);
       ST(0) = &PL_sv_undef;
    }
  }
  XSRETURN(1);

void
is_jxx(SV *sv)
 INIT:
   adis *d = adis_get(sv);
   unsigned long addr = 0;
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
   else if ( !d->is_jxx(addr) )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVuv(addr) );
  XSRETURN(1);

void
bl_jimm(SV *sv)
 INIT:
   adis *d = adis_get(sv);
   unsigned long addr = 0;
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
   else if ( !d->is_bl_jimm() )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVuv(d->operands[0].op_imm.bits) );
  XSRETURN(1);

void
regpad(SV *sv)
  INIT:
  HV *pkg = NULL;
  AV *fake = NULL;
  adis *d = adis_get(sv);
  regs_pad *rp = NULL;
  SV *objref= NULL;
  MAGIC* magic;
 PPCODE:
  if ( !(pkg = gv_stashpv(s_regpad, 0)) ) {
    croak("Package %s does not exists", s_regpad);
    XSRETURN(0);
  }
  rp = new regs_pad();
  // tie on array
  fake = newAV();
  objref = newRV_noinc((SV*)fake);
  sv_bless(objref, pkg);
  magic = sv_magicext((SV*)fake, NULL, PERL_MAGIC_tied, &regpad_magic_vt, (const char *)rp, 0);
  SvREADONLY_on((SV*)fake);
  ST(0) = objref;
  XSRETURN(1);

void
apply(SV *a, SV *r)
 INIT:
   adis *d = adis_get(a);
   regs_pad *rp = regpad_get(r);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVuv( (unsigned long)d->apply(rp) ) );
   XSRETURN(1);

void
is_ls(SV *a, SV *r)
 INIT:
   adis *d = adis_get(a);
   regs_pad *rp = regpad_get(r);
   int base = -1, res;
   long off = 0;
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
   else {
    res = d->is_ls(rp, base, off);
    if ( !res )
      ST(0) = &PL_sv_undef;
    else { // return array [res base off]
      AV *av = newAV();
      mXPUSHs(newRV_noinc((SV*)av));
      av_push(av, newSViv( res ));
      av_push(av, newSViv( base ));
      av_push(av, newSViv( off ));
    }
   }
   XSRETURN(1);

MODULE = Disasm::Arm64		PACKAGE = Disasm::Arm64::Regpad

void
FETCH(self, key)
  SV *self;
  IV key;
 INIT:
  auto *r = regpad_get(self);
 PPCODE:
  if ( key >= AD_REG_SP || key < 0 )
    ST(0) = &PL_sv_undef;
  else
    ST(0) = sv_2mortal( newSVuv( (unsigned long)r->get(key) ) );
  XSRETURN(1);

void
clone(SV *r)
 INIT:
  HV *pkg = NULL;
  AV *fake = NULL;
  regs_pad *rp = regpad_get(r), *res = NULL;
  SV *objref= NULL;
  MAGIC* magic;
 PPCODE:
  if ( !(pkg = gv_stashpv(s_regpad, 0)) ) {
    croak("Package %s does not exists", s_regpad);
    XSRETURN(0);
  }
  res = new regs_pad(*rp);
  // tie on array
  fake = newAV();
  objref = newRV_noinc((SV*)fake);
  sv_bless(objref, pkg);
  magic = sv_magicext((SV*)fake, NULL, PERL_MAGIC_tied, &regpad_magic_vt, (const char *)res, 0);
  SvREADONLY_on((SV*)fake);
  ST(0) = objref;
  XSRETURN(1);

void
reset(SV *self, IV key)
 INIT:
  auto *r = regpad_get(self);
 PPCODE:
  if ( key >= AD_REG_SP || key < 0 ) {
    XSRETURN_NO;
  } else {
   r->zero(key);
   XSRETURN_YES;
  }

void
abi(SV *self)
 INIT:
  auto *r = regpad_get(self);
 PPCODE:
  if ( !r ) XSRETURN_NO;
  r->mark_abi();
  XSRETURN_YES;

BOOT:
{
  HV *stash= gv_stashpvn("Disasm::Arm64", 13, 1);
  // op types
  EXPORT_ENUM("OP_REG", AD_OP_REG)
  EXPORT_ENUM("OP_IMM", AD_OP_IMM)
  EXPORT_ENUM("OP_SHIFT", AD_OP_SHIFT)
  EXPORT_ENUM("OP_MEM", AD_OP_MEM)
  // shift kinds
  EXPORT_ENUM("SHIFT_LSL", AD_SHIFT_LSL)
  EXPORT_ENUM("SHIFT_LSR", AD_SHIFT_LSR)
  EXPORT_ENUM("SHIFT_ASR", AD_SHIFT_ASR)
  EXPORT_ENUM("SHIFT_ROR", AD_SHIFT_ROR)
  EXPORT_ENUM("SHIFT_MSL", AD_SHIFT_MSL)
  // imm kinds
  EXPORT_ENUM("IMM_INT", AD_IMM_INT)
  EXPORT_ENUM("IMM_UINT", AD_IMM_UINT)
  EXPORT_ENUM("IMM_LONG", AD_IMM_LONG)
  EXPORT_ENUM("IMM_ULONG", AD_IMM_ULONG)
  EXPORT_ENUM("IMM_FLOAT", AD_IMM_FLOAT)
  // cc
  EXPORT_ENUM("CC_EQ", AD_CC_EQ)
  EXPORT_ENUM("CC_NE", AD_CC_NE)
  EXPORT_ENUM("CC_CS", AD_CC_CS)
  EXPORT_ENUM("CC_CC", AD_CC_CC)
  EXPORT_ENUM("CC_MI", AD_CC_MI)
  EXPORT_ENUM("CC_PL", AD_CC_PL)
  EXPORT_ENUM("CC_VS", AD_CC_VS)
  EXPORT_ENUM("CC_VC", AD_CC_VC)
  EXPORT_ENUM("CC_HI", AD_CC_HI)
  EXPORT_ENUM("CC_LS", AD_CC_LS)
  EXPORT_ENUM("CC_GE", AD_CC_GE)
  EXPORT_ENUM("CC_LT", AD_CC_LT)
  EXPORT_ENUM("CC_GT", AD_CC_GT)
  EXPORT_ENUM("CC_LE", AD_CC_LE)
  EXPORT_ENUM("CC_AL", AD_CC_AL)
}
