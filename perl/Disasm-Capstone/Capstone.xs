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
 *s_riscv_pkg = nullptr,
 *s_ppc_regpad_pkg = nullptr;

// base disasm class
static const int s_sign = 0x63617073;

struct CaBase {
  int sign = s_sign;
  const char *psp, *end;
  unsigned long addr = 0;
  IElf *e;
  bool succ = false;
  cs_insn *insn = nullptr;
  csh handle = 0;
  // methods
  CaBase(IElf *_e): e(_e) {
    psp = end = nullptr;
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
  constexpr static int base = PPC_REG_R0;
  constexpr static int xbase = PPC_REG_X0;
  constexpr static int size = 32;
  uint64_t regs[size];
  std::bitset<size> pres;
  // for moves
  char m[size];
  ppc_regs() {
    memset(m, -1, size);
  }
  ppc_regs(ppc_regs &) = default;
  uint64_t get(int idx) const
  {
    if ( idx < base ) return 0;
    idx -= base;
    if ( idx >= size ) return 0;
    if ( !pres[idx] ) return 0;
    return regs[idx];
  }
  uint64_t set(int idx, uint64_t v)
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
  void clean(int idx) {
    if ( idx >= xbase )
      idx -= xbase;
    else {
      if ( idx < base ) return;
      idx -= base;
    }
    if ( idx >= size ) return;
    pres[idx] = 0; regs[idx] = 0;
    m[idx] = -1;
  }
  // see details https://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi.html
  void abi()
  {
    char v = 1;
    // r3 - 1, r4 - 2, ... up tp r10
    for ( int i = PPC_REG_R3 - base; i <= PPC_REG_R10 - base; i++, v++ )
      m[i] = v;
  }
  int arg(int idx) const {
    if ( idx < base ) return -1;
    idx -= base;
    if ( idx >= size ) return -1;
    return m[idx];
  }
  inline int has(int idx) const
  {
    if ( idx < base ) return 0;
    idx -= base;
    if ( idx >= size ) return 0;
    return pres[idx];
  }
  uint64_t add(int idx, int v) {
    if ( idx < base ) return 0;
    idx -= base;
    if ( idx >= size ) return 0;
    if ( !pres[idx] ) return 0;
    return regs[idx] + v;
  }
  // addis reg1 = reg2 + int << 0x10
  int addis(int idx, int src, int v) {
    if ( idx < base || src < base ) return 0;
    idx -= base; src -= base;
    if ( idx >= size || src >= size ) return 0;
    if ( !pres[src] ) return 0;
    pres[idx] = 1;
    if ( v < 0 )
     regs[idx] = regs[src] - (-v << 0x10);
    else
     regs[idx] = regs[src] + (v << 0x10);
// printf("ADDIS %d %lX %d %lX %lX\n", idx, regs[idx], src, regs[src], v << 0x10);
    m[idx] = -1;
    return 1;
  }
  // addi reg1 = reg2 + int
  int addi(int idx, int src, int v) {
    if ( idx < base || src < base ) return 0;
    idx -= base; src -= base;
    if ( idx >= size || src >= size ) return 0;
    if ( !pres[src] ) return 0;
// printf("ADDI %d %d %lX\n", idx, src, regs[src]);
    pres[idx] = 1;
    regs[idx] = regs[src] + v;
    m[idx] = -1;
    return 1;
  }
  // li reg, int
  int li(int idx, int v) {
    if ( idx < base ) return 0;
    idx -= base;
    if ( idx >= size ) return 0;
    pres[idx] = 1;
    regs[idx] = v;
    m[idx] = -1;
    return 1;
  }
  // lis reg, v << 0x10
  uint64_t lis(int idx, int v) {
    if ( idx < base ) return 0;
    idx -= base;
    if ( idx >= size ) return 0;
    pres[idx] = 1;
    if ( v < 0 )
     regs[idx] = (-v) << 0x10;
    else
     regs[idx] = v << 0x10;
    m[idx] = -1;
    return regs[idx];
  }
  // ori reg, reg, v
  uint64_t ori(int idx, int src, int64_t v)
  {
    if ( idx < base || src < base ) return 0;
    idx -= base; src -= base;
    if ( idx >= size || src >= size ) return 0;
    if ( !pres[src] ) return 0;
// printf("ORI %d %d %lX\n", idx, src, regs[src]);
    pres[idx] = 1;
    regs[idx] = regs[src] | (unsigned short)v;
    m[idx] = -1;
    return regs[idx];
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
  // 1 - load, 2 - store
  // see https://wiki.raptorcs.com/w/images/f/f5/PowerISA_public.v3.1.pdf
  int is_ls() const {
    switch(insn->id) {
      case PPC_INS_LBZ:
      case PPC_INS_LHA:
      case PPC_INS_LHZ:
      case PPC_INS_LWA:
      case PPC_INS_LWZ:
      case PPC_INS_LD:
      case PPC_INS_LQ:
       if ( is_xxx(2, PPC_OP_REG, PPC_OP_MEM) ) return 1;
       return 0;
      case PPC_INS_STB:
      case PPC_INS_STH:
      case PPC_INS_STW:
      case PPC_INS_STD:
      case PPC_INS_STQ:
       if ( is_xxx(2, PPC_OP_REG, PPC_OP_MEM) ) return 2;
       return 0;
      default: return 0;
    }
  }
  uint64_t apply(ppc_regs *r) {
    // li/lis/ori - for constants, must be before addi/addis bcs those are aliases
    if ( insn->alias_id == PPC_INS_ALIAS_LI && is_xxx(2, PPC_OP_REG, PPC_OP_IMM) ) {
      auto v = insn->detail->ppc.operands[1].imm;
      r->li(get_reg(0), v);
      return v;
    }
    if ( insn->alias_id == PPC_INS_ALIAS_LIS && is_xxx(2, PPC_OP_REG, PPC_OP_IMM) ) {
      r->lis(get_reg(0), insn->detail->ppc.operands[1].imm);
      return 0;
    }
    if ( insn->id == PPC_INS_ORI && is_xxx(3, PPC_OP_REG, PPC_OP_REG, PPC_OP_IMM) ) {
      return r->ori(get_reg(0), get_reg(1), (int)insn->detail->ppc.operands[2].imm);
    }
    // addis
    if ( insn->id == PPC_INS_ADDIS && is_xxx(3, PPC_OP_REG, PPC_OP_REG, PPC_OP_IMM) )
    {
      r->addis(get_reg(0), get_reg(1), (int)insn->detail->ppc.operands[2].imm);
      return 0;
    }
    if ( insn->id == PPC_INS_ADDI && is_xxx(3, PPC_OP_REG, PPC_OP_REG, PPC_OP_IMM) )
    {
      if ( r->addi(get_reg(0), get_reg(1), (int)insn->detail->ppc.operands[2].imm) )
        return r->get(get_reg(0));
      return 0;
    }
    // mr
    if ( (insn->alias_id == PPC_INS_ALIAS_MR || insn->alias_id == PPC_INS_ALIAS_MR_) && is_xxx(2, PPC_OP_REG, PPC_OP_REG) )
    {
      r->move(get_reg(0), get_reg(1));
      return 0;
    }
    // load/store
    uint64_t res = 0;
    int ls = is_ls();
    if ( ls )
      res = r->add(insn->detail->ppc.operands[1].mem.base, insn->detail->ppc.operands[1].mem.disp);
    // must be final
    if ( is_reg(0) && insn->detail->ppc.operands[0].access & CS_AC_WRITE ) {
      r->clean(get_reg(0));
// printf("CLEAR %d %lX\n", get_reg(0), r->get(get_reg(0)));
    }
    return res;
  }
};

// risc-v disasm
struct riscv_disasm: public CaBase
{
  riscv_disasm(IElf *_e): CaBase(_e)
  { }
  int disasm() {
    if ( !psp || psp >= end ) return 0;
    size_t size = end - psp;
// __asm__ volatile("int $0x03");
    bool berr = cs_disasm_iter(handle, (const unsigned char **)&psp, &size, &addr, insn);
    if ( !berr ) {
//      auto err_ = cs_errno(handle);
// printf("psp %p size %d err %d insn %d\n", psp, size, err_, insn->id);
      succ = false;
      return 0;
    }
    succ = true;
    // check for end instructions
    if ( insn->id == RISCV_INS_SRET || insn->id == RISCV_INS_MRET || insn->id == RISCV_INS_URET || is_ret() ||
         insn->id == RISCV_INS_C_JR || insn->id == RISCV_INS_C_J || // jmp reg/jmp imm
         insn->id == RISCV_INS_C_EBREAK || insn->id == RISCV_INS_EBREAK
       )
      end = psp;
    return 1;
  }
  inline int is_ret() const
  {
    return (insn->id == RISCV_INS_C_JR) && (get_reg(0) == RISCV_REG_RA);
  }
  // getters
  inline int is_reg(int idx) const
  {
    if ( idx >= insn->detail->riscv.op_count ) return 0;
    return insn->detail->riscv.operands[idx].type == RISCV_OP_REG;
  }
  inline int is_imm(int idx) const
  {
    if ( idx >= insn->detail->riscv.op_count ) return 0;
    return insn->detail->riscv.operands[idx].type == RISCV_OP_IMM;
  }
  inline int is_mem(int idx) const
  {
    if ( idx >= insn->detail->riscv.op_count ) return 0;
    return insn->detail->riscv.operands[idx].type == RISCV_OP_MEM;
  }
  inline int get_reg(int idx) const
  {
    if ( idx >= insn->detail->riscv.op_count || insn->detail->riscv.operands[idx].type != RISCV_OP_REG ) return 0;
    return insn->detail->riscv.operands[idx].reg;
  }
  int is_xxx(int num, int t0, int t1 = 0, int t2 = 0) const {
    if ( insn->detail->riscv.op_count < num ) return 0;
    if ( t0 != insn->detail->riscv.operands[0].type ) return 0;
    if ( t1 && t1 != insn->detail->riscv.operands[1].type ) return 0;
    if ( t2 && t2 != insn->detail->riscv.operands[2].type ) return 0;
    return 1;
  }
  bool is_jxx(unsigned long &addr) const {
    switch(insn->id) {
      case RISCV_INS_BEQ:
      case RISCV_INS_BGE:
      case RISCV_INS_BGEU:
      case RISCV_INS_BLT:
      case RISCV_INS_BLTU:
      case RISCV_INS_BNE:
        if ( is_imm(1) ) {
          addr = insn->address + insn->detail->riscv.operands[1].imm;
          return true;
        }
        if ( is_imm(2) ) {
          addr = insn->address + insn->detail->riscv.operands[2].imm;
          return true;
        }
        return false;

      case RISCV_INS_C_J:
        if ( is_imm(0) ) {
          addr = insn->address + insn->detail->riscv.operands[0].imm;
          return true;
        }
        return false;
      default: return false;
    }
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

// magic table for Disasm::Capstone::PPC
static const char *s_riscv_name = "Disasm::Capstone::RiscV";
static MGVTBL riscv_magic_vt = {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        cap_magic_free<riscv_disasm>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};


template <typename T>
static T *get_disasm(SV *obj, MGVTBL *tab)
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

IV
version(SV *self)
 CODE:
   RETVAL= cs_version(NULL, NULL);
  OUTPUT:
    RETVAL

void
err(SV *self)
 INIT:
   auto *d = cabase_get(self);
 PPCODE:
   if ( !d || !d->handle )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVuv(cs_errno(d->handle)) );
   XSRETURN(1);

void
grp_cnt(SV *self)
 INIT:
   auto *d = cabase_get(self);
 PPCODE:
   if ( !d || d->empty() )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVuv(d->insn->detail->groups_count) );
   XSRETURN(1);

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
apply(SV *self, SV *pad)
 INIT:
   auto *d = get_disasm<ppc_disasm>(self, &ppc_magic_vt);
   auto *rp = get_disasm<ppc_regs>(pad, &ppc_regpad_magic_vt);
PPCODE:
   if ( !d || !rp || d->empty() || !d->insn->detail )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVuv( d->apply(rp) ) );
  XSRETURN(1);

void
is_ls(SV *self, SV *pad)
 INIT:
   auto *d = get_disasm<ppc_disasm>(self, &ppc_magic_vt);
   auto *rp = get_disasm<ppc_regs>(pad, &ppc_regpad_magic_vt);
PPCODE:
   if ( !d || !rp || d->empty() || !d->insn->detail )
    ST(0) = &PL_sv_undef;
   else {
     int kind = d->is_ls();
     if ( !kind ) ST(0) = &PL_sv_undef;
     else {
       // like mips - in case of success return ref to array [ kind arg_idx off ]
       auto arg_idx = rp->arg( d->insn->detail->ppc.operands[1].mem.base );
       if ( -1 == arg_idx || rp->has(d->insn->detail->ppc.operands[1].mem.base) ) ST(0) = &PL_sv_undef;
       else {
         AV *av = newAV();
         av_push(av, newSViv( kind ));
         av_push(av, newSViv( arg_idx ));
         av_push(av, newSViv( d->insn->detail->ppc.operands[1].mem.disp ));
         ST(0) = newRV_noinc((SV*)av);
         // for load clear dst register
         if ( kind == 1 ) rp->clean( d->get_reg(0) );
       }
     }
   }
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
       // for abi set r12 to addr + args
       res->set(PPC_REG_R12, d->addr);
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

MODULE = Disasm::Capstone		PACKAGE = Disasm::Capstone::RiscV

void
new(obj_or_pkg, SV *elsv)
  SV *obj_or_pkg
 INIT:
  HV *pkg = NULL;
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  struct IElf *e= extract(elsv);
  riscv_disasm *res = nullptr;
  int mod = 0;
 PPCODE:
  // check what we have
  ELFIO::Elf_Half machine = e->rdr->get_machine();
  if ( machine != ELFIO::EM_RISCV ) {
    my_warn("new: not RiscV elf");
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  auto cl = e->rdr->get_class();
  if ( cl == ELFIO::ELFCLASS32 ) mod = CS_MODE_RISCV32 | CS_MODE_RISCVC;
  else mod = CS_MODE_RISCV64 | CS_MODE_RISCVC;
  if (SvPOK(obj_or_pkg) && (pkg= gv_stashsv(obj_or_pkg, 0))) {
    if (!sv_derived_from(obj_or_pkg, "Disasm::Capstone"))
        croak("Package %s does not derive from Disasm::Capstone", SvPV_nolen(obj_or_pkg));
  } else
    croak("new: first arg must be package name or blessed object");
  // make real disasm object
  e->add_ref();
  res = new riscv_disasm( e );
  if ( e->rdr->get_encoding() == ELFIO::ELFDATA2MSB ) mod |= CS_MODE_BIG_ENDIAN;
  // try to open
  cs_err err = cs_open(CS_ARCH_RISCV, (cs_mode)mod, &res->handle);
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
  sv_bless(objref, s_riscv_pkg);
  ST(0)= objref;
// attach magic
  magic = sv_magicext(msv, NULL, PERL_MAGIC_ext, &riscv_magic_vt, (const char*)res, 0);
#ifdef USE_ITHREADS
  magic->mg_flags |= MGf_DUP;
#endif
  XSRETURN(1);

void
disasm(SV *self)
 INIT:
   auto *d = get_disasm<riscv_disasm>(self, &riscv_magic_vt);
 PPCODE:
   if ( !d ) {
     my_warn("riscv disasm: cannot get disasm");
     ST(0) = &PL_sv_undef;
   } else {
     if ( d->disasm() )
       ST(0) = &PL_sv_yes;
     else
       ST(0) = &PL_sv_no;
  }
  XSRETURN(1);

void
is_jxx(SV *self)
 INIT:
   auto *d = get_disasm<riscv_disasm>(self, &riscv_magic_vt);
 PPCODE:
   if ( !d || d->empty() || !d->insn->detail ) {
     ST(0) = &PL_sv_undef;
   } else {
     unsigned long addr = 0;
     if ( d->is_jxx(addr) ) {
       ST(0) = sv_2mortal( newSVuv(addr) );
     } else
      ST(0) = &PL_sv_undef;
   }
   XSRETURN(1);

void
ea(SV *self)
 INIT:
   auto *d = get_disasm<riscv_disasm>(self, &riscv_magic_vt);
 PPCODE:
   if ( !d ) {
     ST(0) = &PL_sv_undef;
   } else {
     if ( d->insn->detail->riscv.need_effective_addr )
       ST(0) = &PL_sv_yes;
     else
       ST(0) = &PL_sv_no;
  }
  XSRETURN(1);

void
op_cnt(SV *self)
 INIT:
   auto *d = cabase_get(self);
PPCODE:
   if ( !d || d->empty() || !d->insn->detail )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSViv(d->insn->detail->riscv.op_count ) );
   XSRETURN(1);

void
op_type(SV *self, IV idx)
 INIT:
   auto *d = cabase_get(self);
PPCODE:
   if ( !d || d->empty() || !d->insn->detail || idx >= d->insn->detail->riscv.op_count )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSViv(d->insn->detail->riscv.operands[idx].type) );
   XSRETURN(1);

void
op_access(SV *self, IV idx)
 INIT:
   auto *d = cabase_get(self);
PPCODE:
   if ( !d || d->empty() || !d->insn->detail || idx >= d->insn->detail->riscv.op_count )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSViv(d->insn->detail->riscv.operands[idx].access) );
   XSRETURN(1);

void
op_reg(SV *self, IV idx)
 INIT:
   auto *d = cabase_get(self);
PPCODE:
   if ( !d || d->empty() || !d->insn->detail || idx >= d->insn->detail->riscv.op_count )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSViv(d->insn->detail->riscv.operands[idx].reg) );
   XSRETURN(1);

void
op_imm(SV *self, IV idx)
 INIT:
   auto *d = cabase_get(self);
PPCODE:
   if ( !d || d->empty() || !d->insn->detail || idx >= d->insn->detail->riscv.op_count )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSViv(d->insn->detail->riscv.operands[idx].imm) ); // imm is signed so return IV
   XSRETURN(1);

void
op_mem(SV *self, IV idx)
 INIT:
   auto *d = cabase_get(self);
   AV *av;
PPCODE:
   if ( !d || d->empty() || !d->insn->detail || idx >= d->insn->detail->riscv.op_count )
    ST(0) = &PL_sv_undef;
   else {
     av = newAV();
     // return ref to [ base disp ], disp is signed
     av_push(av, newSViv( d->insn->detail->riscv.operands[idx].mem.base ));
     av_push(av, newSViv( d->insn->detail->riscv.operands[idx].mem.disp ));
     ST(0) = newRV_noinc((SV*)av);
   }
   XSRETURN(1);

BOOT:
 s_ppc_pkg = gv_stashpv(s_ppc_name, 0);
 if ( !s_ppc_pkg )
    croak("Package %s does not exists", s_ppc_name);
 s_riscv_pkg = gv_stashpv(s_riscv_name, 0);
 if ( !s_riscv_pkg )
    croak("Package %s does not exists", s_riscv_name);
 s_ppc_regpad_pkg = gv_stashpv(s_ppc_regpad_name, 0);
 if ( !s_ppc_regpad_pkg )
    croak("Package %s does not exists", s_ppc_regpad_name);
 // register archs
 cs_arch_register_powerpc();
 cs_arch_register_riscv();
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
