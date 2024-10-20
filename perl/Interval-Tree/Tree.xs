// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "interval_tree.hpp"

typedef lib_interval_tree::interval_tree<lib_interval_tree::interval<unsigned long, lib_interval_tree::right_open>> ITree;

// all boring stuff like in Elf::Reader
static ITree *itree_get_magic(SV *obj, int die, MGVTBL *tab)
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
            return (ITree*) magic->mg_ptr;
    }
  return NULL;
}

static int itree_magic_free(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        ITree *t = (ITree *)mg->mg_ptr;
        delete t;
        mg->mg_ptr= NULL;
    }
    return 0; // ignored anyway
}

// magic table for Elf::Reader
static MGVTBL itree_magic_vt = {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        itree_magic_free,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};


static const char *s_package = "Interval::Tree";

MODULE = Interval::Tree		PACKAGE = Interval::Tree		

void
new(SV *obj_or_pkg)
 INIT:
  HV *pkg = NULL;
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  ITree *t = NULL;
 PPCODE:
  if (SvPOK(obj_or_pkg) && (pkg= gv_stashsv(obj_or_pkg, 0))) {
    if (!sv_derived_from(obj_or_pkg, s_package))
        croak("Package %s does not derive from %s", SvPV_nolen(obj_or_pkg), s_package);
    msv = newSViv(0);
    objref= sv_2mortal(newRV_noinc(msv));
    sv_bless(objref, pkg);
    ST(0)= objref;
  } else
        croak("new: first arg must be package name or blessed object");
  t = new ITree();
  magic = sv_magicext(msv, NULL, PERL_MAGIC_ext, &itree_magic_vt, (const char*)t, 0);
#ifdef USE_ITHREADS
  magic->mg_flags |= MGf_DUP;
#endif
  XSRETURN(1);

void
insert(SV *arg, unsigned long start, unsigned long len)
 INIT:
   auto *t= itree_get_magic(arg, 1, &itree_magic_vt);
 PPCODE:
   t->insert( { start, start + len } );
   XSRETURN(0);

void
deoverlap(SV *arg)
 INIT:
   auto *t= itree_get_magic(arg, 1, &itree_magic_vt);
 PPCODE:
   t->deoverlap();
   XSRETURN(0);

void
in_tree(SV *arg, unsigned long addr)
 INIT:
   auto *t= itree_get_magic(arg, 1, &itree_magic_vt);
   int res = 0;
 PPCODE:
   auto find = t->overlap_find( { addr, addr + 1 } );
   if ( find != t->end() ) res = 1;
   ST(0)= sv_2mortal( newSViv( res ) );
   XSRETURN(1);

void
next(SV *arg, unsigned long addr)
 INIT:
   auto *t= itree_get_magic(arg, 1, &itree_magic_vt);
 PPCODE:
   auto find = t->overlap_find( { addr, addr + 1 } );
   // if addr already in tree return undef
   if ( find != t->end() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
   }
   // find next overlapped in tree
   auto next = t->overlap_find( { addr, ULONG_MAX });
   if ( next == t->end() )
     ST(0)= sv_2mortal( newSVuv(0) );
   else
     ST(0)= sv_2mortal( newSVuv( next->low() ) );
   XSRETURN(1);
