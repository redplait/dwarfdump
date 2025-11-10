// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "interval_tree.hpp"
#include <vector>

using BaseInterval = lib_interval_tree::interval<unsigned long, lib_interval_tree::right_open>;
typedef lib_interval_tree::interval_tree<BaseInterval> ITree;

// my custom interval with SV attached
struct my_interval: public BaseInterval
{
  SV *sv = nullptr;
  my_interval(value_type _low, value_type _hi, SV *_sv): BaseInterval(_low, _hi)
  { sv = _sv; if ( sv ) SvREFCNT_inc(sv); }
  ~my_interval() override
  { if ( sv ) SvREFCNT_dec(sv); }
  // copy constructor
  my_interval(my_interval &ot): BaseInterval(ot.low_, ot.high_)
  { sv = ot.sv; if ( sv ) SvREFCNT_inc(sv); }
  // move constructor
  my_interval(my_interval &&ot): BaseInterval(ot.low_, ot.high_)
  { sv = ot.sv; ot.sv = nullptr; }
};

typedef lib_interval_tree::interval_tree<my_interval> SVTree;

// all boring stuff like in Elf::Reader
template <typename T>
static T *itree_get_magic(SV *obj, int die, MGVTBL *tab)
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

template <typename T>
static int itree_magic_free(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        T *t = (T *)mg->mg_ptr;
        delete t;
        mg->mg_ptr= NULL;
    }
    return 0; // ignored anyway
}

// magic table for Interval::Tree
static MGVTBL itree_magic_vt = {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        itree_magic_free<ITree>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

// magic table for Interval::Tree::SV
static MGVTBL itreeSV_magic_vt = {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        itree_magic_free<SVTree>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};


static const char *s_package = "Interval::Tree";
static const char *s_sv = "Interval::Tree::SV";

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
   auto *t= itree_get_magic<ITree>(arg, 1, &itree_magic_vt);
 PPCODE:
   t->insert( { start, start + len } );
   XSRETURN(0);

void
deoverlap(SV *arg)
 INIT:
   auto *t= itree_get_magic<ITree>(arg, 1, &itree_magic_vt);
 PPCODE:
   t->deoverlap();
   XSRETURN(0);

void
in_tree(SV *arg, unsigned long addr)
 INIT:
   auto *t= itree_get_magic<ITree>(arg, 1, &itree_magic_vt);
 PPCODE:
   auto find = t->overlap_find( { addr, addr + 1 } );
   ST(0)= ( find != t->end() ) ? &PL_sv_yes : &PL_sv_no;
   XSRETURN(1);

void
next(SV *arg, unsigned long addr)
 INIT:
   auto *t= itree_get_magic<ITree>(arg, 1, &itree_magic_vt);
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

MODULE = Interval::Tree		PACKAGE = Interval::Tree::SV

void
new(SV *obj_or_pkg)
 INIT:
  HV *pkg = NULL;
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  SVTree *t = NULL;
 PPCODE:
  if (SvPOK(obj_or_pkg) && (pkg= gv_stashsv(obj_or_pkg, 0))) {
    if (!sv_derived_from(obj_or_pkg, s_sv))
        croak("Package %s does not derive from %s", SvPV_nolen(obj_or_pkg), s_sv);
    msv = newSViv(0);
    objref= sv_2mortal(newRV_noinc(msv));
    sv_bless(objref, pkg);
    ST(0)= objref;
  } else
        croak("new: first arg must be package name or blessed object");
  t = new SVTree();
  magic = sv_magicext(msv, NULL, PERL_MAGIC_ext, &itreeSV_magic_vt, (const char*)t, 0);
#ifdef USE_ITHREADS
  magic->mg_flags |= MGf_DUP;
#endif
  XSRETURN(1);

void
insert(SV *arg, unsigned long start, unsigned long len, SV *what)
 INIT:
   auto *t= itree_get_magic<SVTree>(arg, 1, &itreeSV_magic_vt);
 PPCODE:
   t->insert( { start, start + len, what } );
   XSRETURN(0);

void
in_tree(SV *arg, unsigned long addr)
 INIT:
   auto *t= itree_get_magic<SVTree>(arg, 1, &itreeSV_magic_vt);
   int res = 0;
 PPCODE:
   auto find = t->overlap_find( { addr, addr + 1, nullptr } );
   if ( find == t->end() || !find->sv )
     ST(0) = &PL_sv_undef;
   else
     ST(0)= SvREFCNT_inc(find->sv);
   XSRETURN(1);

void
in_all(SV *arg, unsigned long addr)
 PREINIT:
  U8 gimme = GIMME_V;
 INIT:
   auto *t= itree_get_magic<SVTree>(arg, 1, &itreeSV_magic_vt);
   std::vector<SV *> res;
 PPCODE:
   t->overlap_find_all( { addr, addr + 1, nullptr }, [&](auto iter) { res.push_back(iter->sv); return true; } );
   if ( res.empty() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  } else {
    if ( gimme == G_ARRAY) {
      EXTEND(SP, res.size());
      for ( auto si: res )
       mPUSHs(SvREFCNT_inc(si));
    } else {
      AV *av = newAV();
      for ( auto si: res )
       av_push(av, SvREFCNT_inc(si));
      mXPUSHs(newRV_noinc((SV*)av));
      XSRETURN(1);
    }
  }


UV
in_cnt(SV *arg, unsigned long addr)
 INIT:
   auto *t= itree_get_magic<SVTree>(arg, 1, &itreeSV_magic_vt);
   UV res = 0;
 CODE:
   t->overlap_find_all( { addr, addr + 1, nullptr }, [&](auto iter) { res++; return true; } );
   RETVAL = res;
 OUTPUT:
  RETVAL


void
next(SV *arg, unsigned long addr)
 INIT:
   auto *t= itree_get_magic<SVTree>(arg, 1, &itreeSV_magic_vt);
 PPCODE:
   auto find = t->overlap_find( { addr, addr + 1, nullptr } );
   // if addr already in tree return undef
   if ( find != t->end() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
   }
   // find next overlapped in tree
   auto next = t->overlap_find( { addr, ULONG_MAX, nullptr });
   if ( next == t->end() )
     ST(0)= sv_2mortal( newSVuv(0) );
   else
     ST(0)= sv_2mortal( newSVuv( next->low() ) );
   XSRETURN(1);
