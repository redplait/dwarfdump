// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "elfio/elfio.hpp"
#include "../elf.inc"
#include <vector>

// const bank params
struct cb_param {
  int ordinal;
  unsigned short offset;
  size_t size;
};

struct CAttr {
  unsigned long offset;
  size_t len = 0;
  char attr;
};

struct CAttrs {
  CAttrs(IElf *e, int idx) {
    m_e = e;
    s_idx = idx;
  }
  ~CAttrs() {
    if ( m_e ) m_e->release();
  }
  IElf *m_e = nullptr;
  int s_idx = -1;
  std::vector<CAttr> m_attrs;
  // cb
  std::vector<cb_param> params;
  unsigned short cb_size = 0;
  unsigned short cb_offset = 0;
  // methods
  SV *fetch(int idx);
  SV *fetch_cb(int idx);
};

SV *CAttrs::fetch_cb(int idx) {
  // check idx
  if ( idx < 0 || idx >= params.size() ) {
    my_warn("invalid cb index %d\n", idx);
    return &PL_sv_undef;
  }
  // create and fill HV
  HV *hv = newHV();
  hv_store(hv, "off", 3, newSVuv(params[idx].offset), 0);
  hv_store(hv, "size", 4, newSVuv(params[idx].size), 0);
  hv_store(hv, "ord", 3, newSVuv(params[idx].ordinal), 0);
  return newRV_noinc((SV*)hv);
}

SV *CAttrs::fetch(int idx) {
  // check idx
  if ( idx < 0 || idx >= m_attrs.size() ) {
    my_warn("invalid index %d\n", idx);
    return &PL_sv_undef;
  }
  // create and fill HV
  HV *hv = newHV();
  hv_store(hv, "attr", 4, newSViv(m_attrs[idx].attr), 0);
  hv_store(hv, "off", 3, newSVuv(m_attrs[idx].offset), 0);
  hv_store(hv, "len", 3, newSVuv(m_attrs[idx].len), 0);
  return newRV_noinc((SV*)hv);
}


// check if we have real section with attributes
static int check_section(ELFIO::elfio *rdr, int idx) {
  if ( idx < 0 || idx >= rdr->sections.size() ) {
    my_warn("Cubin::Attrs: invalid section index %d\n", idx);
    return 0;
  }
  auto s = rdr->sections[idx];
  auto st = s->get_type();
  if ( st == ELFIO::SHT_NOBITS ) {
    my_warn("Cubin::Attrs: empty section %d\n", idx);
    return 0;
  }
  if ( !s->get_size() ) {
    my_warn("Cubin::Attrs: empty section index %d\n", idx);
    return 0;
  }
  return (st == 0x70000000) || (st == 0x70000083);
}

#ifdef MGf_LOCAL
#define TAB_TAIL ,0
#else
#define TAB_TAIL
#endif

static U32 my_len(pTHX_ SV *sv, MAGIC* mg);

// magic table for Cubin::Attrs
static const char *s_ca = "Cubin::Attrs";
static HV *s_ca_pkg = nullptr;
static MGVTBL ca_magic_vt = {
        0, /* get */
        0, /* write */
        my_len, /* length */
        0, /* clear */
        magic_del<CAttrs>,
        0, /* copy */
        0 /* dup */
        TAB_TAIL
};

#define DWARF_TIE(vtab, pkg, what) \
  fake = newAV(); \
  objref = newRV_noinc((SV*)fake); \
  sv_bless(objref, pkg); \
  magic = sv_magicext((SV*)fake, NULL, PERL_MAGIC_tied, &vtab, (const char *)what, 0); \
  SvREADONLY_on((SV*)fake); \
  ST(0) = objref; \
  XSRETURN(1);

static U32 my_len(pTHX_ SV *sv, MAGIC* mg)
{
  CAttrs *d = nullptr;
  if (SvMAGICAL(sv)) {
    MAGIC* magic;
    for (magic= SvMAGIC(sv); magic; magic = magic->mg_moremagic)
      if (magic->mg_type == PERL_MAGIC_tied && magic->mg_virtual == &ca_magic_vt) {
        d = (CAttrs*) magic->mg_ptr;
        break;
      }
  }
  if ( !d ) {
    my_warn("my_len %d\n", SvTYPE(sv));
    return 0;
  }
  return (U32)d->m_attrs.size();
}

MODULE = Cubin::Attrs		PACKAGE = Cubin::Attrs

void
new(obj_or_pkg, SV *elsv, int s_idx)
  SV *obj_or_pkg
 INIT:
  HV *pkg = NULL;
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  struct IElf *e= extract(elsv);
  AV *fake;
 PPCODE:
  if (SvPOK(obj_or_pkg) && (pkg= gv_stashsv(obj_or_pkg, 0))) {
    if (!sv_derived_from(obj_or_pkg, s_ca))
        croak("Package %s does not derive from %s", SvPV_nolen(obj_or_pkg), s_ca);
  } else
    croak("new: first arg must be package name or blessed object");
  if ( !check_section(e->rdr, s_idx) ) {
    my_warn("cannot create Cubin::Attrs for %s section %d\n", e->fname.c_str(), s_idx);
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  } else {
   // make new CAttrs
   auto res = new CAttrs(e, s_idx);
   DWARF_TIE(ca_magic_vt, s_ca_pkg, res)
 }

SV *
FETCH(SV *self, int idx)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->fetch(idx);
 OUTPUT:
  RETVAL

SV *
get_cb(SV *self, int idx)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->fetch_cb(idx);
 OUTPUT:
  RETVAL

UV
params_cnt(SV *self)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->params.size();
 OUTPUT:
  RETVAL

unsigned short
cb_size(SV *self)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->cb_size;
 OUTPUT:
  RETVAL

unsigned short
cb_off(SV *self)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->cb_offset;
 OUTPUT:
  RETVAL

BOOT:
 s_ca_pkg = gv_stashpv(s_ca, 0);
 if ( !s_ca_pkg )
    croak("Package %s does not exists", s_ca);