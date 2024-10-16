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

template <typename T>
static T *Elf_get_tmagic(SV *obj, int die, MGVTBL *tab)
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
        if (magic->mg_type == PERL_MAGIC_tied && magic->mg_virtual == tab)
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

static const char *s_seciter = "Elf::Reader::SecIterator",
 *s_segiter = "Elf::Reader::SegIterator";

// see https://github.com/Perl/perl5/blob/blead/mg.c
static U32
secs_magic_sizepack(pTHX_ SV *sv, MAGIC *mg)
{
  U32 res = 0;
  if (mg->mg_ptr) {
    IElf *e = (IElf *)mg->mg_ptr;
    res = e->rdr->sections.size();
  }
  return res;
}

static U32
segs_magic_sizepack(pTHX_ SV *sv, MAGIC *mg)
{
  U32 res = 0;
  if (mg->mg_ptr) {
    IElf *e = (IElf *)mg->mg_ptr;
    res = e->rdr->segments.size();
  }
  return res;
}

// magic table for Elf::Reader
static MGVTBL Elf_magic_vt = {
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

// magic table for Elf::Reader::SecIterator
static MGVTBL Elf_magic_sec = {
        0, /* get */
        0, /* write */
        secs_magic_sizepack, /* length */
        0, /* clear */
        elf_magic_free,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

// magic table for Elf::Reader::SegIterator
static MGVTBL Elf_magic_seg = {
        0, /* get */
        0, /* write */
        segs_magic_sizepack, /* length */
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
    croak("new: cannot load Elf file %s", fname);
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
secs(SV *arg)
 INIT:
  HV *pkg = NULL;
  AV *fake = NULL;
  struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
  SV *objref= NULL;
  MAGIC* magic;
 PPCODE:
  if ( !(pkg = gv_stashpv(s_seciter, 0)) ) {
    croak("Package %s does not exists", s_seciter);
  }
  fake = newAV();
  objref = newRV_noinc((SV*)fake);
  sv_bless(objref, pkg);
  // set PERL_MAGIC_tied for fake av
  e->add_ref();
  magic = sv_magicext((SV*)fake, NULL, PERL_MAGIC_tied, &Elf_magic_sec, (const char *)e, 0);
  SvREADONLY_on((SV*)fake);
  ST(0) = objref;
  XSRETURN(1);

void
segs(SV *arg)
 INIT:
  HV *pkg = NULL;
  AV *fake = NULL;
  struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
  SV *objref= NULL;
  MAGIC* magic;
 PPCODE:
  if ( !(pkg = gv_stashpv(s_segiter, 0)) ) {
    croak("Package %s does not exists", s_segiter);
  }
  fake = newAV();
  objref = newRV_noinc((SV*)fake);
  sv_bless(objref, pkg);
  // set PERL_MAGIC_tied for fake av
  e->add_ref();
  magic = sv_magicext((SV*)fake, NULL, PERL_MAGIC_tied, &Elf_magic_seg, (const char *)e, 0);
  SvREADONLY_on((SV*)fake);
  ST(0) = objref;
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

MODULE = Elf::Reader		PACKAGE = Elf::Reader::SecIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 INIT:
  SV *str;
  struct IElf *e = Elf_get_tmagic<IElf>(self, 1, &Elf_magic_sec);
 PPCODE:
  if ( key >= e->rdr->sections.size() )
    str = &PL_sv_undef;
  else {
    auto s = e->rdr->sections[key]->get_name();
    str = newSVpv(s.c_str(), s.size());
  }
  ST(0) = str;
  XSRETURN(1);


MODULE = Elf::Reader		PACKAGE = Elf::Reader::SegIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 PREINIT:
  U8 gimme = GIMME_V;
 INIT:
  struct IElf *e = Elf_get_tmagic<IElf>(self, 1, &Elf_magic_seg);
 PPCODE:
  if ( key >= e->rdr->segments.size() )
    ST(0) = &PL_sv_undef;
  else {
    auto s = e->rdr->segments[key];
    // we can return ref to array or array itself
    // see https://stackoverflow.com/questions/46719061/perl-xs-return-perl-array-from-c-array
    // format of array:
    // 0 - index
    // 1 - type
    // 2 - flags
    // 3 - align
    // 4 - virtual address, 64bit
    // 5 - physical address, 64bit
    // 6 - file size
    // 7 - memory size
    // 8 - offset, 64bit
    if ( gimme == G_ARRAY) {
      EXTEND(SP, 9);
      mXPUSHi(s->get_index());
      mXPUSHi(s->get_type());
      mXPUSHi(s->get_flags());
      mXPUSHi(s->get_align());
      mXPUSHu(s->get_virtual_address());
      mXPUSHu(s->get_physical_address());
      mXPUSHu(s->get_file_size());
      mXPUSHu(s->get_memory_size());
      mXPUSHu(s->get_offset());
      XSRETURN(9);
    } else {
      AV *av = newAV();
      mXPUSHs(newRV_noinc((SV*)av));
      av_push(av, newSViv( s->get_index() ));
      av_push(av, newSViv( s->get_type() ));
      av_push(av, newSViv( s->get_flags() ));
      av_push(av, newSViv( s->get_align() ));
      av_push(av, newSVuv( s->get_virtual_address() ));
      av_push(av, newSVuv( s->get_physical_address() ));
      av_push(av, newSVuv( s->get_file_size() ));
      av_push(av, newSVuv( s->get_memory_size() ));
      av_push(av, newSVuv( s->get_offset() ));
    }
  }
  XSRETURN(1);

