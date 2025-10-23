// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "elfio/elfio.hpp"
#include "../elf.inc"
#include <vector>
#include <list>
#include <unordered_set>

// const bank params
struct cb_param {
  int ordinal;
  unsigned short offset;
  size_t size;
};

struct CAttr {
  ptrdiff_t offset;
  size_t len = 0;
  char attr, form;
};

// indirect branch items
struct ib_item {
  ptrdiff_t offset;
  uint32_t addr;
  std::list<uint32_t> labels;
};

static SV *new_enum_dualvar(pTHX_ IV ival, SV *name) {
        SvUPGRADE(name, SVt_PVNV);
        SvIV_set(name, ival);
        SvIOK_on(name);
        SvREADONLY_on(name);
        return name;
}

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

static std::unordered_map<int, const char *> s_ei = {
#include "eiattrs.inc"
};

struct CAttrs {
  CAttrs(IElf *e) {
    m_e = e;
    e->add_ref();
  }
  ~CAttrs() {
    if ( m_e ) m_e->release();
    if ( m_wf ) fclose(m_wf);
  }
  IElf *m_e = nullptr;
  int s_idx = -1;
  std::vector<CAttr> m_attrs;
  // externals
  std::vector<uint32_t> m_extrs;
  // cb
  std::vector<cb_param> params;
  unsigned short cb_size = 0;
  unsigned short cb_offset = 0;
  // indirect branches
  std::unordered_map<uint32_t, ib_item> indirect_branches;
  // write file handle
  FILE *m_wf = nullptr;
  // methods
  void clear() {
    m_extrs.clear();
    m_attrs.clear();
    params.clear();
    indirect_branches.clear();
    cb_size = cb_offset = 0;
  }
  int read(int idx);
  SV *fetch(const CAttr &a, int idx);
  SV *fetch(int idx);
  SV *fetch_cb(int idx);
  SV *fetch_extrs() const {
    if ( m_extrs.empty() ) return &PL_sv_undef;
    AV *av = newAV();
    std::for_each(m_extrs.cbegin(), m_extrs.cend(), [av](uint32_t v) { av_push(av, newSVuv(v)); });
    return newRV_noinc((SV*)av);
  }
  SV *get_value(int idx);
  SV *addr_list(const CAttr &a);
  SV *ibt_hash();
  SV *try_attr(int t_idx);
  SV *try_rels(int t_idx, ELFIO::Elf_Word);
  SV *patch_ibt(std::unordered_map<uint32_t, ib_item>::iterator &, uint32_t v);
  SV *patch_ibt(std::unordered_map<uint32_t, ib_item>::iterator &, AV *);
  bool patch_rels(int rs_idx, int r_idx, int reloc_type);
  bool patch_rel_off(int rs_idx, int r_idx, UV reloc_offset);
  bool patch_rela_off(int rs_idx, int r_idx, UV reloc_offset, IV add_delta);
  bool patch_relsif(int rs_idx, int r_idx, int reloc_type, int old_type);
  template <typename T>
  bool _patch_rel_off(int rs_idx, int r_idx, UV rel_offset);
  template <typename T>
  bool _patch_rela_off(int rs_idx, int r_idx, UV rel_offset, IV);
  template <typename T>
  bool _patch_rels(int rs_idx, int r_idx, int reloc_type);
  template <typename T>
  bool _patch_relsif(int rs_idx, int r_idx, int reloc_type, int old_type);
  inline void add_cparam(ELFIO::Elf_Word ordinal, unsigned short off, unsigned short size) {
    params.push_back( { ordinal, off, size } );
  }
  template <typename T>
  T read(const CAttr &a) {
   auto sec = m_e->rdr->sections[s_idx];
   const char *data = sec->get_data() + 2 + a.offset;
   if ( 4 == a.form ) data += 2;
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
    if ( 4 == a.form ) off += 2;
    fseek(m_wf, off, SEEK_SET);
    return 1 == fwrite(&value, sizeof(value), 1, m_wf);
  }
  template <typename T>
  // ElfXX_Rel(a), section index, entry index
  bool write_rel(const T &value, int rs_idx, int r_idx) {
    if ( !check_wf() ) return false;
    auto sec = m_e->rdr->sections[rs_idx];
    auto off = sec->get_offset() + sizeof(T) * r_idx;
    fseek(m_wf, off, SEEK_SET);
    return 1 == fwrite(&value, sizeof(value), 1, m_wf);
  }
  template <typename T>
  bool read_rel(T &value, int rs_idx, int r_idx) {
    if ( !check_wf() ) return false;
    auto sec = m_e->rdr->sections[rs_idx];
    size_t off = sizeof(T) * r_idx;
    if ( off + sizeof(T) > sec->get_size() ) return false;
    fseek(m_wf, off + sec->get_offset(), SEEK_SET);
    return 1 == fread(&value, sizeof(T), 1, m_wf);
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
  SV *patch_addr(int idx, int a_idx, U32 off);
  SV *patch_addr(int idx, AV *);
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
  hv_store(hv, "form", 4, newSViv(a.form), 0);
  hv_store(hv, "off", 3, newSVuv(a.offset), 0);
  hv_store(hv, "len", 3, newSVuv(a.len), 0);
  return newRV_noinc((SV*)hv);
}

SV *CAttrs::ibt_hash() {
  if ( indirect_branches.empty() ) return &PL_sv_undef;
  HV *hv = newHV();
  for ( std::unordered_map<uint32_t, ib_item>::const_iterator bi = indirect_branches.begin(); bi != indirect_branches.end(); ++bi )
  {
    if ( bi->second.labels.empty() )
     hv_store_ent(hv, newSVuv(bi->first), &PL_sv_undef, 0);
    else if ( 1 == bi->second.labels.size() ) {
     auto first = bi->second.labels.front();
     hv_store_ent(hv, newSVuv(bi->first), newSVuv(first), 0);
    } else {
      // value - ref to array
      AV *av = newAV();
      for ( auto al: bi->second.labels )
        av_push(av, newSVuv(al));
      hv_store_ent(hv, newSVuv(bi->first), newRV_noinc((SV*)av), 0);
    }
  }
  return newRV_noinc((SV*)hv);
}

SV *CAttrs::patch_ibt(std::unordered_map<uint32_t, ib_item>::iterator &iter, uint32_t v) {
  if ( iter->second.labels.front() == v ) return &PL_sv_yes; // nothing to patch - values are the same
  if ( !check_wf() ) return &PL_sv_undef;
  auto sec = m_e->rdr->sections[s_idx];
  auto off = sec->get_offset() + 0xc + iter->second.offset;
  fseek(m_wf, off, SEEK_SET);
  if ( 1 != fwrite(&v, sizeof(v), 1, m_wf) ) return &PL_sv_no;
  iter->second.labels.front() = v;
  return &PL_sv_yes;
}

SV *CAttrs::patch_ibt(std::unordered_map<uint32_t, ib_item>::iterator &iter, AV *av) {
  std::vector<uint32_t> tmp;
  for (int i = 0; i <= av_len(av); i++) {
    SV** elem = av_fetch(av, i, 0);
    U32 ov = SvUV(*elem);
    tmp.push_back(ov);
  }
  std::sort(tmp.begin(), tmp.end());
  if ( !check_wf() ) return &PL_sv_undef;
  auto sec = m_e->rdr->sections[s_idx];
  auto off = sec->get_offset() + 0xc + iter->second.offset;
  fseek(m_wf, off, SEEK_SET);
  auto asize = sizeof(uint32_t) * tmp.size();
  if ( 1 != fwrite(tmp.data(), asize, 1, m_wf) ) return &PL_sv_no;
  // copy tmp to labels
  iter->second.labels.clear();
  std::copy(tmp.begin(), tmp.end(), std::back_inserter(iter->second.labels));
  return &PL_sv_yes;
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

SV *CAttrs::patch_addr(int idx, int a_idx, U32 off_v)
{
  // check idx
  if ( idx < 0 || idx >= m_attrs.size() ) {
    my_warn("patch_addr: invalid index %d\n", idx);
    return &PL_sv_undef;
  }
  auto &attr = m_attrs[idx];
  if ( !is_addr_list(attr.attr) ) {
    my_warn("patch_addr: bad index %d\n", idx);
    return &PL_sv_no;
  }
  // check offset
  auto addr_size = attr.len / 4;
  if ( a_idx < 0 || a_idx >= addr_size ) {
    my_warn("patch_addr: bad a_index %d\n", idx);
    return &PL_sv_no;
  }
  // lets patch
  if ( !check_wf() ) return &PL_sv_undef;
  auto sec = m_e->rdr->sections[s_idx];
  auto off = sec->get_offset() + 4 + attr.offset + sizeof(U32) * a_idx;
  fseek(m_wf, off, SEEK_SET);
  return 1 == fwrite(&off_v, sizeof(off_v), 1, m_wf) ? &PL_sv_yes : &PL_sv_no;
}

SV *CAttrs::patch_addr(int idx, AV *av)
{
  // check idx
  if ( idx < 0 || idx >= m_attrs.size() ) {
    my_warn("patch_alist: invalid index %d\n", idx);
    return &PL_sv_undef;
  }
  auto &attr = m_attrs[idx];
  if ( !is_addr_list(attr.attr) ) {
    my_warn("patch_alist: bad index %d\n", idx);
    return &PL_sv_no;
  }
  auto addr_size = attr.len / 4;
  if ( addr_size != av_len(av) + 1 ) {
    my_warn("patch_alist: size %d array len %d\n", addr_size, av_len(av));
    return &PL_sv_no;
  }
  // lets patch
  if ( !check_wf() ) return &PL_sv_undef;
  auto sec = m_e->rdr->sections[s_idx];
  auto off = sec->get_offset() + 4 + attr.offset;
  fseek(m_wf, off, SEEK_SET);
  for (int i = 0; i <= av_len(av); i++) {
    SV** elem = av_fetch(av, i, 0);
    U32 ov = SvIV(*elem);
    if ( 1 == fwrite(&ov, sizeof(ov), 1, m_wf) ) {
      my_warn("patch_alist: cannot write item %d\n", i);
      return &PL_sv_no;
    }
  }
  return &PL_sv_yes;
}

SV *CAttrs::get_value(int idx) {
  // check idx
  if ( idx < 0 || idx >= m_attrs.size() ) {
    my_warn("get_value: invalid index %d\n", idx);
    return &PL_sv_undef;
  }
  auto &attr = m_attrs[idx];
  if ( !attr.len ) return &PL_sv_yes;
  if ( attr.attr == 0x34 ) // EIATTR_INDIRECT_BRANCH_TARGETS
    return ibt_hash();
  if ( attr.attr == 0xf ) // EIATTR_EXTERNS
    return fetch_extrs();
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

int CAttrs::read(int idx)
{
  clear();
  s_idx = idx;
  if ( s_idx < 0 ) return 0;
  if ( !check_section(m_e->rdr, idx) ) return 0;
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
        m_attrs.push_back( { data - start, 0, attr, format });
        data += 2;
        // check align
        if ( (data - start) & 0x3 ) data += 4 - ((data - start) & 0x3);
        break;
      case 2:
        m_attrs.push_back( { data - start, 1, attr, format });
        data += 3;
        // check align
        if ( (data - start) & 0x1 ) data++;
       break;
      case 3:
        m_attrs.push_back( { data - start, 2, attr, format });
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
        } else if ( attr == 0xf ) // EIATTR_EXTERNS
        {
          auto end = kp + a_len;
          for ( auto curr = kp; curr < end; ) {
            m_extrs.push_back(*(uint32_t *)curr);
            curr += 4;
          }
        } else if ( attr == 0x34 ) // EIATTR_INDIRECT_BRANCH_TARGETS
        {
          if ( a_len < 0xc ) my_warn("invalid INDIRECT_BRANCH_TARGETS size %X\n", a_len);
          else { // ripped from CElf::parse_branch_targets
            auto end = kp + a_len;
            for ( auto curr = kp; curr < end; ) {
              ib_item ib;
              // record like
              // 0 - 32bit address
              // 4 & 6 - unknown 16bit words
              // 8 - 32bit count
              // 0xc - ... - list of 32bit labels, count items
              // so minimal size should be 0xc
              if ( end - kp < 0xc ) break;
              ib.addr = *(uint32_t *)(curr);
              uint32_t cnt = *(uint32_t *)(curr + 0x8);
              ib.offset = curr - start;
              curr += 0xc;
              // read labels
              for ( uint32_t i = 0; i < cnt && curr < end; i++, curr += 4 ) ib.labels.push_back( *(uint32_t *)(curr) );
              // insert this item
              indirect_branches[ib.addr] = std::move(ib);
            }
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
          m_attrs.push_back( { data - start, a_len, attr, format } );
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

// try to find attribs section linked with t_idx (info == t_idx)
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

static void patch_info(ELFIO::Elf_Word *r, int reloc_type)
{
  auto sym = ELF32_R_SYM(*r);
  *r = ELF32_R_INFO(sym, reloc_type);
}

static void patch_info(ELFIO::Elf_Xword *r, int reloc_type)
{
  auto sym = ELF64_R_SYM(*r);
  *r = ELF64_R_INFO(sym, reloc_type);
}

template <typename T>
bool CAttrs::_patch_rels(int rs_idx, int r_idx, int reloc_type)
{
  T rel;
  if ( !read_rel(rel, rs_idx, r_idx) ) return false;
  // patch info
  patch_info(&rel.r_info, reloc_type);
  return write_rel(rel, rs_idx, r_idx);
}

bool CAttrs::patch_rels(int rs_idx, int r_idx, int reloc_type)
{
  // check if rs_idx is valid section
  if ( rs_idx < 0 || rs_idx >= m_e->rdr->sections.size() ) return false;
  auto s = m_e->rdr->sections[rs_idx];
  auto st = s->get_type();
  if ( st == ELFIO::SHT_REL ) {
    if ( m_e->rdr->get_class() == ELFIO::ELFCLASS32 )
      return _patch_rels<ELFIO::Elf32_Rel>(rs_idx, r_idx, reloc_type);
    else
      return _patch_rels<ELFIO::Elf64_Rel>(rs_idx, r_idx, reloc_type);
  } else if ( st == ELFIO::SHT_RELA ) {
    if ( m_e->rdr->get_class() == ELFIO::ELFCLASS32 )
      return _patch_rels<ELFIO::Elf32_Rela>(rs_idx, r_idx, reloc_type);
    else
      return _patch_rels<ELFIO::Elf64_Rela>(rs_idx, r_idx, reloc_type);
  } else
    return false;
}

template <typename T>
bool CAttrs::_patch_rel_off(int rs_idx, int r_idx, UV reloc_offset)
{
  T rel;
  if ( !read_rel(rel, rs_idx, r_idx) ) return false;
  rel.r_offset = reloc_offset;
  return write_rel(rel, rs_idx, r_idx);
}

bool CAttrs::patch_rel_off(int rs_idx, int r_idx, UV reloc_offset)
{
  // check if rs_idx is valid section
  if ( rs_idx < 0 || rs_idx >= m_e->rdr->sections.size() ) return false;
  auto s = m_e->rdr->sections[rs_idx];
  auto st = s->get_type();
  if ( st == ELFIO::SHT_REL ) {
    if ( m_e->rdr->get_class() == ELFIO::ELFCLASS32 )
      return _patch_rel_off<ELFIO::Elf32_Rel>(rs_idx, r_idx, reloc_offset);
    else
      return _patch_rel_off<ELFIO::Elf64_Rel>(rs_idx, r_idx, reloc_offset);
  }
  return false;
}

template <typename T>
bool CAttrs::_patch_rela_off(int rs_idx, int r_idx, UV reloc_offset, IV delta)
{
  T rel;
  if ( !read_rel(rel, rs_idx, r_idx) ) return false;
  rel.r_offset = reloc_offset;
  if ( delta )
    rel.r_addend += delta;
  return write_rel(rel, rs_idx, r_idx);
}

bool CAttrs::patch_rela_off(int rs_idx, int r_idx, UV reloc_offset, IV delta)
{
  // check if rs_idx is valid section
  if ( rs_idx < 0 || rs_idx >= m_e->rdr->sections.size() ) return false;
  auto s = m_e->rdr->sections[rs_idx];
  auto st = s->get_type();
  if ( st == ELFIO::SHT_RELA ) {
    if ( m_e->rdr->get_class() == ELFIO::ELFCLASS32 )
      return _patch_rela_off<ELFIO::Elf32_Rela>(rs_idx, r_idx, reloc_offset, delta);
    else
      return _patch_rela_off<ELFIO::Elf64_Rela>(rs_idx, r_idx, reloc_offset, delta);
  }
  return false;
}

static bool patch_infoif(ELFIO::Elf_Word *r, int reloc_type, int old_reloc)
{
  auto old = ELF32_R_TYPE(*r);
  if ( old != old_reloc ) return false;
  auto sym = ELF32_R_SYM(*r);
  *r = ELF32_R_INFO(sym, reloc_type);
  return true;
}

static bool patch_infoif(ELFIO::Elf_Xword *r, int reloc_type, int old_reloc)
{
  auto old = ELF64_R_TYPE(*r);
  if ( old != old_reloc ) return false;
  auto sym = ELF64_R_SYM(*r);
  *r = ELF64_R_INFO(sym, reloc_type);
  return true;
}

template <typename T>
bool CAttrs::_patch_relsif(int rs_idx, int r_idx, int reloc_type, int old_reloc)
{
  T rel;
  if ( !read_rel(rel, rs_idx, r_idx) ) return false;
  // patch info
  if ( !patch_infoif(&rel.r_info, reloc_type, old_reloc) ) return false;
  return write_rel(rel, rs_idx, r_idx);
}

bool CAttrs::patch_relsif(int rs_idx, int r_idx, int reloc_type, int old_type) {
  // check if rs_idx is valid section
  if ( rs_idx < 0 || rs_idx >= m_e->rdr->sections.size() ) return false;
  auto s = m_e->rdr->sections[rs_idx];
  auto st = s->get_type();
  if ( st == ELFIO::SHT_REL ) {
    if ( m_e->rdr->get_class() == ELFIO::ELFCLASS32 )
      return _patch_relsif<ELFIO::Elf32_Rel>(rs_idx, r_idx, reloc_type, old_type);
    else
      return _patch_relsif<ELFIO::Elf64_Rel>(rs_idx, r_idx, reloc_type, old_type);
  } else if ( st == ELFIO::SHT_RELA ) {
    if ( m_e->rdr->get_class() == ELFIO::ELFCLASS32 )
      return _patch_relsif<ELFIO::Elf32_Rela>(rs_idx, r_idx, reloc_type, old_type);
    else
      return _patch_relsif<ELFIO::Elf64_Rela>(rs_idx, r_idx, reloc_type, old_type);
  } else
    return false;
}

// try to find rel section linked with t_idx (info == t_idx)
SV *CAttrs::try_rels(int t_idx, ELFIO::Elf_Word what)
{
  for ( int idx = 0; idx < m_e->rdr->sections.size(); idx++ )
  {
    auto s = m_e->rdr->sections[idx];
    auto st = s->get_type();
    if ( st != what ) continue;
    if ( s->get_info() == t_idx ) return newSViv(idx);
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
    my_warn("Cubin::Attrs: my_type %d\n", SvTYPE(sv));
    return 0;
  }
  return (U32)d->m_attrs.size()-1;
}

#define RET_RES   if ( res.empty() ) { \
    if ( gimme == G_ARRAY) { XSRETURN(0); \
    } else { mXPUSHs(&PL_sv_undef); XSRETURN(1); } \
  } else { \
    if ( gimme == G_ARRAY) { \
      EXTEND(SP, res.size()); \
      for ( auto &p: res ) \
        mPUSHs( d->fetch(*p.first, p.second) ); \
    } else { \
      AV *av = newAV(); \
      for ( auto &p: res ) \
        av_push(av, d->fetch(*p.first, p.second) ); \
      mXPUSHs(newRV_noinc((SV*)av)); \
      XSRETURN(1); \
    } \
  }


MODULE = Cubin::Attrs		PACKAGE = Cubin::Attrs

void
new(obj_or_pkg, SV *elsv)
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
  // make new CAttrs
  auto res = new CAttrs(e);
  DWARF_TIE(ca_magic_vt, s_ca_pkg, res)

SV *
try(SV *self, int t_idx)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->try_attr(t_idx);
 OUTPUT:
  RETVAL

SV *
try_rel(SV *self, int t_idx)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->try_rels(t_idx, ELFIO::SHT_REL);
 OUTPUT:
  RETVAL

SV *
try_rela(SV *self, int t_idx)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->try_rels(t_idx, ELFIO::SHT_RELA);
 OUTPUT:
  RETVAL

SV *
patch_ft(SV *self, int s_idx, int r_idx, int reloc_type)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->patch_rels(s_idx, r_idx, reloc_type) ? &PL_sv_yes : &PL_sv_no;
 OUTPUT:
  RETVAL

SV *
patch_ftif(SV *self, int s_idx, int r_idx, int reloc_type, int old_reloc)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->patch_relsif(s_idx, r_idx, reloc_type, old_reloc) ? &PL_sv_yes : &PL_sv_no;
 OUTPUT:
  RETVAL

SV *
patch_foff(SV *self, int s_idx, int r_idx, UV new_offset)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->patch_rel_off(s_idx, r_idx, new_offset) ? &PL_sv_yes : &PL_sv_no;
 OUTPUT:
  RETVAL

SV *
patch_foffa(SV *self, int s_idx, int r_idx, UV new_offset, IV add_delta = 0)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->patch_rela_off(s_idx, r_idx, new_offset, add_delta) ? &PL_sv_yes : &PL_sv_no;
 OUTPUT:
  RETVAL

int
read(SV *self, int idx)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->read(idx);
 OUTPUT:
  RETVAL

SV *
link(SV *self)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  if ( -1 == d->s_idx || !d->m_e ) RETVAL = &PL_sv_undef;
  else {
    auto s = d->m_e->rdr->sections[d->s_idx];
    RETVAL = newSViv(s->get_info());
  }
 OUTPUT:
  RETVAL

SV *
attr_name(SV *self, int v)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
  std::unordered_map<int, const char *>::const_iterator nv;
 CODE:
  nv = s_ei.find(v);
  if ( nv == s_ei.end() )
    RETVAL = &PL_sv_undef;
  else
    RETVAL = newSVpv(nv->second, strlen(nv->second));
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

SV *
patch_addr(SV *self, int idx, int a_idx, U32 v)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
 CODE:
  RETVAL = d->patch_addr(idx, a_idx, v);
 OUTPUT:
  RETVAL

SV *
patch_alist(SV *self, int idx, SV *ar)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
  AV* array;
 CODE:
  // Check if it's a valid array reference
  if (!SvROK(ar) || SvTYPE(SvRV(ar)) != SVt_PVAV) {
    croak("patch_alist: expected an ARRAY reference");
  }
  array = (AV*) SvRV(ar); // Dereference the SV to get the AV*
  RETVAL = d->patch_addr(idx, array);
 OUTPUT:
  RETVAL

SV *
patch_ibt(SV *self, UV addr, SV *what)
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
  std::unordered_map<uint32_t, ib_item>::iterator ibt_iter;
 CODE:
  // check if we have ibt
  if ( d->indirect_branches.empty() )
    RETVAL = &PL_sv_undef;
  else {
    ibt_iter = d->indirect_branches.find(addr);
    if ( ibt_iter == d->indirect_branches.end() )
     RETVAL = &PL_sv_no;
    else {
      auto lsize = ibt_iter->second.labels.size();
      if ( !lsize ) RETVAL = &PL_sv_no;
      else if ( lsize == 1 ) {
        if ( !SvIOK(what) )
          croak("patch_ibt: second arg should be integer");
        else
          RETVAL = d->patch_ibt(ibt_iter, SvUV(what));
      } else {
        // Check if it's a valid array reference
        if (!SvROK(what) || SvTYPE(SvRV(what)) != SVt_PVAV)
          croak("patch_ibt: second arg should be ARRAY reference");
        else { // check if we have the same size
          AV* array = (AV*)SvRV(what);
          if ( 1 + av_len(array) != lsize )
          {
            my_warn("bad array len %d, should be %ld items", 1 + av_len(array), lsize);
            RETVAL = &PL_sv_no;
          } else
           RETVAL = d->patch_ibt(ibt_iter, array);
        }
      }
    }
  }
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
grep_list(SV *self,SV *ar)
  U8 gimme = GIMME_V;
 INIT:
  auto *d = magic_tied<CAttrs>(self, 1, &ca_magic_vt);
  AV* array;
  std::unordered_set<int> keys;
  std::vector<std::pair<const CAttr*, int> > res;
 CODE:
  // Check if it's a valid array reference
  if (!SvROK(ar) || SvTYPE(SvRV(ar)) != SVt_PVAV) {
    croak("patch_alist: expected an ARRAY reference");
  }
  array = (AV*) SvRV(ar); // Dereference the SV to get the AV*
  // fill keys
  for (int i = 0; i <= av_len(array); i++) {
    SV** elem = av_fetch(array, i, 0);
    keys.insert(SvIV(*elem));
  }
  // check if list if non-empty
  if ( !keys.empty() ) {
    for ( size_t i = 0; i < d->m_attrs.size(); i++ ) {
      if ( keys.end() != keys.find(d->m_attrs[i].attr) ) {
// my_warn("add %d type %x\n", int(i), d->m_attrs[i].attr);
        res.push_back( { &d->m_attrs[i], int(i) });
      }
    }
  }
  // tail is the same as in regular grep below
  RET_RES

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
  RET_RES

BOOT:
 s_ca_pkg = gv_stashpv(s_ca, 0);
 if ( !s_ca_pkg )
    croak("Package %s does not exists", s_ca);
 // export enums from eiattrs.inc
 HV *stash = gv_stashpvn(s_ca, 12, 1);
 for ( auto en: s_ei ) {
   auto name = en.second;
   auto len = strlen(name);
   auto sv = newSVpvn_share(name, len, 0);
   newCONSTSUB(stash, name, new_enum_dualvar(aTHX_ en.first, sv));
   // and second - without EIATTR_ prefix
   if ( len > 7 ) {
     name += 7;
     len -= 7;
     sv = newSVpvn_share(name, len, 0);
     newCONSTSUB(stash, name, new_enum_dualvar(aTHX_ en.first, sv));
   }
 }
