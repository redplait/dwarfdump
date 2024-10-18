// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "elfio/elfio.hpp"
#include "../elf.inc"
#include "mips.h"

static unsigned char get_host_encoding(void)
{
 static const int tmp = 1;
 if ( 1 == *reinterpret_cast<const char*>( &tmp ) ) return ELFIO::ELFDATA2LSB;
 return ELFIO::ELFDATA2MSB;
}

void my_warn(const char * pat, ...) {
 va_list args;
 vwarn(pat, &args);
}

struct mdis {
  const char *psp, *end;
  unsigned long addr;
  int m_needswap;
  mips::MipsVersion m_mv;
  mips::Instruction inst;
  char txt[1024];
  IElf *e;
  mdis(IElf *_e, mips::MipsVersion v, int b): e(_e), m_mv(v), m_needswap(b)
  { addr = 0;
    psp = end = nullptr;
    memset(&inst, 0, sizeof(inst));
  }
  ~mdis() {
    if ( e ) e->release();
  }
  bool inline empty() const {
    return !psp || !inst.size;
  }
  bool inline setuped() const {
    return psp;
  }
 int is_end() const
 {
   switch(inst.operation)
   {
     case mips::MIPS_BREAK:
     case mips::MIPS_B:
     case mips::MIPS_JR:
      return 1;
   }
   return 0;
 }
 int setup(unsigned long addr_)
 {
   psp = end = nullptr;
   addr = addr_;
   auto *s = find_section(e, addr_);
   if ( !s ) {
     my_warn("cannot find section for address %lX", addr_);
     return 0;
   }
   size_t diff = addr - s->get_address();
   psp = s->get_data() + diff;
   end = s->get_data() + s->get_size();
   return 1;
 }
 int disasm()
 {
   if ( psp >= end ) return 0;
   memset(&inst, 0, sizeof(inst));
   int rc = mips::mips_decompose((const uint32_t*)psp, end - psp, &inst, m_mv, (uint64_t)psp, m_needswap, 1);
   if ( rc ) return 0;
   // update addr & psp
   psp += inst.size;
   addr += inst.size;
   // check for speculative execution
   if ( is_end() )
     end = psp + 4;
   return inst.size;;
 }
};

static int mips_magic_free(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        mdis *m = (mdis *)mg->mg_ptr;
        delete m;
        mg->mg_ptr= NULL;
    }
    return 0; // ignored anyway
}

// magic table for Disasm::Mips
static MGVTBL Mips_magic_vt = {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        mips_magic_free,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

mdis *mdis_get(SV *obj)
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
        if (magic->mg_type == PERL_MAGIC_ext && magic->mg_virtual == &Mips_magic_vt)
          /* If found, the mg_ptr points to the fields structure. */
            return (mdis*) magic->mg_ptr;
    }
  return NULL;
}

#define EXPORT_ENUM(x) newCONSTSUB(stash, #x, new_enum_dualvar(aTHX_ mips::x, newSVpvs_share(#x)));
static SV * new_enum_dualvar(pTHX_ IV ival, SV *name) {
        SvUPGRADE(name, SVt_PVNV);
        SvIV_set(name, ival);
        SvIOK_on(name);
        SvREADONLY_on(name);
        return name;
}

MODULE = Disasm::Mips		PACKAGE = Disasm::Mips

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
  mips::MipsVersion ver;
  mdis *res = nullptr;
 PPCODE:
  // check what we have
  machine = e->rdr->get_machine();
  if ( machine != ELFIO::EM_MIPS ) {
    my_warn("new: not MIPS elf");
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  ver = e->rdr->get_class() == ELFIO::ELFCLASS32 ? mips::MIPS_32 : mips::MIPS_64;
  if (SvPOK(obj_or_pkg) && (pkg= gv_stashsv(obj_or_pkg, 0))) {
    if (!sv_derived_from(obj_or_pkg, "Disasm::Mips"))
        croak("Package %s does not derive from Disasm::Mips", SvPV_nolen(obj_or_pkg));
    msv = newSViv(0);
    objref= sv_2mortal(newRV_noinc(msv));
    sv_bless(objref, pkg);
    ST(0)= objref;
  } else
    croak("new: first arg must be package name or blessed object");
  // make real disasm object
  res = new mdis( e, ver, e->rdr->get_encoding() != get_host_encoding() );
  // attach magic
  magic = sv_magicext(msv, NULL, PERL_MAGIC_ext, &Mips_magic_vt, (const char*)res, 0);
#ifdef USE_ITHREADS
    magic->mg_flags |= MGf_DUP;
#endif
  XSRETURN(1);

void
setup(SV *sv, unsigned long addr)
 INIT:
   mdis *d = mdis_get(sv);
 PPCODE:
   ST(0) = sv_2mortal( newSVuv( d->setup(addr) ) );
   XSRETURN(1);

void
disasm(SV *sv)
 INIT:
   mdis *d = mdis_get(sv);
 PPCODE:
   if ( !d->setuped() ) {
     my_warn("disasm on non-setuped Mips::Disasm called");
     ST(0) = &PL_sv_undef;
   } else
    ST(0) = sv_2mortal( newSViv( d->disasm() ) );
   XSRETURN(1);

void
op_name(int v)
 INIT:
  const char *name = mips::get_operation(mips::Operation(v));
 PPCODE:
  if ( !name )
    ST(0) = &PL_sv_undef;
  else
    ST(0) = sv_2mortal( newSVpv(name, strlen(name)) );
  XSRETURN(1);

void
reg_name(int v)
 INIT:
  const char *name = mips::get_register(mips::Reg(v));
 PPCODE:
  if ( !name )
    ST(0) = &PL_sv_undef;
  else
    ST(0) = sv_2mortal( newSVpv(name, strlen(name)) );
  XSRETURN(1);

void
text(SV *sv)
 INIT:
   mdis *d = mdis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else {
    d->txt[0] = 0;
    mips::mips_disassemble(&d->inst, d->txt, 1023);
    ST(0) = sv_2mortal( newSVpv(d->txt, strlen(d->txt)) );
  }
  XSRETURN(1);

void
addr(SV *sv)
 INIT:
   mdis *d = mdis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else
    ST(0) = sv_2mortal( newSViv( d->addr - d->inst.size ) );
  XSRETURN(1);

void
op(SV *sv)
 INIT:
   mdis *d = mdis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else
    ST(0) = sv_2mortal( newSViv( d->inst.operation ) );
  XSRETURN(1);

void
size(SV *sv)
 INIT:
   mdis *d = mdis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else
    ST(0) = sv_2mortal( newSViv( d->inst.size ) );
  XSRETURN(1);


void
op_class(SV *sv, int idx)
 INIT:
   mdis *d = mdis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else if ( idx < 0 || idx >= MAX_OPERANDS )
    croak("invalid index of operand %d", idx);
  else
    ST(0) = sv_2mortal( newSViv( d->inst.operands[idx].operandClass ) );
  XSRETURN(1);

void
op_reg(SV *sv, int idx)
 INIT:
   mdis *d = mdis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else if ( idx < 0 || idx >= MAX_OPERANDS )
    croak("invalid index of operand %d", idx);
  else
    ST(0) = sv_2mortal( newSViv( d->inst.operands[idx].reg ) );
  XSRETURN(1);

void
op_imm(SV *sv, int idx)
 INIT:
   mdis *d = mdis_get(sv);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
  else if ( idx < 0 || idx >= MAX_OPERANDS )
    croak("invalid index of operand %d", idx);
  else
    ST(0) = sv_2mortal( newSVuv( d->inst.operands[idx].immediate ) );
  XSRETURN(1);

BOOT:
 HV *stash= gv_stashpvn("Disasm::Mips", 12, 1);
 EXPORT_ENUM(NONE)
 EXPORT_ENUM(REG)
 EXPORT_ENUM(FLAG)
 EXPORT_ENUM(IMM)
 EXPORT_ENUM(LABEL)
 EXPORT_ENUM(MEM_IMM)
 EXPORT_ENUM(MEM_REG)
 EXPORT_ENUM(HINT)
 // registers
 EXPORT_ENUM(REG_ZERO)
 EXPORT_ENUM(REG_AT)
 EXPORT_ENUM(REG_V0)
 EXPORT_ENUM(REG_V1)
 EXPORT_ENUM(REG_A0)
 EXPORT_ENUM(REG_A1)
 EXPORT_ENUM(REG_A2)
 EXPORT_ENUM(REG_A3)
 EXPORT_ENUM(REG_T0)
 EXPORT_ENUM(REG_A4)
 EXPORT_ENUM(REG_T1)
 EXPORT_ENUM(REG_A5)
 EXPORT_ENUM(REG_T2)
 EXPORT_ENUM(REG_A6)
 EXPORT_ENUM(REG_T3)
 EXPORT_ENUM(REG_A7)
 EXPORT_ENUM(REG_T4)
 EXPORT_ENUM(REG_T5)
 EXPORT_ENUM(REG_T6)
 EXPORT_ENUM(REG_T7)
 EXPORT_ENUM(REG_S0)
 EXPORT_ENUM(REG_S1)
 EXPORT_ENUM(REG_S2)
 EXPORT_ENUM(REG_S3)
 EXPORT_ENUM(REG_S4)
 EXPORT_ENUM(REG_S5)
 EXPORT_ENUM(REG_S6)
 EXPORT_ENUM(REG_S7)
 EXPORT_ENUM(REG_T8)
 EXPORT_ENUM(REG_T9)
 EXPORT_ENUM(REG_K0)
 EXPORT_ENUM(REG_K1)
 EXPORT_ENUM(REG_GP)
 EXPORT_ENUM(REG_SP)
 EXPORT_ENUM(REG_FP)
 EXPORT_ENUM(REG_RA)
