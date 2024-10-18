// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "mips.h"

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
