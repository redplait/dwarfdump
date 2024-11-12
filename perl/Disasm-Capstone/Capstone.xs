// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "elfio/elfio.hpp"
#include "../elf.inc"
#include "capstone/capstone.h"
#include <bitset>

void my_warn(const char * pat, ...) {
 va_list args;
 vwarn(pat, &args);
}

static HV *s_ppc_pkg = nullptr,
 *s_ppc_regpad_pkg = nullptr;

// base disasm class
static const int s_sign = 0x63617073;

struct CaBase {
  int sign = s_sign;
  const char *psp, *end;
  unsigned long addr;
  IElf *e;
  bool succ = false;
  cs_insn *insn = nullptr;
  csh handle = 0;
  // methods
  CaBase(IElf *_e): e(_e) {
    psp = end = nullptr;
    addr = 0;
  }
  int alloc_insn() {
    if ( insn ) return 1;
    insn = cs_malloc(handle);
    return insn != nullptr;
  }
  virtual ~CaBase() {
    e->release();
    if ( insn ) cs_free(insn, 1);
    if ( handle ) cs_close(&handle);
  }
  inline int empty() const {
    return (insn == nullptr) || (psp == nullptr) || !succ;
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
 // like setup but also with explicit len, anyway need to check if this len can fit into some section
 int setup(unsigned long addr_, unsigned long len_)
 {
   psp = end = nullptr;
   addr = addr_;
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
   return 1;
 }
};

// ppc regpad
struct ppc_regs {
  constexpr static int base = PPC_REG_R9;
  constexpr static int size = 32;
  int64_t regs[size];
  std::bitset<size> pres;
  // for moves
  char m[size];
  ppc_regs() {
    memset(m, -1, size);
  }
  ppc_regs(ppc_regs &) = default;
  int64_t get(int idx) const
  {
    if ( idx < base ) return 0;
    idx -= base;
    if ( idx >= size ) return 0;
    if ( !pres[idx] ) return 0;
    return regs[idx];
  }
  int64_t set(int idx, int64_t v)
  {
    if ( idx < base ) return 0;
    idx -= base;
    if ( idx >= size ) return 0;
    pres[idx] = 1; m[idx] = -1;
    regs[idx] = v;
    return v;
  }
  int move(int idx, int src)
  {
    if ( idx < base || src < base ) return 0;
    idx -= base; src -= base;
    if ( idx >= size || src >= size ) return 0;
    regs[idx] = regs[src];
    pres[idx] = pres[src];
    m[idx] = m[src];
    return 1;
  }
  // see details https://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi.html
  void abi()
  {
    char v = 1;
    for ( int i = PPC_REG_R4 - base; i <= PPC_REG_R10 - base; i++, v++ )
      m[i] = v;
  }
  int arg(int idx) const {
    if ( idx < base ) return -1;
    idx -= base;
    if ( idx >= size ) return -1;
    return m[idx];
  }
  // addis reg1 = reg2 + int << 0x10
  int addis(int idx, int src, int v) {
    if ( idx < base || src < base ) return 0;
    idx -= base; src -= base;
    if ( !pres[src] ) return 0;
    pres[idx] = 1;
    if ( v < 0 )
     regs[idx] = regs[src] - (-v) << 0x10;
    else
     regs[idx] = regs[src] + v << 0x10;
    return 1;
  }
  // addi reg1 = reg2 + int
  int addi(int idx, int src, int v) {
    if ( idx < base || src < base ) return 0;
    idx -= base; src -= base;
    if ( !pres[src] ) return 0;
    pres[idx] = 1;
    regs[idx] = regs[src] + v;
    return 1;
  }
};

// ppc disasm
struct ppc_disasm: public CaBase
{
  ppc_disasm(IElf *_e): CaBase(_e)
  { }
  int disasm() {
    if ( !psp || psp >= end ) return 0;
    size_t size = end - psp;
    auto err = cs_disasm_iter(handle, (const unsigned char **)&psp, &size, &addr, insn);
    if ( !err ) { succ = false; return 0; }
    succ = true;
    // check for end instructions
    if ( insn->id == PPC_INS_B || insn->id == PPC_INS_BLR || insn->id == PPC_INS_BCTR ||
      (insn->id == PPC_INS_BCLR && insn->alias_id == PPC_INS_ALIAS_BLR) )
      end = psp;
    return 1;
  }
  // getters
  inline int is_reg(int idx) const
  {
    if ( idx >= insn->detail->ppc.op_count ) return 0;
    return insn->detail->ppc.operands[idx].type == PPC_OP_REG;
  }
  inline int is_imm(int idx) const
  {
    if ( idx >= insn->detail->ppc.op_count ) return 0;
    return insn->detail->ppc.operands[idx].type == PPC_OP_IMM;
  }
  inline int is_mem(int idx) const
  {
    if ( idx >= insn->detail->ppc.op_count ) return 0;
    return insn->detail->ppc.operands[idx].type == PPC_OP_MEM;
  }
  inline int get_reg(int idx) const
  {
    if ( idx >= insn->detail->ppc.op_count || insn->detail->ppc.operands[idx].type != PPC_OP_REG ) return 0;
    return insn->detail->ppc.operands[idx].reg;
  }
  int is_xxx(int num, int t0, int t1 = 0, int t2 = 0) const {
    if ( insn->detail->ppc.op_count < num ) return 0;
    if ( t0 != insn->detail->ppc.operands[0].type ) return 0;
    if ( t1 && t1 != insn->detail->ppc.operands[1].type ) return 0;
    if ( t2 && t2 != insn->detail->ppc.operands[2].type ) return 0;
    return 1;
  }
};

// magic
template <typename T>
static int cap_magic_free(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        auto *m = (T *)mg->mg_ptr;
        delete m;
        mg->mg_ptr= NULL;
    }
    return 0; // ignored anyway
}

// magic table for Disasm::Capstone::PPC
static const char *s_ppc_name = "Disasm::Capstone::PPC";
static MGVTBL ppc_magic_vt = {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        cap_magic_free<ppc_disasm>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

// magic table for Disasm::Capstone::PPC::Regpad
static const char *s_ppc_regpad_name = "Disasm::Capstone::PPC::Regpad";
static MGVTBL ppc_regpad_magic_vt = {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        cap_magic_free<ppc_regs>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

template <typename T>
T *get_disasm(SV *obj, MGVTBL *tab)
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
        if (magic->mg_type == PERL_MAGIC_ext && magic->mg_virtual == tab)
          /* If found, the mg_ptr points to the fields structure. */
            return (T*) magic->mg_ptr;
    }
  return NULL;
}


CaBase *cabase_get(SV *obj)
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
        if (magic->mg_type == PERL_MAGIC_ext && magic->mg_ptr ) {
          CaBase *cb = (CaBase *)magic->mg_ptr;
          if ( cb->sign == s_sign ) return cb;
        }
    }
  return NULL;
}

#define EXPORT_ENUM(x) newCONSTSUB(stash, #x, new_enum_dualvar(aTHX_ x, newSVpvs_share(#x)));
static SV * new_enum_dualvar(pTHX_ IV ival, SV *name) {
        SvUPGRADE(name, SVt_PVNV);
        SvIV_set(name, ival);
        SvIOK_on(name);
        SvREADONLY_on(name);
        return name;
}

MODULE = Disasm::Capstone		PACKAGE = Disasm::Capstone

void
op(SV *self)
 INIT:
   auto *d = cabase_get(self);
 PPCODE:
   if ( !d || d->empty() )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVuv(d->insn->id) );
   XSRETURN(1);

void
alias(SV *self)
 INIT:
   auto *d = cabase_get(self);
 PPCODE:
   if ( !d || d->empty() )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVuv(d->insn->alias_id) );
   XSRETURN(1);

void
addr(SV *self)
 INIT:
   auto *d = cabase_get(self);
 PPCODE:
   if ( !d || d->empty() )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVuv(d->insn->address) );
   XSRETURN(1);

void
size(SV *self)
 INIT:
   auto *d = cabase_get(self);
 PPCODE:
   if ( !d || d->empty() )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSViv(d->insn->size) );
   XSRETURN(1);

void
mnem(SV *self)
 INIT:
   auto *d = cabase_get(self);
 PPCODE:
   if ( !d || d->empty() )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVpv(d->insn->mnemonic, strlen(d->insn->mnemonic)) );
   XSRETURN(1);

void
text(SV *self)
 INIT:
   auto *d = cabase_get(self);
 PPCODE:
   if ( !d || d->empty() )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVpv(d->insn->op_str, strlen(d->insn->op_str)) );
   XSRETURN(1);

void
setup(SV *sv, unsigned long addr)
 INIT:
   auto *d = cabase_get(sv);
 PPCODE:
   ST(0) = sv_2mortal( newSVuv( d->setup(addr) ) );
   XSRETURN(1);

void
setup2(SV *sv, unsigned long addr, unsigned long len)
 INIT:
   auto *d = cabase_get(sv);
 PPCODE:
   ST(0) = sv_2mortal( newSVuv( d->setup(addr, len) ) );
   XSRETURN(1);


MODULE = Disasm::Capstone		PACKAGE = Disasm::Capstone::PPC

void
new(obj_or_pkg, SV *elsv)
  SV *obj_or_pkg
 INIT:
  HV *pkg = NULL;
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  struct IElf *e= extract(elsv);
  CaBase *res = nullptr;
  int mod = 0;
 PPCODE:
  // check what we have
  ELFIO::Elf_Half machine = e->rdr->get_machine();
  if ( machine == ELFIO::EM_PPC ) mod = CS_MODE_32;
  else if ( machine == ELFIO::EM_PPC64 ) mod = CS_MODE_64;
  else {
    my_warn("new: not PPC elf");
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  if (SvPOK(obj_or_pkg) && (pkg= gv_stashsv(obj_or_pkg, 0))) {
    if (!sv_derived_from(obj_or_pkg, "Disasm::Capstone"))
        croak("Package %s does not derive from Disasm::Capstone", SvPV_nolen(obj_or_pkg));
  } else
    croak("new: first arg must be package name or blessed object");
  // make real disasm object
  e->add_ref();
  res = new CaBase( e );
  if ( e->rdr->get_encoding() == ELFIO::ELFDATA2MSB ) mod |= CS_MODE_BIG_ENDIAN;
  // try to open
  cs_err err = cs_open(CS_ARCH_PPC, (cs_mode)mod, &res->handle);
  if ( err ) {
    my_warn("new: cs_open failed, err %d", err);
    delete res;
    ST(0) = sv_2mortal( newSViv(err) );
    XSRETURN(1);
  }
  err = cs_option(res->handle, CS_OPT_DETAIL, CS_OPT_ON);
  if ( err ) {
    my_warn("new: cs_option failed, err %d", err);
    delete res;
    ST(0) = sv_2mortal( newSViv(err) );
    XSRETURN(1);
  }
  res->alloc_insn();
  // make blessed obj
  msv = newSViv(0);
  objref= sv_2mortal(newRV_noinc(msv));
  sv_bless(objref, s_ppc_pkg);
  ST(0)= objref;
// attach magic
  magic = sv_magicext(msv, NULL, PERL_MAGIC_ext, &ppc_magic_vt, (const char*)res, 0);
#ifdef USE_ITHREADS
  magic->mg_flags |= MGf_DUP;
#endif
  XSRETURN(1);

void
op_cnt(SV *self)
 INIT:
   auto *d = cabase_get(self);
PPCODE:
   if ( !d || d->empty() || !d->insn->detail )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSViv(d->insn->detail->ppc.op_count ) );
   XSRETURN(1);

void
op_type(SV *self, IV idx)
 INIT:
   auto *d = cabase_get(self);
PPCODE:
   if ( !d || d->empty() || !d->insn->detail || idx >= d->insn->detail->ppc.op_count )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSViv(d->insn->detail->ppc.operands[idx].type) );
   XSRETURN(1);

void
op_reg(SV *self, IV idx)
 INIT:
   auto *d = cabase_get(self);
PPCODE:
   if ( !d || d->empty() || !d->insn->detail || idx >= d->insn->detail->ppc.op_count )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSViv(d->insn->detail->ppc.operands[idx].reg) );
   XSRETURN(1);

void
op_imm(SV *self, IV idx)
 INIT:
   auto *d = cabase_get(self);
PPCODE:
   if ( !d || d->empty() || !d->insn->detail || idx >= d->insn->detail->ppc.op_count )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVuv(d->insn->detail->ppc.operands[idx].imm) );
   XSRETURN(1);

void
op_mem(SV *self, IV idx)
 INIT:
   auto *d = cabase_get(self);
   AV *av;
PPCODE:
   if ( !d || d->empty() || !d->insn->detail || idx >= d->insn->detail->ppc.op_count )
    ST(0) = &PL_sv_undef;
   else {
     av = newAV();
     // return ref to [ base disp offset]
     av_push(av, newSViv( d->insn->detail->ppc.operands[idx].mem.base ));
     av_push(av, newSViv( d->insn->detail->ppc.operands[idx].mem.disp ));
     av_push(av, newSViv( d->insn->detail->ppc.operands[idx].mem.offset ));
     ST(0) = newRV_noinc((SV*)av);
   }
   XSRETURN(1);

void
op_access(SV *self, IV idx)
 INIT:
   auto *d = cabase_get(self);
PPCODE:
   if ( !d || d->empty() || !d->insn->detail || idx >= d->insn->detail->ppc.op_count )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSViv(d->insn->detail->ppc.operands[idx].access) );
   XSRETURN(1);

void
disasm(SV *self)
 INIT:
   auto *d = get_disasm<ppc_disasm>(self, &ppc_magic_vt);
 PPCODE:
   if ( !d ) ST(0) = &PL_sv_undef;
   else {
     if ( d->disasm() )
       ST(0) = &PL_sv_yes;
     else
       ST(0) = &PL_sv_no;
  }
  XSRETURN(1);

void
regpad(SV *self)
 ALIAS:
  Disasm::Capstone::PPC::abi = 1
 INIT:
   auto *d = get_disasm<ppc_disasm>(self, &ppc_magic_vt);
   SV *msv;
   SV *objref= NULL;
   MAGIC* magic;
 PPCODE:
   if ( !d || !d->addr ) ST(0) = &PL_sv_undef;
   else {
     ppc_regs *res = new ppc_regs();
     if ( ix == 1 ) {
       // for abi set r2 to addr + args
       res->set(PPC_REG_R2, d->addr);
       res->abi();
     }
     // make blessed obj
     msv = newSViv(0);
     objref= sv_2mortal(newRV_noinc(msv));
     sv_bless(objref, s_ppc_regpad_pkg);
     ST(0)= objref;
     // attach magic
     magic = sv_magicext(msv, NULL, PERL_MAGIC_ext, &ppc_regpad_magic_vt, (const char*)res, 0);
#ifdef USE_ITHREADS
     magic->mg_flags |= MGf_DUP;
#endif
     XSRETURN(1);
   }

MODULE = Disasm::Capstone		PACKAGE = Disasm::Capstone::PPC::Regpad

void
clone(SV *self)
 INIT:
   auto *d = get_disasm<ppc_regs>(self, &ppc_regpad_magic_vt);
   SV *msv;
   SV *objref= NULL;
   MAGIC* magic;
 PPCODE:
   if ( !d ) ST(0) = &PL_sv_undef;
   else {
     ppc_regs *res = new ppc_regs(*d);
     // make blessed obj
     msv = newSViv(0);
     objref= sv_2mortal(newRV_noinc(msv));
     sv_bless(objref, s_ppc_regpad_pkg);
     ST(0)= objref;
     // attach magic
     magic = sv_magicext(msv, NULL, PERL_MAGIC_ext, &ppc_regpad_magic_vt, (const char*)res, 0);
#ifdef USE_ITHREADS
     magic->mg_flags |= MGf_DUP;
#endif
   }
   XSRETURN(1);

void
get(SV *self, int idx)
 INIT:
   auto *d = get_disasm<ppc_regs>(self, &ppc_regpad_magic_vt);
 PPCODE:
   UV v = d->get(idx);
   ST(0) = sv_2mortal( newSVuv(v) );
   XSRETURN(1);

void
arg(SV *self, int idx)
 INIT:
   auto *d = get_disasm<ppc_regs>(self, &ppc_regpad_magic_vt);
 PPCODE:
   int res = d->arg(idx);
   if ( res == -1 ) ST(0) = &PL_sv_undef;
   else sv_2mortal( newSViv(res) );
   XSRETURN(1);

BOOT:
 s_ppc_pkg = gv_stashpv(s_ppc_name, 0);
 if ( !s_ppc_pkg )
    croak("Package %s does not exists", s_ppc_name);
 s_ppc_regpad_pkg = gv_stashpv(s_ppc_regpad_name, 0);
 if ( !s_ppc_regpad_pkg )
    croak("Package %s does not exists", s_ppc_regpad_name);
 cs_arch_register_powerpc();
 // export some enums
 HV *stash= gv_stashpvn("Disasm::Capstone", 16, 1);
 EXPORT_ENUM(CS_OP_INVALID)
 EXPORT_ENUM(CS_OP_REG)
 EXPORT_ENUM(CS_OP_IMM)
 EXPORT_ENUM(CS_OP_FP)
 EXPORT_ENUM(CS_OP_PRED)
 EXPORT_ENUM(CS_OP_SPECIAL)
 EXPORT_ENUM(CS_OP_BOUND)
 EXPORT_ENUM(CS_OP_MEM)
 EXPORT_ENUM(CS_OP_MEM_REG)
 EXPORT_ENUM(CS_OP_MEM_IMM)
 EXPORT_ENUM(CS_AC_READ)
 EXPORT_ENUM(CS_AC_WRITE)
 EXPORT_ENUM(CS_AC_READ_WRITE)
