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
 vwarn(pat, &args);
}

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
         (instr_id == AD_INSTR_B && cc == AD_NONE)
       )
    {
      end = psp;
    }
    return 1;
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
  inline int is_ldr_off() const
  {
     return ( (instr_id == AD_INSTR_LDRAA || instr_id == AD_INSTR_LDR)
               && num_operands == 3 && operands[0].type == AD_OP_REG && operands[1].type == AD_OP_REG
             );
  }
  inline int is_ldraa() const
  {
      return ( (instr_id == AD_INSTR_LDRAA || instr_id == AD_INSTR_LDR)
               && num_operands == 2 && operands[0].type == AD_OP_REG && operands[1].type == AD_OP_REG
             );
  }
  int s_adr() const
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
   int is_ldr0() const
   {
     return (instr_id == AD_INSTR_LDR) && 
            (num_operands == 2) &&
            (operands[0].type == AD_OP_REG) &&
            (operands[1].type == AD_OP_REG)
     ;
   }
   int is_ldr() const
   {
     return (instr_id == AD_INSTR_LDR) && 
            (num_operands == 3) &&
            (operands[0].type == AD_OP_REG) &&
            (operands[1].type == AD_OP_REG) &&
            (operands[2].type == AD_OP_IMM)
     ;
   }
};

static int arm64_magic_free(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        auto *m = (adis *)mg->mg_ptr;
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
        arm64_magic_free,
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
     ST(0) = sv_2mortal( newSVuv( d->addr ) );
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
  } else if ( d->operands[idx].type != AD_OP_REG )
    ST(0) = &PL_sv_undef;
  else
   sv_2mortal( newSViv(d->operands[idx].op_reg.rn) );
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
