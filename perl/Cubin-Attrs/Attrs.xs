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
  ptrdiff_t offset;
  size_t len = 0;
  char attr;
};

static int is_addr_list(char attr) {
  switch(attr) {
    case 0x28: // EIATTR_COOP_GROUP_INSTR_OFFSETS
    case 0x1c: // EIATTR_EXIT_INSTR_OFFSETS
    case 0x1d: // EIATTR_S2RCTAID_INSTR_OFFSETS
    case 0x25: // EIATTR_LD_CACHEMOD_INSTR_OFFSETS
    case 0x31: // EIATTR_INT_WARP_WIDE_INSTR_OFFSETS
    case 0x39: // EIATTR_MBARRIER_INSTR_OFFSETS
    case 0x47: // EIATTR_SW_WAR_MEMBAR_SYS_INSTR_OFFSETS
      return 1;
    default: return 0;
  }
}

struct CAttrs {
  CAttrs(IElf *e, int idx) {
    m_e = e;
    s_idx = idx;
  }
  ~CAttrs() {
    if ( m_e ) m_e->release();
    if ( m_wf ) fclose(m_wf);
  }
  IElf *m_e = nullptr;
  int s_idx = -1;
  std::vector<CAttr> m_attrs;
  // cb
  std::vector<cb_param> params;
  unsigned short cb_size = 0;
  unsigned short cb_offset = 0;
  // write file handle
  FILE *m_wf = nullptr;
  // methods
  int read();
  SV *fetch(const CAttr &a, int idx);
  SV *fetch(int idx);
  SV *fetch_cb(int idx);
  SV *get_value(int idx);
  SV *addr_list(const CAttr &a);
  SV *try_attr(int t_idx);
  inline void add_cparam(ELFIO::Elf_Word ordinal, unsigned short off, unsigned short size) {
    params.push_back( { ordinal, size, off } );
  }
  template <typename T>
  T read(const CAttr &a) {
   auto sec = m_e->rdr->sections[s_idx];
   const char *data = sec->get_data() + 2 + a.offset;
   return *(T *)data;
  }
  // write-patch methods
  inline bool check_wf() {
    if ( m_wf ) return true;
    m_wf = fopen(m_e->fname.c_str(), "r+b");
    if ( !m_wf ) {
      my_warn("Cannot open %d, errno %d (%s)\n", m_e->fname.c_str(), errno, strerror(errno));
      return false;
    }
    return true;
  }
  template <typename T>
  bool write(const CAttr &a, T value) {
    if ( !check_wf() ) return false;
    auto sec = m_e->rdr->sections[s_idx];
    auto off = sec->get_offset() + 2 + a.offset;
    fseek(m_wf, off, SEEK_SET);
    return 1 == fwrite(&value, sizeof(value), 1, m_wf);
  }
  SV *patch(int idx, unsigned long v) {
    // check idx
    if ( idx < 0 || idx >= m_attrs.size() ) {
      my_warn("patch: invalid index %d\n", idx);
      return &PL_sv_undef;
    }
    auto &attr = m_attrs[idx];
    if ( !attr.len ) return &PL_sv_yes;
    if ( is_addr_list(attr.attr) ) return &PL_sv_no;
    if ( 1 == attr.len && write(attr, (unsigned char)v) ) return &PL_sv_yes;
    if ( attr.len == 2 && write(attr, (unsigned short)v) ) return &PL_sv_yes;
    if ( attr.len == 4 && write(attr, (uint32_t)v) ) return &PL_sv_yes;
    if ( attr.len == 8 && sizeof(unsigned long) == 8 && write(attr, v) ) return &PL_sv_yes;
    return &PL_sv_no;
  }
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

SV *CAttrs::fetch(const CAttr &a, int idx)
{
  HV *hv = newHV();
  hv_store(hv, "id", 2, newSViv(idx), 0);
  hv_store(hv, "attr", 4, newSViv(a.attr), 0);
  hv_store(hv, "off", 3, newSVuv(a.offset), 0);
  hv_store(hv, "len", 3, newSVuv(a.len), 0);
  return newRV_noinc((SV*)hv);
}

SV *CAttrs::fetch(int idx) {
  // check idx
  if ( idx < 0 || idx >= m_attrs.size() ) {
    my_warn("invalid index %d\n", idx);
    return &PL_sv_undef;
  }
  // create and fill HV
  return fetch(m_attrs[idx], idx);
}


SV *CAttrs::addr_list(const CAttr &a)
{
  if ( !a.len ) return &PL_sv_undef;
  auto sec = m_e->rdr->sections[s_idx];
  const char *data = sec->get_data() + 4 + a.offset;
  AV *av = newAV();
  for ( const char *bcurr = data; data + a.len - bcurr >= 0x4; bcurr += 0x4 )
  {
    uint32_t addr = *(uint32_t *)(bcurr);
    av_push(av, newSVuv(addr));
  }
  return newRV_noinc((SV*)av);
}

SV *CAttrs::get_value(int idx) {
  // check idx
  if ( idx < 0 || idx >= m_attrs.size() ) {
    my_warn("invalid index %d\n", idx);
    return &PL_sv_undef;
  }
  auto &attr = m_attrs[idx];
  if ( !attr.len ) return &PL_sv_yes;
  if ( is_addr_list(attr.attr) ) return addr_list(attr);
  if ( 1 == attr.len )
    return newSViv( read<unsigned char>(attr) );
  if ( attr.len == 2 )
    return newSViv( read<unsigned short>(attr) );
  if ( attr.len == 4 )
    return newSVuv( read<uint32_t>(attr) );
  if ( attr.len == 8 && sizeof(unsigned long) == 8 )
    return newSVuv( read<unsigned long>(attr) );
  return &PL_sv_undef;
}

int CAttrs::read()
{
  if ( s_idx < 0 ) return 0;
  auto sec = m_e->rdr->sections[s_idx];
  if ( sec->get_type() == ELFIO::SHT_NOBITS ) return 0;
  auto size = sec->get_size();
  if ( !size ) return 0;
  const char *data = sec->get_data();
  auto sidx = sec->get_info();
  const char *start, *end = data + size;
  start = data;
  while( data < end )
  {
    if ( end - data < 2 ) {
      my_warn("bad attrs data. section %d\n", s_idx);
      return 0;
    }
    char format = data[0];
    char attr = data[1];
    unsigned short a_len;
    const char *kp = nullptr;
    bool skip = false;
    switch (format)
    {
      case 1:
        m_attrs.push_back( { data - start, 0, attr });
        data += 2;
        // check align
        if ( (data - start) & 0x3 ) data += 4 - ((data - start) & 0x3);
        break;
      case 2:
        m_attrs.push_back( { data - start, 1, attr });
        data += 3;
        // check align
        if ( (data - start) & 0x1 ) data++;
       break;
      case 3:
        m_attrs.push_back( { data - start, 2, attr });
        data += 4;
       break;
     case 4:
       a_len = *(unsigned short *)(data + 2);
       kp = data + 4;
       if ( attr == 0xa ) { // EIATTR_PARAM_CBANK
          skip = true;
          if ( a_len != 8 ) my_warn("invalid PARAM_CBANK size %X\n", a_len);
          else {
            uint32_t sec_id = *(uint32_t *)kp;
            kp += 4;
            unsigned short off = *(unsigned short *)kp;
            kp += 2;
            unsigned short size = *(unsigned short *)kp;
            cb_size = size;
            cb_offset = off;
          }
        } else if ( attr == 0x17 ) // EIATTR_KPARAM_INFO
        {
          skip = true;
          // from https://github.com/VivekPanyam/cudaparsers/blob/main/src/cubin.rs
          if ( a_len != 0xc ) my_warn("invalid KPARAM_INFO size %X\n", a_len);
          else {
            kp += 4;
            unsigned short ord = *(unsigned short *)kp;
            kp += 2;
            unsigned short off = *(unsigned short *)kp;
            kp += 2;
            uint32_t tmp = *(uint32_t *)kp;
            unsigned space = (tmp >> 0x8) & 0xf;
            int is_cbank = ((tmp >> 0x10) & 2) == 0;
            uint32_t csize = (((tmp >> 0x10) & 0xffff) >> 2);
            if ( is_cbank ) add_cparam(ord, off, csize);
          }
        }
        if ( !skip )
          m_attrs.push_back( { data - start, a_len, attr } );
        data += 4 + a_len;
       break;
     default:
       my_warn("unknown format %d, section %d off %lX (%s)\n",
        format, s_idx, data - start, sec->get_name().c_str());
       return 0;
    }
  }
  return !m_attrs.empty();
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

SV *CAttrs::try_attr(int t_idx)
{
  for ( int idx = 0; idx < m_e->rdr->sections.size(); idx++ )
  {
    auto s = m_e->rdr->sections[idx];
    auto st = s->get_type();
    if ( st == ELFIO::SHT_NOBITS ) continue;
    if ( (st == 0x70000000) || (st == 0x70000083) ) {
      if ( s->get_info() == t_idx ) return newSViv(idx);
    }
  }
  return &PL_sv_undef;
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
    my_warn("Cubin::Attrs: my_len %d\n", SvTYPE(sv));
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
try(SV *self, int t_idx)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->try_attr(t_idx);
 OUTPUT:
  RETVAL

int
read(SV *self)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->read();
 OUTPUT:
  RETVAL

UV
count(SV *self)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->m_attrs.size();
 OUTPUT:
  RETVAL

SV *
FETCH(SV *self, int idx)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->fetch(idx);
 OUTPUT:
  RETVAL

SV *
param(SV *self, int idx)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->fetch_cb(idx);
 OUTPUT:
  RETVAL

SV *
value(SV *self, int idx)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->get_value(idx);
 OUTPUT:
  RETVAL

SV *
patch(SV *self, int idx, unsigned long v)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->patch(idx, v);
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

void
grep(SV *self, IV key)
  U8 gimme = GIMME_V;
 INIT:
  SV *str;
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 PPCODE:
  std::vector<std::pair<const CAttr*, int> > res;
  // filter
  for ( size_t i = 0; i < d->m_attrs.size(); i++ ) {
    if ( d->m_attrs[i].attr == key ) {
      res.push_back( { &d->m_attrs[i], int(i) });
    }
  }
  if ( res.empty() ) {
    if ( gimme == G_ARRAY) {
      XSRETURN(0);
    } else {
      mXPUSHs(&PL_sv_undef);
      XSRETURN(1);
    }
  } else {
    if ( gimme == G_ARRAY) {
      EXTEND(SP, res.size());
      for ( auto &p: res )
        mXPUSHs( d->fetch(*p.first, p.second) );
      XSRETURN(res.size());
    } else {
      AV *av = newAV();
      for ( auto &p: res )
        av_push(av, d->fetch(*p.first, p.second) );
      mXPUSHs(newRV_noinc((SV*)av));
      XSRETURN(1);
    }
  }

BOOT:
 s_ca_pkg = gv_stashpv(s_ca, 0);
 if ( !s_ca_pkg )
    croak("Package %s does not exists", s_ca);