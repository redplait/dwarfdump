// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "elfio/elfio.hpp"

#include "ppport.h"

template <typename T>
static T *Elf_get_magic(SV *obj, int die, MGVTBL *tab)
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

struct IElf {
 int ref_cnt = 1;
 ELFIO::elfio *rdr;
 void add_ref() {
   ref_cnt++;
 }
 void release() {
   if ( !--ref_cnt ) delete this;
 };
 ~IElf() {
   if ( rdr ) delete rdr;
 }
};

static int elf_magic_free(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        IElf *e = (IElf *)mg->mg_ptr;
        e->release();
        mg->mg_ptr= NULL;
    }
    return 0; // ignored anyway
}

// magic table for Elf::Reader
static MGVTBL Elf_magic_vt= {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        elf_magic_free,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

static int s_rdr_id = 0;

MODULE = Elf::Reader		PACKAGE = Elf::Reader		

void
new(obj_or_pkg, const char *fname)
  SV *obj_or_pkg
 INIT:
  HV *pkg = NULL;
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  struct IElf *e= NULL;
  ELFIO::elfio *rde = NULL;
 PPCODE:
  if (SvPOK(obj_or_pkg) && (pkg= gv_stashsv(obj_or_pkg, 0))) {
    if (!sv_derived_from(obj_or_pkg, "Elf::Reader"))
        croak("Package %s does not derive from Elf:Reader", SvPV_nolen(obj_or_pkg));
    msv = newSViv(++s_rdr_id);
    objref= sv_2mortal(newRV_noinc(msv));
    sv_bless(objref, pkg);
    ST(0)= objref;
  } else
        croak("new: first arg must be package name or blessed object");
  // read elf file
  rde = new ELFIO::elfio();
  if ( !rde->load(fname) ) {
    delete rde;
    croak("new: cannit load Elf file %s", fname);
  } else {
    // attach magic to msv
    e = new IElf();
    e->rdr = rde;
    magic = sv_magicext(msv, NULL, PERL_MAGIC_ext, &Elf_magic_vt, (const char*)e, 0);
#ifdef USE_ITHREADS
    magic->mg_flags |= MGf_DUP;
#endif
  }
  XSRETURN(1);

void
get_class(SV *arg)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 PPCODE:
   ST(0)= sv_2mortal( newSViv( e->rdr->get_class() ) );
   XSRETURN(1);

void encoding(SV *arg)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 PPCODE:
   ST(0)= sv_2mortal( newSViv( e->rdr->get_encoding() ) );
   XSRETURN(1);

void type(SV *arg)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 PPCODE:
   ST(0)= sv_2mortal( newSViv( e->rdr->get_type() ) );
   XSRETURN(1);

void machine(SV *arg)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 PPCODE:
   ST(0)= sv_2mortal( newSViv( e->rdr->get_machine() ) );
   XSRETURN(1);

void flags(SV *arg)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 PPCODE:
   ST(0)= sv_2mortal( newSViv( e->rdr->get_flags() ) );
   XSRETURN(1);

void entry(SV *arg)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 PPCODE:
   ST(0)= sv_2mortal( newSVuv( e->rdr->get_entry() ) );
   XSRETURN(1);
