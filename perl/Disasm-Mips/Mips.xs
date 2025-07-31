// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "elfio/elfio.hpp"
#include "../elf.inc"
#include "mips.h"
#include <bitset>

void my_warn(const char * pat, ...) {
 va_list args;
 va_start(args, pat);
 vwarn(pat, &args);
}

static const char *s_regpad = "Disasm::Mips::Regpad";

// for handling lui/lw pairs
struct mips_regs {
  int64_t regs[mips::REG_RA];
  std::bitset<mips::REG_RA> pres;
  // for moves
  char m[mips::REG_RA];
  mips_regs() {
    memset(m, -1, mips::REG_RA);
  }
  mips_regs(mips_regs &) = default;
  int64_t get(int idx) const
  {
    if ( idx >= mips::REG_RA || idx < 0 ) return 0;
    if ( !pres[idx] ) return 0;
    return regs[idx];
  }
  int64_t set(int idx, int64_t v)
  {
    if ( idx >= mips::REG_RA || idx < 0 ) return 0;
    pres[idx] = 1; m[idx] = -1;
    regs[idx] = v;
    return v;
  }
  int move(int idx, int src)
  {
    if ( idx >= mips::REG_RA || idx < 0 || src >= mips::REG_RA || src < 0 ) return 0;
    regs[idx] = regs[src];
    pres[idx] = pres[src];
    m[idx] = m[src];
    return 1;
  }
  int clear(int idx)
  {
    if ( idx >= mips::REG_RA || idx < 0 ) return 0;
    pres[idx] = 0;
    regs[idx] = 0;
    m[idx] = -1;
    return 1;
  }
  // a0 .. a3, see https://refspecs.linuxfoundation.org/elf/mipsabi.pdf
  void abi() {
    for ( int i = mips::REG_A0; i <= mips::REG_A3; ++i )
     m[i] = i - mips::REG_A0;
  }
  int base(int idx, int &breg)
  {
    if ( idx >= mips::REG_RA || idx < 0 ) return 0;
    if ( m[idx] != -1 ) {
      breg = m[idx];
      return 1;
    }
    return 0;
  }
};

struct mdis {
  const char *psp, *end;
  unsigned long addr, start;
  mips::MipsVersion m_mv;
  mips::Instruction inst;
  char txt[1024];
  IElf *e;
  mdis(IElf *_e, mips::MipsVersion v): e(_e), m_mv(v)
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
     case mips::MIPS_J:
      return 1;
   }
   return 0;
 }
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
   inst.size = 0; // empty after setup
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
   inst.size = 0; // empty after setup
   return 1;
 }
 int disasm()
 {
   if ( psp >= end ) return 0;
   memset(&inst, 0, sizeof(inst));
   int rc = mips::mips_decompose((const uint32_t*)psp, end - psp, &inst, m_mv, (uint64_t)addr, e->needswap, 1);
   if ( rc ) return 0;
   // update addr & psp
   psp += inst.size;
   addr += inst.size;
   // check for speculative execution
   if ( is_end() )
     end = psp + 4;
   return inst.size;
 }
 // some shortcuts to avoid multiple methods invocation
 unsigned long is_jal() const
 {
   if ( empty() ) return 0;
   if ( inst.operation == mips::MIPS_JAL && inst.operands[0].operandClass == mips::LABEL )
     return inst.operands[0].immediate;
   return 0;
 }
 // bxx label
 unsigned long is_jxx() const
 {
   if ( empty() ) return 0;
using namespace mips;
   // https://www.cs.cmu.edu/afs/cs/academic/class/15740-f97/public/doc/mips-isa.pdf
   switch(inst.operation) {
     case MIPS_B:
     case MIPS_J:
      if ( inst.operands[0].operandClass == mips::LABEL )
        return inst.operands[0].immediate;
      break;

     // REG/LABEL
     case MIPS_BEQZ: // Branch on Equal Zero
     case MIPS_BGEZ:  // Branch on Greater Than Equal Zero
     case MIPS_BGEZAL:
     case MIPS_BGEZALL:
     case MIPS_BGEZL:
     case MIPS_BLTZL: // Branch on Greater Than or Equal to Zero Likely
     case MIPS_BLTZAL:
     case MIPS_BLTZALL:
     case MIPS_BLTZ:  // Branch on Less Than Zero
     case MIPS_BGTZ:  // Branch on Greater Than Zero
     case MIPS_BGTZL: // Branch on Greater Than Zero Likely
     case MIPS_BLEZ:  // Branch on Less Than or Equal to Zero
     case MIPS_BLEZL: // Branch on Less Than or Equal to Zero Likely
      if ( inst.operands[1].operandClass == mips::LABEL )
        return inst.operands[1].immediate;
      break;

     // reg/reg/LABEL
     case MIPS_BEQ:
     case MIPS_BEQL: // Branch on Equal Likely
     case MIPS_BNE:
     case MIPS_BNEL:  // Branch on Not Equal Likely
      if ( inst.operands[2].operandClass == mips::LABEL )
        return inst.operands[2].immediate;
      break;

     // next group can be LABEL or FLAG/LABEL
     case MIPS_BC1T:
     case MIPS_BC1F:
     case MIPS_BC1FL:
     case MIPS_BC1TL:
     case MIPS_BC2F:
     case MIPS_BC2FL:
     case MIPS_BC2T:
     case MIPS_BC2TL:
      if ( inst.operands[0].operandClass == mips::LABEL )
        return inst.operands[0].immediate;
      if ( inst.operands[1].operandClass == mips::LABEL )
        return inst.operands[1].immediate;
      break;
   }
   return 0;
 }
 int64_t apply(mips_regs *mr)
 {
   if ( inst.operation == mips::MIPS_MOVE )
   {
     mr->move(inst.operands[0].reg, inst.operands[1].reg);
     return 0;
   }
   if ( (inst.operation == mips::MIPS_LUI || inst.operation == mips::MIPS_LI) &&
        inst.operands[0].operandClass == mips::OperandClass::REG &&
        inst.operands[1].operandClass == mips::OperandClass::IMM )
    return mr->set(inst.operands[0].reg, inst.operands[1].immediate << 16);
   if ( is_lw() )
   {
     auto old = mr->get(inst.operands[1].reg);
     if ( old ) {
       old += (int)inst.operands[1].immediate;
       return mr->set(inst.operands[0].reg, old);
     } else {
       old = (int)inst.operands[1].immediate;
       mr->set(inst.operands[0].reg, old);
       return 0;
     }
   }
   int val = 0;
   if ( is_addiu(val) ) {
     auto old = mr->get(inst.operands[1].reg);
     if ( old ) {
       old += val;
       return mr->set(inst.operands[0].reg, old);
     }
   }
   if ( inst.operands[0].operandClass == mips::OperandClass::REG && is_dst() )
     mr->clear(inst.operands[0].reg);
   return 0;
 }
 // boring stuff
 int is_dst() const
 {
using namespace mips;
   // https://www.cs.cmu.edu/afs/cs/academic/class/15740-f97/public/doc/mips-isa.pdf
   switch(inst.operation) {
     case MIPS_ABS_D:
     case MIPS_ABS_PS:
     case MIPS_ABS_S:
     case MIPS_DADD:
     case MIPS_DADDI:
     case MIPS_DADDIU:
     case MIPS_DADDU:
     case MIPS_ADD:
     case MIPS_ADDU:
     case MIPS_ADDIU:
     case MIPS_SUB:
     case MIPS_DSUB:
     case MIPS_SUBU:
     case MIPS_DSUBU:
     case MIPS_MUL:
     case MIPS_MULT:
     case MIPS_MULTU:
     case MIPS_DIV_D:
     case MIPS_DIV_PS:
     case MIPS_DIV_S:
     case MIPS_MFHI:
     case MIPS_MTHI:
     case MIPS_MFLO:
     case MIPS_MTLO:
     case MIPS_NEG_D:
     case MIPS_NEG_PS:
     case MIPS_NEG_S:
     case MIPS_NEG:
     case MIPS_NEGU:
     case MIPS_NOR:
     case MIPS_NOT:
     case MIPS_C_SEQ_D:
     case MIPS_C_SEQ_PS:
     case MIPS_C_SEQ_S:
     case MIPS_C_SEQ:
     case MIPS_SRA:
     case MIPS_SRAV:
     case MIPS_DSRA:
     case MIPS_SLT:
     case MIPS_SLTI:
     case MIPS_SLTU:
     case MIPS_SLTIU:
     case MIPS_AND:
     case MIPS_OR:
     case MIPS_ANDI:
     case MIPS_ORI:
     case MIPS_XOR:
     case MIPS_XORI:
     case MIPS_SLL:
     case MIPS_SLLV:
     case MIPS_SRL:
     case MIPS_SRLV:
     case MIPS_DSRL:
     case MIPS_DSLL:
     case MIPS_MOVN:
     case MIPS_MOVZ:
      return 1;
   }
   return 0;
 }
 int is_lbX() const
 {
   return (inst.operation == mips::MIPS_LBU || inst.operation == mips::MIPS_LB ||
      inst.operation == mips::MIPS_LH || inst.operation == mips::MIPS_LHU ||
      inst.operation == mips::MIPS_LW || inst.operation == mips::MIPS_LL || inst.operation == mips::MIPS_LLD) &&
    inst.operands[0].operandClass == mips::OperandClass::REG &&
    inst.operands[1].operandClass == mips::OperandClass::MEM_IMM;
 }
 int is_stX() const
 {
  return (inst.operation == mips::MIPS_SB || inst.operation == mips::MIPS_SH ||
       inst.operation == mips::MIPS_SW || inst.operation == mips::MIPS_SD) &&
    inst.operands[0].operandClass == mips::OperandClass::REG &&
    inst.operands[1].operandClass == mips::OperandClass::MEM_IMM;
 }
 int is_lw() const
 {
   return (inst.operation == mips::MIPS_LW || inst.operation == mips::MIPS_LBU) &&
          inst.operands[0].operandClass == mips::OperandClass::REG &&
          inst.operands[1].operandClass == mips::OperandClass::MEM_IMM;
 }
 int is_addiu(int &val) const
 {
   if ( inst.operation == mips::MIPS_ADDIU &&
        inst.operands[0].operandClass == mips::OperandClass::REG &&
        inst.operands[1].operandClass == mips::OperandClass::REG &&
        inst.operands[2].operandClass == mips::OperandClass::IMM )
   {
     val = (int)inst.operands[2].immediate;
     return 1;
   }
   return 0;
 }
 // return 1 if instr is lX reg, [base + off],
 //        2 if instr is sX reg, [base + off],
 // 0 otherwise
 int is_ls(mips_regs *mr, int &base, int &off)
 {
   if ( is_lbX() && mr->base(inst.operands[1].reg, base) ) {
     off = inst.operands[1].immediate;
     return 1;
   }
   if ( is_stX() && mr->base(inst.operands[1].reg, base) ) {
     off = inst.operands[1].immediate;
     return 2;
   }
   if ( is_addiu(off) && mr->base(inst.operands[1].reg, base) ) {
     mr->clear(inst.operands[0].reg);
     return 1;
   }
   return 0;
 }
};

template <typename T>
static int mips_magic_free(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        auto *m = (T *)mg->mg_ptr;
        if ( m ) delete m;
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
        mips_magic_free<mdis>,
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
    res = mips::REG_RA-1;
  }
  return res;
}

// magic table for Disasm::Arm64::Regpad
static MGVTBL regpad_magic_vt = {
        0, /* get */
        0, /* write */
        rpad_magic_sizepack, /* length */
        0, /* clear */
        mips_magic_free<mips_regs>,
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

mips_regs *regpad_get(SV *obj)
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
            return (mips_regs*) magic->mg_ptr;
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
  e->add_ref();
  res = new mdis( e, ver );
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
setup2(SV *sv, unsigned long addr, unsigned long len)
 INIT:
   mdis *d = mdis_get(sv);
 PPCODE:
   ST(0) = sv_2mortal( newSVuv( d->setup(addr, len) ) );
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
flag_name(int f)
 INIT:
  const char *name = mips::get_flag(mips::Flag(f));
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

unsigned long
is_jal(SV *sv)
 INIT:
  mdis *d = mdis_get(sv);
 CODE:
  RETVAL = d->is_jal();
 OUTPUT:
  RETVAL

unsigned long
is_jxx(SV *sv)
 INIT:
  mdis *d = mdis_get(sv);
 CODE:
  RETVAL = d->is_jxx();
 OUTPUT:
  RETVAL

void
is_ls(SV *a, SV *r)
 INIT:
   mdis *d = mdis_get(a);
   mips_regs *rp = regpad_get(r);
   int base = -1, res, off = 0;
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

void
apply(SV *a, SV *r)
 INIT:
   mdis *d = mdis_get(a);
   mips_regs *rp = regpad_get(r);
 PPCODE:
   if ( d->empty() )
    ST(0) = &PL_sv_undef;
   else
    ST(0) = sv_2mortal( newSVuv( (unsigned long)d->apply(rp) ) );
   XSRETURN(1);

void
regpad(SV *sv)
  INIT:
  HV *pkg = NULL;
  AV *fake = NULL;
  mdis *d = mdis_get(sv);
  mips_regs *rp = NULL;
  SV *objref= NULL;
  MAGIC* magic;
 PPCODE:
  if ( !(pkg = gv_stashpv(s_regpad, 0)) ) {
    croak("Package %s does not exists", s_regpad);
    XSRETURN(0);
  }
  rp = new mips_regs();
  // tie on array
  fake = newAV();
  objref = newRV_noinc((SV*)fake);
  sv_bless(objref, pkg);
  magic = sv_magicext((SV*)fake, NULL, PERL_MAGIC_tied, &regpad_magic_vt, (const char *)rp, 0);
  SvREADONLY_on((SV*)fake);
  ST(0) = objref;
  XSRETURN(1);


MODULE = Disasm::Mips		PACKAGE = Disasm::Mips::Regpad

void
FETCH(self, key)
  SV *self;
  IV key;
 INIT:
  auto *r = regpad_get(self);
 PPCODE:
  if ( key >= mips::REG_RA || key < 0 || !r->pres[key] )
    ST(0) = &PL_sv_undef;
  else
    ST(0) = sv_2mortal( newSVuv( (unsigned long)r->get(key) ) );
  XSRETURN(1);

void
clone(SV *r)
 INIT:
  HV *pkg = NULL;
  AV *fake = NULL;
  mips_regs *rp = regpad_get(r), *res = NULL;
  SV *objref= NULL;
  MAGIC* magic;
 PPCODE:
  if ( !(pkg = gv_stashpv(s_regpad, 0)) ) {
    croak("Package %s does not exists", s_regpad);
    XSRETURN(0);
  }
  res = new mips_regs(*rp);
  // tie on array
  fake = newAV();
  objref = newRV_noinc((SV*)fake);
  sv_bless(objref, pkg);
  magic = sv_magicext((SV*)fake, NULL, PERL_MAGIC_tied, &regpad_magic_vt, (const char *)res, 0);
  SvREADONLY_on((SV*)fake);
  ST(0) = objref;
  XSRETURN(1);

void
abi(SV *self)
  INIT:
  mips_regs *mr = regpad_get(self);
 CODE:
  mr->abi();

int
reset(SV *self, int key)
  INIT:
  mips_regs *mr = regpad_get(self);
 CODE:
  mr->abi();
  RETVAL = mr->clear(key);
 OUTPUT:
  RETVAL


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
