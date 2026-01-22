// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "elfio/elfio.hpp"
#include "cudacoredump.h"

#include "ppport.h"
#include "../elf.inc"
#include "bm_search.h"

void my_warn(const char * pat, ...) {
 va_list args;
 va_start(args, pat);
 vwarn(pat, &args);
}

static unsigned char get_host_encoding(void)
{
 static const int tmp = 1;
 if ( 1 == *reinterpret_cast<const char*>( &tmp ) ) return ELFIO::ELFDATA2LSB;
 return ELFIO::ELFDATA2MSB;
}

static unsigned char s_host_encoding = 0;

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

struct IElfSyms {
 IElf *e;
 ELFIO::symbol_section_accessor ssa;
 ~IElfSyms() {
   e->release();
 }
 IElfSyms(IElf *_e, ELFIO::section *s):
  e(_e),
  ssa(*_e->rdr, s)
 { e->add_ref(); }
};

struct IElfDyns {
 IElf *e;
 ELFIO::dynamic_section_accessor dsa;
 ~IElfDyns() {
   e->release();
 }
 IElfDyns(IElf *_e, ELFIO::section *s):
  e(_e),
  dsa(*_e->rdr, s)
 { e->add_ref(); }
};

struct IElfRels {
 IElf *e;
 ELFIO::relocation_section_accessor rsa;
 ~IElfRels() {
   e->release();
 }
 IElfRels(IElf *_e, ELFIO::section *s):
  e(_e),
  rsa(*_e->rdr, s)
 { e->add_ref(); }
};

struct IElfNotes {
 IElf *e;
 ELFIO::note_section_accessor nsa;
 ~IElfNotes() {
   e->release();
 }
 IElfNotes(IElf *_e, ELFIO::section *s):
  e(_e),
  nsa(*_e->rdr, s)
 { e->add_ref(); }
};

struct IElfVersyms {
 IElf *e;
 ELFIO::versym_r_section_accessor vsa;
 ~IElfVersyms() {
   e->release();
 }
 IElfVersyms(IElf *_e, ELFIO::section *s):
  e(_e),
  vsa(*_e->rdr, s)
 { e->add_ref(); }
};

struct IElfModinfo {
 IElf *e;
 ELFIO::const_modinfo_section_accessor msa;
 ~IElfModinfo() {
   e->release();
 }
 IElfModinfo(IElf *_e, ELFIO::section *s):
  e(_e),
  msa(s)
 { e->add_ref(); }
};

static int elf_magic_free(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        IElf *e = (IElf *)mg->mg_ptr;
        e->release();
        mg->mg_ptr= NULL;
    }
    return 0; // ignored anyway
}

template <typename T>
static int xxx_magic_free(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        T *e = (T *)mg->mg_ptr;
        if ( e ) delete e;
        mg->mg_ptr= NULL;
    }
    return 0; // ignored anyway
}

static const char *s_seciter = "Elf::Reader::SecIterator",
 *s_segiter = "Elf::Reader::SegIterator",
 *s_symbols = "Elf::Reader::SymIterator",
 *s_dynamics = "Elf::Reader::DynIterator",
 *s_relocs = "Elf::Reader::RelIterator",
 *s_notes = "Elf::Reader::NotesIterator",
 *s_versyms = "Elf::Reader::VersymsIterator",
 *s_modinfo = "Elf::Reader::ModinfoIterator"
;

// see https://github.com/Perl/perl5/blob/blead/mg.c
static U32
secs_magic_sizepack(pTHX_ SV *sv, MAGIC *mg)
{
  U32 res = 0;
  if (mg->mg_ptr) {
    IElf *e = (IElf *)mg->mg_ptr;
    res = e->rdr->sections.size() - 1;
  }
  return res;
}

static U32
segs_magic_sizepack(pTHX_ SV *sv, MAGIC *mg)
{
  U32 res = 0;
  if (mg->mg_ptr) {
    IElf *e = (IElf *)mg->mg_ptr;
    res = e->rdr->segments.size() - 1;
  }
  return res;
}

static U32
syms_magic_sizepack(pTHX_ SV *sv, MAGIC *mg)
{
  U32 res = 0;
  if (mg->mg_ptr) {
    IElfSyms *e = (IElfSyms *)mg->mg_ptr;
    res = e->ssa.get_symbols_num() - 1;
  }
  return res;
}

static U32
dyn_magic_sizepack(pTHX_ SV *sv, MAGIC *mg)
{
  U32 res = 0;
  if (mg->mg_ptr) {
    IElfDyns *e = (IElfDyns *)mg->mg_ptr;
    res = e->dsa.get_entries_num() - 1;
  }
  return res;
}

static U32
rel_magic_sizepack(pTHX_ SV *sv, MAGIC *mg)
{
  U32 res = 0;
  if (mg->mg_ptr) {
    IElfRels *e = (IElfRels *)mg->mg_ptr;
    res = e->rsa.get_entries_num() - 1;
  }
  return res;
}

static U32
notes_magic_sizepack(pTHX_ SV *sv, MAGIC *mg)
{
  U32 res = 0;
  if (mg->mg_ptr) {
    IElfNotes *e = (IElfNotes *)mg->mg_ptr;
    res = e->nsa.get_notes_num() - 1;
  }
  return res;
}

static U32
versyms_magic_sizepack(pTHX_ SV *sv, MAGIC *mg)
{
  U32 res = 0;
  if (mg->mg_ptr) {
    IElfVersyms *e = (IElfVersyms *)mg->mg_ptr;
    res = e->vsa.get_entries_num() - 1;
  }
  return res;
}

static U32
modinfo_magic_sizepack(pTHX_ SV *sv, MAGIC *mg)
{
  U32 res = 0;
  if (mg->mg_ptr) {
    IElfModinfo *e = (IElfModinfo *)mg->mg_ptr;
    res = e->msa.get_attribute_num() - 1;
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

// magic table for Elf::Reader::SymIterator
static MGVTBL Elf_magic_sym = {
        0, /* get */
        0, /* write */
        syms_magic_sizepack, /* length */
        0, /* clear */
        xxx_magic_free<IElfSyms>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

// magic table for Elf::Reader::DynIterator
static MGVTBL Elf_magic_dyn = {
        0, /* get */
        0, /* write */
        dyn_magic_sizepack, /* length */
        0, /* clear */
        xxx_magic_free<IElfDyns>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

// magic table for Elf::Reader::RelIterator
static MGVTBL Elf_magic_rel = {
        0, /* get */
        0, /* write */
        rel_magic_sizepack, /* length */
        0, /* clear */
        xxx_magic_free<IElfRels>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

// magic table for Elf::Reader::NotesIterator
static MGVTBL Elf_magic_notes = {
        0, /* get */
        0, /* write */
        notes_magic_sizepack, /* length */
        0, /* clear */
        xxx_magic_free<IElfNotes>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

// magic table for Elf::Reader::VersymsIterator
static MGVTBL Elf_magic_versyms = {
        0, /* get */
        0, /* write */
        versyms_magic_sizepack, /* length */
        0, /* clear */
        xxx_magic_free<IElfVersyms>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

// magic table for Elf::Reader::ModinfoIterator
static MGVTBL Elf_magic_modinfo = {
        0, /* get */
        0, /* write */
        modinfo_magic_sizepack, /* length */
        0, /* clear */
        xxx_magic_free<IElfModinfo>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

IElf *extract(SV *sv)
{
  return Elf_get_magic<IElf>(sv, 1, &Elf_magic_vt);
}

const ELFIO::section *find_section(IElf *e, unsigned long addr)
{
  for ( auto *s: e->rdr->sections ) {
    if ( s->get_type() != ELFIO::SHT_PROGBITS ) continue;
    if ( addr >= s->get_address() && addr < s->get_address() + s->get_size() )
      return s;
  }
  return nullptr;
}

// get section by index, check if it has non-zero size
const ELFIO::section *get_nonzero(IElf *e, int s_idx) {
  auto n = e->rdr->sections.size();
  if ( s_idx >= n ) return nullptr;
  auto *s = e->rdr->sections[s_idx];
  if ( !s->get_size() ) return nullptr;
  return s;
}

// unlike find_section try to search any section including NOBITS (e.g. .bss)
const ELFIO::section *find_any(IElf *e, unsigned long addr)
{
  for ( auto *s: e->rdr->sections ) {
    auto a = s->get_address();
    if ( !a ) continue;
    if ( addr >= a && addr < a + s->get_size() )
      return s;
  }
  return nullptr;
}

static AV *fill_section(const ELFIO::section *s)
{
  auto name = s->get_name();
  AV *av = newAV();
  av_push(av, newSViv( s->get_index() ));
  av_push(av, newSVpv(name.c_str(), name.size()) );
  av_push(av, newSViv( s->get_type() ));
  av_push(av, newSViv( s->get_flags() ));
  av_push(av, newSViv( s->get_info() ));
  av_push(av, newSViv( s->get_link() ));
  av_push(av, newSViv( s->get_addr_align() ));
  av_push(av, newSViv( s->get_entry_size() ));
  av_push(av, newSVuv( s->get_address() ));
  av_push(av, newSViv( s->get_size() ));
  av_push(av, newSVuv( s->get_offset() ));
  return av;
}

IElf::~IElf()
{
  if ( rdr ) delete rdr;
}

static int s_rdr_id = 0;

#define ELF_TIE(vtab, what) \
  fake = newAV(); \
  objref = newRV_noinc((SV*)fake); \
  sv_bless(objref, pkg); \
  magic = sv_magicext((SV*)fake, NULL, PERL_MAGIC_tied, &vtab, (const char *)what, 0); \
  SvREADONLY_on((SV*)fake); \
  ST(0) = objref; \
  XSRETURN(1);

#define EXPORT_ENUM(x) newCONSTSUB(stash, #x, new_enum_dualvar(aTHX_ x, newSVpvs_share(#x)));
static SV * new_enum_dualvar(pTHX_ IV ival, SV *name) {
        SvUPGRADE(name, SVt_PVNV);
        SvIV_set(name, ival);
        SvIOK_on(name);
        SvREADONLY_on(name);
        return name;
}

// bm search
AV *bm_ascii(SV *pattern, const unsigned char *start, const unsigned char *end, ptrdiff_t diff)
{
  int cnt = 0;
  STRLEN len;
  const unsigned char *ptr = (const unsigned char *)SvPVbyte(pattern, len);
  if ( !ptr || !len ) return nullptr;
  AV *av = newAV();
  // make BM
  bm_search srch( ptr, len );
  auto curr = start;
  while ( curr < end )
  {
    auto fres = srch.search(curr, end - curr);
    if ( NULL == fres )
      break;
    cnt++;
    av_push(av, newSVuv( UV(curr - diff) ) );
    curr = fres + len;
  }
  // instead of returning empty AV return nullptr
  if ( !cnt ) {
    SvREFCNT_dec((SV *)av);
    return nullptr;
  }
  return av;
}

AV *bm_asciiz(SV *pattern, const unsigned char *start, const unsigned char *end, ptrdiff_t diff)
{
  int cnt = 0;
  STRLEN len;
  char *ptr = SvPVbyte(pattern, len);
  if ( !ptr || !len ) return nullptr;
  bm_search srch;
  // check if SV already has SV_HAS_TRAILING_NUL
  if ( SvFLAGS(pattern) & SV_HAS_TRAILING_NUL )
  {
    len++;
    srch.set((const unsigned char *)ptr, len);
  } else {
    std::string tmp(ptr, len);
    // append leading zero
    tmp.push_back(0); len++;
    srch.set((const unsigned char *)tmp.c_str(), len);
  }
  AV *av = newAV();
  // make BM
  auto curr = start;
  while ( curr < end )
  {
    auto fres = srch.search(curr, end - curr);
    if ( NULL == fres )
      break;
    cnt++;
    av_push(av, newSVuv( UV(curr - diff) ) );
    curr = fres + len;
  }
  // instead of returning empty AV return nullptr
  if ( !cnt ) {
    SvREFCNT_dec((SV *)av);
    return nullptr;
  }
  return av;
}

template <typename T, typename F>
static int patch_s(FILE *fp, unsigned long off, F &&func)
{
  T data;
  fseek(fp, off, SEEK_SET);
  if ( 1 != fread(&data, sizeof(T), 1, fp) ) return 0;
  if ( func(data) ) {
    fseek(fp, off, SEEK_SET);
    if ( 1 != fwrite(&data, sizeof(T), 1, fp) ) return 0;
  }
  return 1;
}

template <typename F>
static int patch_segm(struct IElf *e, int idx, F &&func)
{
  // open file
  FILE *fp = fopen(e->fname.c_str(), "r+b");
  if ( !fp ) {
    my_warn("patch_segm: cannot open ELF file %s\n", e->fname.c_str());
    return 0;
  }
  auto is32 = e->rdr->get_class() == ELFIO::ELFCLASS32;
  unsigned long off = e->rdr->get_segments_offset();
  off += idx * (is32 ? sizeof(ELFIO::Elf32_Phdr) : sizeof(ELFIO::Elf64_Phdr));
  int res = is32 ?
    patch_s<ELFIO::Elf32_Phdr>(fp, off, func) :
    patch_s<ELFIO::Elf64_Phdr>(fp, off, func);
  fclose(fp);
  return res;
}

template <typename F>
static int patch_sect(struct IElf *e, int idx, F &&func)
{
  // open file
  FILE *fp = fopen(e->fname.c_str(), "r+b");
  if ( !fp ) {
    my_warn("patch_sect: cannot open ELF file %s\n", e->fname.c_str());
    return 0;
  }
  auto is32 = e->rdr->get_class() == ELFIO::ELFCLASS32;
  unsigned long off = e->rdr->get_sections_offset();
  off += idx * (is32 ? sizeof(ELFIO::Elf32_Shdr) : sizeof(ELFIO::Elf64_Shdr));
  int res = is32 ?
    patch_s<ELFIO::Elf32_Shdr>(fp, off, func) :
    patch_s<ELFIO::Elf64_Shdr>(fp, off, func);
  fclose(fp);
  return res;
}

// nvidia core dumps
static bool is_ncore(struct IElf *e) {
  return e->rdr->get_type() == ELFIO::ET_CORE &&
   e->rdr->get_machine() == ELFIO::EM_CUDA;
}

static void marshal(const CudbgDeviceTableEntry *ptr, AV *av) {
  // 0 - devName, index into the string table
  av_push(av, newSVuv(ptr->devName));
  // 1 - devType, index into the string table
  av_push(av, newSVuv(ptr->devType));
  // 2 - smType, index into the string table
  av_push(av, newSVuv(ptr->smType));
  // 3 - devId
  av_push(av, newSVuv(ptr->devId));
  // 4 - pciBusId
  av_push(av, newSVuv(ptr->pciBusId));
  // 5 - pciDevId
  av_push(av, newSVuv(ptr->pciDevId));
  // 6 - numSMs
  av_push(av, newSVuv(ptr->numSMs));
  // 7 - numWarpsPerSM
  av_push(av, newSVuv(ptr->numWarpsPerSM));
  // 8 - numLanesPerWarp
  av_push(av, newSVuv(ptr->numLanesPerWarp));
  // 9 - numRegsPerLane
  av_push(av, newSVuv(ptr->numRegsPerLane));
  // 10 - numPredicatesPrLane
  av_push(av, newSVuv(ptr->numPredicatesPrLane));
  // 11 - smMajor
  av_push(av, newSVuv(ptr->smMajor));
  // 12 - smMinor
  av_push(av, newSVuv(ptr->smMinor));
  // 13 - instructionSize
  av_push(av, newSVuv(ptr->instructionSize));
  // 14 - status
  av_push(av, newSVuv(ptr->status));
  // 15 - numUniformRegsPrWarp
  av_push(av, newSVuv(ptr->numUniformRegsPrWarp));
  // 16 - numUniformPredicatesPrWarp
  av_push(av, newSVuv(ptr->numUniformPredicatesPrWarp));
}

static void marshal(const CudbgDeviceTableEntry_575 *ptr, AV *av) {
  marshal((CudbgDeviceTableEntry *)ptr, av);
  // 17 - numConvergenceBarriersPrWarp
  av_push(av, newSVuv(ptr->numConvergenceBarriersPrWarp));
}

static bool check_size(const ELFIO::section *s, size_t item_size, const char *pfx) {
  auto ss = s->get_size();
  if ( !ss ) return true;
  if ( ss < item_size ) {
    my_warn("bad %d section %s size for %s, lesser %X\n", s->get_index(), s->get_name().c_str(), pfx, item_size);
    return false;
  }
  auto num = ss / item_size;
  if ( num * item_size != ss ) {
    my_warn("bad %d section %s size alignment for %s\n", s->get_index(), s->get_name().c_str(), pfx);
    return false;
  }
  return true;
}

static bool check_mask_size(const ELFIO::section *s, size_t item_mask, const char *pfx) {
  auto ss = s->get_size();
  if ( !ss ) return true;
  if ( ss <= item_mask ) {
    my_warn("bad %d section %s size for %s, lesser %X\n", s->get_index(), s->get_name().c_str(), pfx, 1 + item_mask);
    return false;
  }
  if ( ss & item_mask ) {
    my_warn("bad %d section %s size alignment for %s\n", s->get_index(), s->get_name().c_str(), pfx);
    return false;
  }
  return true;
}

static void get_ncd(const int32_t *ptr, AV *av) {
  av_push(av, newSViv(*ptr));
}

static void get_ncd(const uint64_t *ptr, AV *av) {
  av_push(av, newSVuv(*ptr));
}

static void get_ncd(const CudbgBacktraceTableEntry *ptr, AV *av_cont) {
  AV *av = newAV();
  // 0 - virtualReturnAddress
  av_push(av, newSVuv(ptr->virtualReturnAddress));
  // 1 - level
  av_push(av, newSVuv(ptr->level));
  av_push(av_cont, newRV_noinc((SV*)av));
}

static void get_ncd(const CudbgConstBankTableEntry *ptr, AV *av_cont) {
  AV *av = newAV();
  // 0 - addr
  av_push(av, newSVuv(ptr->addr));
  // 1 - size
  av_push(av, newSVuv(ptr->size));
  // 2 - bankId
  av_push(av, newSVuv(ptr->bankId));
  av_push(av_cont, newRV_noinc((SV*)av));
}

static void get_ncd(const CudbgMetaDataEntry *ptr, AV *av_cont) {
  AV *av = newAV();
  // 0 - generatorName
  av_push(av, newSVuv(ptr->generatorName));
  // 1 - driverVersionMajor
  av_push(av, newSVuv(ptr->driverVersionMajor));
  // 2 - driverVersionMinor
  av_push(av, newSVuv(ptr->driverVersionMinor));
  // 3 - cudaDriverVersionMajor
  av_push(av, newSVuv(ptr->cudaDriverVersionMajor));
  // 4 - cudaDriverVersionMinor
  av_push(av, newSVuv(ptr->cudaDriverVersionMinor));
  // 5 - flags
  av_push(av, newSVuv(ptr->flags));
  // 6 - timestamp
  av_push(av, newSVuv(ptr->timestamp));
  av_push(av_cont, newRV_noinc((SV*)av));
}

template <typename T>
static void get_ncd(const T *ptr, AV *av_cont) {
  AV *av = newAV();
  marshal(ptr, av);
  av_push(av_cont, newRV_noinc((SV*)av));
}

template <typename T>
SV *read_ncd(struct IElf *e, unsigned int stype, int idx, const char *pfx) {
  if ( !is_ncore(e) ) return &PL_sv_undef;
  // check section
  auto n = e->rdr->sections.size();
  if ( idx >= n ) {
    my_warn("invalid section index %d for %s\n", idx, pfx);
    return &PL_sv_undef;
  }
  auto *s = e->rdr->sections[idx];
  if ( s->get_type() != stype ) return &PL_sv_undef;
  auto isize = sizeof(T);
  switch(isize) {
    case 4: if ( !check_mask_size(s, 3, pfx) ) return &PL_sv_undef;
     break;
    case 8: if ( !check_mask_size(s, 7, pfx) ) return &PL_sv_undef;
     break;
    case 16: if ( !check_mask_size(s, 15, pfx) ) return &PL_sv_undef;
     break;
    case 32: if ( !check_mask_size(s, 31, pfx) ) return &PL_sv_undef;
     break;
    case 64: if ( !check_mask_size(s, 63, pfx) ) return &PL_sv_undef;
     break;
    default: if ( !check_size(s, isize, pfx) ) return &PL_sv_undef;
  }
  AV *av = newAV();
  auto start = s->get_data();
  auto end = start + s->get_size();
  for ( const T *ptr = (const T *)start; ptr <(const T *)end; ++ptr ) {
    get_ncd(ptr, av);
  }
  return newRV_noinc((SV*)av);
}

const int DRV_VERSION = 535;

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
    e->fname = fname;
    e->needswap = rde->get_encoding() != s_host_encoding;
    magic = sv_magicext(msv, NULL, PERL_MAGIC_ext, &Elf_magic_vt, (const char*)e, 0);
#ifdef USE_ITHREADS
    magic->mg_flags |= MGf_DUP;
#endif
  }
  XSRETURN(1);

UV
save2fd(SV *self, IV section_idx, PerlIO *handle)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(self, 1, &Elf_magic_vt);
 CODE:
  if ( !handle || section_idx < 0 || section_idx >= e->rdr->sections.size() )
    RETVAL = 0;
  else {
    auto *s = e->rdr->sections[section_idx];
    if ( !s ) {
      my_warn("save2fd - no section with index %d", section_idx);
      RETVAL = 0;
    } else {
      auto size = s->get_size();
      if ( !size ) RETVAL = 0;
      else RETVAL = PerlIO_write(handle, s->get_data(), size);
    }
  }
 OUTPUT:
  RETVAL

IV
patch_sec_flag(SV *self, IV section_idx, UV flag)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(self, 1, &Elf_magic_vt);
 CODE:
  if ( section_idx < 0 || section_idx >= e->rdr->sections.size() )
    RETVAL = 0;
  else {
   auto patch_flag = [&,flag](auto &p) -> bool {
    if ( e->needswap ) {
      const auto& convertor = e->rdr->get_convertor();
      p.sh_flags = convertor(flag);
    } else
      p.sh_flags = flag;
    return 1;
   };
   RETVAL = patch_sect(e, section_idx, patch_flag);
  }
 OUTPUT:
  RETVAL

IV
patch_seg_flag(SV *self, IV section_idx, UV flag)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(self, 1, &Elf_magic_vt);
 CODE:
  if ( section_idx < 0 || section_idx >= e->rdr->segments.size() )
    RETVAL = 0;
  else {
   auto patch_flag = [&,flag](auto &p) -> bool {
    if ( e->needswap ) {
      const auto& convertor = e->rdr->get_convertor();
      p.p_flags = convertor(flag);
    } else
      p.p_flags = flag;
    return 1;
   };
   RETVAL = patch_segm(e, section_idx, patch_flag);
  }
 OUTPUT:
  RETVAL


IV
patch_sec_type(SV *self, IV section_idx, UV flag)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(self, 1, &Elf_magic_vt);
 CODE:
  if ( section_idx < 0 || section_idx >= e->rdr->sections.size() )
    RETVAL = 0;
  else {
   auto patch_type = [&,flag](auto &p) -> bool {
    if ( e->needswap ) {
      const auto& convertor = e->rdr->get_convertor();
      p.sh_type = convertor(flag);
    } else
      p.sh_type = flag;
    return 1;
   };
   RETVAL = patch_sect(e, section_idx, patch_type);
  }
 OUTPUT:
  RETVAL

IV
patch_seg_type(SV *self, IV section_idx, UV flag)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(self, 1, &Elf_magic_vt);
 CODE:
  if ( section_idx < 0 || section_idx >= e->rdr->segments.size() )
    RETVAL = 0;
  else {
   auto patch_type = [&,flag](auto &p) -> bool {
    if ( e->needswap ) {
      const auto& convertor = e->rdr->get_convertor();
      p.p_type = convertor(flag);
    } else
      p.p_type = flag;
    return 1;
   };
   RETVAL = patch_segm(e, section_idx, patch_type);
  }
 OUTPUT:
  RETVAL

void
bm_idx(SV *self, SV *pattern, IV section_idx)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(self, 1, &Elf_magic_vt);
   AV *res = nullptr;
 PPCODE:
  auto *s = e->rdr->sections[section_idx];
  if ( !s ) {
    my_warn("bm_idx - no section with index %d", section_idx);
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  if ( s->get_type() != ELFIO::SHT_PROGBITS ) {
    my_warn("bm_idx - section with index %d has no content", section_idx);
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  auto start = s->get_data();
  auto end = start + s->get_size();
  res = bm_ascii(pattern, (const unsigned char *)start, (const unsigned char *)end, ptrdiff_t(start - s->get_address()));
  if ( !res )
   ST(0) = &PL_sv_undef;
  else
   ST(0) = newRV_noinc((SV*)res);
  XSRETURN(1);

void
bmz_idx(SV *self, SV *pattern, IV section_idx)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(self, 1, &Elf_magic_vt);
   AV *res = nullptr;
 PPCODE:
  auto *s = e->rdr->sections[section_idx];
  if ( !s ) {
    my_warn("bmz_idx - no section with index %d", section_idx);
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  if ( s->get_type() != ELFIO::SHT_PROGBITS ) {
    my_warn("bmz_idx - section with index %d has no content", section_idx);
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  auto start = s->get_data();
  auto end = start + s->get_size();
  res = bm_asciiz(pattern, (const unsigned char *)start, (const unsigned char *)end, ptrdiff_t(start - s->get_address()));
  if ( !res )
   ST(0) = &PL_sv_undef;
  else
   ST(0) = newRV_noinc((SV*)res);
  XSRETURN(1);

void
bm_from(SV *self, SV *pattern, UV addr)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(self, 1, &Elf_magic_vt);
   AV *res = nullptr;
 PPCODE:
  auto *s = find_section(e, addr);
  if ( !s ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  auto start = s->get_data() + addr - s->get_address();
  auto end = s->get_data() + s->get_size();
  ptrdiff_t diff = ptrdiff_t(s->get_data() - s->get_address());
  res = bm_ascii(pattern, (const unsigned char *)start, (const unsigned char *)end, diff);
  if ( !res )
   ST(0) = &PL_sv_undef;
  else
   ST(0) = newRV_noinc((SV*)res);
  XSRETURN(1);

void
bmz_from(SV *self, SV *pattern, UV addr)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(self, 1, &Elf_magic_vt);
   AV *res = nullptr;
 PPCODE:
  auto *s = find_section(e, addr);
  if ( !s ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  auto start = s->get_data() + addr - s->get_address();
  auto end = s->get_data() + s->get_size();
  ptrdiff_t diff = ptrdiff_t(s->get_data() - s->get_address());
  res = bm_asciiz(pattern, (const unsigned char *)start, (const unsigned char *)end, diff);
  if ( !res )
   ST(0) = &PL_sv_undef;
  else
   ST(0) = newRV_noinc((SV*)res);
  XSRETURN(1);

void
in_elf(SV *self, UV addr)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(self, 1, &Elf_magic_vt);
 PPCODE:
   auto *s = find_any(e, addr);
   if ( s ) ST(0) = &PL_sv_yes;
   else     ST(0) = &PL_sv_no;
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
  e->add_ref();
  ELF_TIE(Elf_magic_sec, e);

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
  e->add_ref();
  ELF_TIE(Elf_magic_seg, e);

void
syms(SV *arg, int key)
 INIT:
  HV *pkg = NULL;
  AV *fake = NULL;
  struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
  IElfSyms *es = NULL;
  SV *objref= NULL;
  MAGIC* magic;
 PPCODE:
  if ( !(pkg = gv_stashpv(s_symbols, 0)) ) {
    croak("Package %s does not exists", s_symbols);
    XSRETURN(0);
  }
  // check that section with key exists
  if ( key >= e->rdr->sections.size() ) {
    croak("section with index %d does not exists", key);
    XSRETURN(0);
  }
  // and really has type SHT_SYMTAB or SHT_DYNSYM
  auto s = e->rdr->sections[key];
  if ( s->get_type() != ELFIO::SHT_SYMTAB &&
       s->get_type() != ELFIO::SHT_DYNSYM ) {
    croak("section with index %d is not symbol section", key);
    XSRETURN(0);
  }
  es = new IElfSyms(e, s);
  ELF_TIE(Elf_magic_sym, es);

void
rels(SV *arg, int key)
 INIT:
  HV *pkg = NULL;
  AV *fake = NULL;
  struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
  IElfRels *er = NULL;
  SV *objref= NULL;
  MAGIC* magic;
 PPCODE:
  if ( !(pkg = gv_stashpv(s_relocs, 0)) ) {
    croak("Package %s does not exists", s_relocs);
    XSRETURN(0);
  }
  // check that section with key exists
  if ( key >= e->rdr->sections.size() ) {
    croak("section with index %d does not exists", key);
    XSRETURN(0);
  }
  // and really has type SHT_REL or SHT_RELA
  auto s = e->rdr->sections[key];
  if ( s->get_type() != ELFIO::SHT_REL &&
       s->get_type() != ELFIO::SHT_RELA ) {
    croak("section with index %d is not reloc section", key);
    XSRETURN(0);
  }
  er = new IElfRels(e, s);
  ELF_TIE(Elf_magic_rel, er);

void
dyns(SV *arg, int key)
 INIT:
  HV *pkg = NULL;
  AV *fake = NULL;
  struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
  IElfDyns *ed = NULL;
  SV *objref= NULL;
  MAGIC* magic;
 PPCODE:
  if ( !(pkg = gv_stashpv(s_dynamics, 0)) ) {
    croak("Package %s does not exists", s_dynamics);
    XSRETURN(0);
  }
  // check that section with key exists
  if ( key >= e->rdr->sections.size() ) {
    croak("section with index %d does not exists", key);
    XSRETURN(0);
  }
  // and really has type SHT_DYNAMIC
  auto s = e->rdr->sections[key];
  if ( s->get_type() != ELFIO::SHT_DYNAMIC ) {
    croak("section with index %d is not dynamic section", key);
    XSRETURN(0);
  }
  ed = new IElfDyns(e, s);
  ELF_TIE(Elf_magic_dyn, ed);

void
notes(SV *arg, int key)
 INIT:
  HV *pkg = NULL;
  AV *fake = NULL;
  struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
  IElfNotes *en = NULL;
  SV *objref= NULL;
  MAGIC* magic;
 PPCODE:
  if ( !(pkg = gv_stashpv(s_notes, 0)) ) {
    croak("Package %s does not exists", s_notes);
    XSRETURN(0);
  }
  // check that section with key exists
  if ( key >= e->rdr->sections.size() ) {
    croak("section with index %d does not exists", key);
    XSRETURN(0);
  }
  // and really has type SHT_NOTE
  auto s = e->rdr->sections[key];
  if ( s->get_type() != ELFIO::SHT_NOTE ) {
    croak("section with index %d is not note section", key);
    XSRETURN(0);
  }
  en = new IElfNotes(e, s);
  ELF_TIE(Elf_magic_notes, en);

void
modinfo(SV *arg)
 INIT:
  HV *pkg = NULL;
  AV *fake = NULL;
  struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
  IElfModinfo *mi = NULL;
  SV *objref= NULL;
  MAGIC* magic;
  ELFIO::section *vs = NULL;
 PPCODE:
  if ( !(pkg = gv_stashpv(s_modinfo, 0)) ) {
    croak("Package %s does not exists", s_modinfo);
    XSRETURN(0);
  }
  // try to find section .modinfo
  // see details https://eng.libretexts.org/Under_Construction/Purgatory/Computer_Science_from_the_Bottom_Up_(Wienand)/0.35%3A_Extending_ELF_concepts
  for ( auto *s: e->rdr->sections ) {
    if ( ".modinfo" == s->get_name() ) {
      vs = s;
      break;
    }
  }
  if ( !vs ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  } else {
    mi = new IElfModinfo(e, vs);
    ELF_TIE(Elf_magic_modinfo, mi);
  }

void
versyms(SV *arg)
 INIT:
  HV *pkg = NULL;
  AV *fake = NULL;
  struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
  IElfVersyms *vn = NULL;
  SV *objref= NULL;
  MAGIC* magic;
  ELFIO::section *vs = NULL;
 PPCODE:
  if ( !(pkg = gv_stashpv(s_versyms, 0)) ) {
    croak("Package %s does not exists", s_versyms);
    XSRETURN(0);
  }
  // try to find section with type SHT_GNU_verneed
  // see details https://refspecs.linuxbase.org/LSB_3.1.1/LSB-Core-generic/LSB-Core-generic/symversion.html
  for ( auto *s: e->rdr->sections ) {
    if ( s->get_type() == ELFIO::SHT_GNU_verneed ) {
      vs = s;
      break;
    }
  }
  if ( !vs ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  } else {
    vn = new IElfVersyms(e, vs);
    ELF_TIE(Elf_magic_versyms, vn);
  }

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

void os_abi(SV *arg)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 PPCODE:
   ST(0)= sv_2mortal( newSVuv( e->rdr->get_os_abi() ) );
   XSRETURN(1);

void abi_version(SV *arg)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 PPCODE:
   ST(0)= sv_2mortal( newSVuv( e->rdr->get_abi_version() ) );
   XSRETURN(1);

void version(SV *arg)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 PPCODE:
   ST(0)= sv_2mortal( newSVuv( e->rdr->get_version() ) );
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

void find(SV *arg, unsigned long addr)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   auto *s = find_section(e, addr);
 PPCODE:
   if ( !s )
     ST(0) = &PL_sv_undef;
   else {
     AV *av = fill_section(s);
     mXPUSHs(newRV_noinc((SV*)av));
   }
   XSRETURN(1);

void asciiz(SV *arg, unsigned long addr)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   auto *s = find_section(e, addr);
   std::string res;
   const char *start, *end;
   char c;
 PPCODE:
   if ( !s )
     ST(0) = &PL_sv_undef;
   else {
     end = s->get_data() + s->get_size();
     start = s->get_data() + addr - s->get_address();
     while( start < end && (c = *start) ) {
       res.push_back(c); start++;
     }
     ST(0)= sv_2mortal( newSVpv(res.c_str(), res.size()) );
   }
   XSRETURN(1);

void byte(SV *arg, unsigned long addr)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   auto *s = find_section(e, addr);
 PPCODE:
   if ( !s )
     ST(0) = &PL_sv_undef;
   else {
     auto *body = s->get_data() + addr - s->get_address();
     // check if we have this byte
     if ( body + 1 > s->get_data() + s->get_size() )
       ST(0) = &PL_sv_undef;
     else
       ST(0)= sv_2mortal( newSVuv( *body ) );
   }
   XSRETURN(1);

void sbyte(SV *arg, int s_idx, unsigned long off)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   auto *s = get_nonzero(e, s_idx);
 PPCODE:
   if ( !s )
     ST(0) = &PL_sv_undef;
   else {
     auto *body = s->get_data() + off;
     if ( off + 1 > s->get_size() )
       ST(0) = &PL_sv_undef;
     else
       ST(0)= sv_2mortal( newSVuv( *body ) );
   }
   XSRETURN(1);


void word(SV *arg, unsigned long addr)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   auto *s = find_section(e, addr);
 PPCODE:
   if ( !s )
     ST(0) = &PL_sv_undef;
   else {
     auto *body = s->get_data() + addr - s->get_address();
     // check if we have size to read word
     if ( body + 2 > s->get_data() + s->get_size() )
       ST(0) = &PL_sv_undef;
     else {
       uint16_t val = *(uint16_t *)body;
       if ( e->needswap ) val = __builtin_bswap16(val);
       ST(0)= sv_2mortal( newSVuv( val ) );
     }
   }
   XSRETURN(1);

void sword(SV *arg, int s_idx, unsigned long off)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   auto *s = get_nonzero(e, s_idx);
 PPCODE:
   if ( !s )
     ST(0) = &PL_sv_undef;
   else {
     auto *body = s->get_data() + off;
     if ( off + 2 > s->get_size() )
       ST(0) = &PL_sv_undef;
     else {
       uint16_t val = *(uint16_t *)body;
       if ( e->needswap ) val = __builtin_bswap16(val);
       ST(0)= sv_2mortal( newSVuv( val ) );
     }
   }
   XSRETURN(1);

void dword(SV *arg, unsigned long addr)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   auto *s = find_section(e, addr);
 PPCODE:
   if ( !s )
     ST(0) = &PL_sv_undef;
   else {
     auto *body = s->get_data() + addr - s->get_address();
     // check if we have size to read dword
     if ( body + 4 > s->get_data() + s->get_size() )
       ST(0) = &PL_sv_undef;
     else {
       uint32_t val = *(uint32_t *)body;
       if ( e->needswap ) val = __builtin_bswap32(val);
       ST(0)= sv_2mortal( newSVuv( val ) );
     }
   }
   XSRETURN(1);

void sdword(SV *arg, int s_idx, unsigned long off)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   auto *s = get_nonzero(e, s_idx);
 PPCODE:
   if ( !s )
     ST(0) = &PL_sv_undef;
   else {
     auto *body = s->get_data() + off;
     if ( off + 4 > s->get_size() )
       ST(0) = &PL_sv_undef;
     else {
       uint32_t val = *(uint32_t *)body;
       if ( e->needswap ) val = __builtin_bswap32(val);
       ST(0)= sv_2mortal( newSVuv( val ) );
     }
   }
   XSRETURN(1);

void qword(SV *arg, unsigned long addr)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   auto *s = find_section(e, addr);
 PPCODE:
   if ( !s )
     ST(0) = &PL_sv_undef;
   else {
     auto *body = s->get_data() + addr - s->get_address();
     // check if we have size to read qword
     if ( body + 8 > s->get_data() + s->get_size() )
       ST(0) = &PL_sv_undef;
     else {
       uint64_t val = *(uint64_t *)body;
       if ( e->needswap ) val = __builtin_bswap64(val);
       ST(0)= sv_2mortal( newSVuv( val ) );
     }
   }
   XSRETURN(1);

void sqword(SV *arg, int s_idx, unsigned long off)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   auto *s = get_nonzero(e, s_idx);
 PPCODE:
   if ( !s )
     ST(0) = &PL_sv_undef;
   else {
     auto *body = s->get_data() + off;
     if ( off + 8 > s->get_size() )
       ST(0) = &PL_sv_undef;
     else {
       uint64_t val = *(uint64_t *)body;
       if ( e->needswap ) val = __builtin_bswap64(val);
       ST(0)= sv_2mortal( newSVuv( val ) );
     }
   }
   XSRETURN(1);


void readN(SV *arg, unsigned long addr, int size)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   auto *s = find_section(e, addr);
 PPCODE:
   if ( !s )
     ST(0) = &PL_sv_undef;
   else {
     auto *body = s->get_data() + addr - s->get_address();
     // check if we have size to read N bytes
     if ( body + size > s->get_data() + s->get_size() )
       ST(0) = &PL_sv_undef;
     else {
       AV *av = newAV();
       for ( int i = 0; i < size; ++i )
         av_push(av, newSViv( body[i] ) );
       mXPUSHs(newRV_noinc((SV*)av));
     }
   }
   XSRETURN(1);

void sreadN(SV *arg, int s_idx, unsigned long off, int size)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   auto *s = get_nonzero(e, s_idx);
 PPCODE:
   if ( !s )
     ST(0) = &PL_sv_undef;
   else {
     auto *body = s->get_data() + off;
     // check if we have size to read 
     if ( off + size > s->get_size() )
       ST(0) = &PL_sv_undef;
     else {
       AV *av = newAV();
       for ( int i = 0; i < size; ++i )
         av_push(av, newSViv( body[i] ) );
       mXPUSHs(newRV_noinc((SV*)av));
     }
   }
   XSRETURN(1);

SV *ncd_regs(SV *arg, int s_idx)
ALIAS:
  Elf::Reader::uregs = 1
  Elf::Reader::pred = 2
  Elf::Reader::upred = 3
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
   static const char *s_names[4] = {
     "regs", "uregs", "pred", "upred"
   };
   static const unsigned int s_types[4] = { CUDBG_SHT_DEV_REGS, CUDBG_SHT_DEV_UREGS, CUDBG_SHT_DEV_PRED, CUDBG_SHT_DEV_UPRED };
 CODE:
   RETVAL = read_ncd<int32_t>(e, s_types[ix], s_idx, s_names[ix]);
 OUTPUT:
   RETVAL

SV *ncd_mods(SV *arg, int s_idx)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 CODE:
   RETVAL = read_ncd<uint64_t>(e, CUDBG_SHT_MOD_TABLE, s_idx, "mods");
 OUTPUT:
   RETVAL

SV *ncd_meta(SV *arg, int s_idx)
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 CODE:
   RETVAL = read_ncd<CudbgMetaDataEntry>(e, CUDBG_SHT_META_DATA, s_idx, "meta");
 OUTPUT:
   RETVAL

SV *ncd_bt(SV *arg, int s_idx)
ALIAS:
  Elf::Reader::ncd_bts = 1
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 CODE:
   RETVAL = read_ncd<CudbgBacktraceTableEntry>(e, CUDBG_SHT_BT, s_idx, "backtrace");
 OUTPUT:
   RETVAL

SV *ncd_cb(SV *arg, int s_idx)
ALIAS:
  Elf::Reader::ncd_cbs = 1
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 CODE:
   RETVAL = read_ncd<CudbgConstBankTableEntry>(e, CUDBG_SHT_CB_TABLE, s_idx, "constBank");
 OUTPUT:
   RETVAL

SV *ncd_dev(SV *arg, int s_idx, int version = DRV_VERSION)
ALIAS:
  Elf::Reader::ncd_devs = 1
 INIT:
   struct IElf *e= Elf_get_magic<IElf>(arg, 1, &Elf_magic_vt);
 CODE:
   if ( version >= 575 )
    RETVAL = read_ncd<CudbgDeviceTableEntry_575>(e, CUDBG_SHT_DEV_TABLE, s_idx, "dev");
   else
    RETVAL = read_ncd<CudbgDeviceTableEntry>(e, CUDBG_SHT_DEV_TABLE, s_idx, "dev");
 OUTPUT:
   RETVAL

MODULE = Elf::Reader		PACKAGE = Elf::Reader::SecIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 PREINIT:
  U8 gimme = GIMME_V;
 INIT:
  auto *e = Elf_get_tmagic<IElf>(self, 1, &Elf_magic_sec);
 PPCODE:
  if ( key >= e->rdr->sections.size() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  } else {
    auto s = e->rdr->sections[key];
    auto name = s->get_name();
    // format of array for section:
    // 0 - index
    // 1 - name, string
    // 2 - type
    // 3 - flags
    // 4 - info
    // 5 - link
    // 6 - addr_align
    // 7 - entry_size
    // 8 - address, 64bit
    // 9 - size
    // 10 - offset, 64bit
    if ( gimme == G_ARRAY) {
      EXTEND(SP, 11);
      mPUSHi(s->get_index());
      mPUSHp(name.c_str(), name.size());
      mPUSHi(s->get_type());
      mPUSHi(s->get_flags());
      mPUSHi(s->get_info());
      mPUSHi(s->get_link());
      mPUSHi(s->get_addr_align());
      mPUSHi(s->get_entry_size());
      mPUSHu(s->get_address());
      mPUSHi(s->get_size());
      mPUSHu(s->get_offset());
    } else {
      AV *av = fill_section(s);
      mXPUSHs(newRV_noinc((SV*)av));
      XSRETURN(1);
    }
  }


MODULE = Elf::Reader		PACKAGE = Elf::Reader::SegIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 PREINIT:
  U8 gimme = GIMME_V;
 INIT:
  auto *e = Elf_get_tmagic<IElf>(self, 1, &Elf_magic_seg);
 PPCODE:
  if ( key >= e->rdr->segments.size() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  } else {
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
      mPUSHi(s->get_index());
      mPUSHi(s->get_type());
      mPUSHi(s->get_flags());
      mPUSHi(s->get_align());
      mPUSHu(s->get_virtual_address());
      mPUSHu(s->get_physical_address());
      mPUSHu(s->get_file_size());
      mPUSHu(s->get_memory_size());
      mPUSHu(s->get_offset());
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
      XSRETURN(1);
    }
  }

MODULE = Elf::Reader		PACKAGE = Elf::Reader::SymIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 PREINIT:
  U8 gimme = GIMME_V;
 INIT:
  auto *s = Elf_get_tmagic<IElfSyms>(self, 1, &Elf_magic_sym);
 PPCODE:
  if ( key >= s->ssa.get_symbols_num() )
    ST(0) = &PL_sv_undef;
  else {
    std::string name;
    ELFIO::Elf64_Addr    value   = 0;
    ELFIO::Elf_Xword     size    = 0;
    unsigned char bind    = 0;
    unsigned char type    = 0;
    ELFIO::Elf_Half      section = 0;
    unsigned char other   = 0;
    if ( !s->ssa.get_symbol(key, name, value, size, bind, type, section, other) ) {
      ST(0) = &PL_sv_undef;
      XSRETURN(1);
    } else {
      // format of array for symbols
      // 0 - name
      // 1 - value, 64bit
      // 2 - size
      // 3 - bind
      // 4 - type
      // 5 - section
      // 6 - other
      if ( gimme == G_ARRAY) {
        EXTEND(SP, 7);
        mPUSHp(name.c_str(), name.size());
        mPUSHu(value);
        mPUSHi(size);
        mPUSHi(bind);
        mPUSHi(type);
        mPUSHi(section);
        mPUSHi(other);
      } else {
        // return ref to array
        AV *av = newAV();
        mXPUSHs(newRV_noinc((SV*)av));
        av_push(av, newSVpv(name.c_str(), name.size()) );
        av_push(av, newSVuv(value));
        av_push(av, newSViv(size));
        av_push(av, newSViv(bind));
        av_push(av, newSViv(type));
        av_push(av, newSViv(section));
        av_push(av, newSViv(other));
        XSRETURN(1);
      }
    }
  }


MODULE = Elf::Reader		PACKAGE = Elf::Reader::DynIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 PREINIT:
  U8 gimme = GIMME_V;
 INIT:
  auto *s = Elf_get_tmagic<IElfDyns>(self, 1, &Elf_magic_dyn);
 PPCODE:
  if ( key >= s->dsa.get_entries_num() )
    ST(0) = &PL_sv_undef;
  else {
    std::string name;
    ELFIO::Elf_Xword tag = 0,
      value = 0;
    if ( !s->dsa.get_entry(key, tag, value, name) ) {
      ST(0) = &PL_sv_undef;
      XSRETURN(1);
    } else {
      // format of array
      // 0 - name
      // 1 - tag
      // 2 - value
      if ( gimme == G_ARRAY) {
        EXTEND(SP, 3);
        mPUSHp(name.c_str(), name.size());
        mPUSHi(tag);
        mPUSHi(value);
      } else {
        // return ref to array
        AV *av = newAV();
        mXPUSHs(newRV_noinc((SV*)av));
        av_push(av, newSVpv(name.c_str(), name.size()) );
        av_push(av, newSViv(tag));
        av_push(av, newSViv(value));
        XSRETURN(1);
      }
    }
  }

MODULE = Elf::Reader		PACKAGE = Elf::Reader::RelIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 PREINIT:
  U8 gimme = GIMME_V;
 INIT:
  auto *s = Elf_get_tmagic<IElfRels>(self, 1, &Elf_magic_rel);
 PPCODE:
  if ( key >= s->rsa.get_entries_num() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  } else {
    // format of array for relocs
    // 0 - offset, 64bit
    // 1 - symbol
    // 2 - type
    // 3 - addend
    ELFIO::Elf64_Addr offset = 0;
    ELFIO::Elf_Word sym_idx = 0;
    unsigned rtype = 0;
    ELFIO::Elf_Sxword add = 0;
    if ( !s->rsa.get_entry(key, offset, sym_idx, rtype, add) )
      ST(0) = &PL_sv_undef;
    else {
      if ( gimme == G_ARRAY) {
        EXTEND(SP, 4);
        mPUSHu(offset);
        mPUSHi(sym_idx);
        mPUSHu(rtype);
        mPUSHi(add);
      } else {
        // return ref to array
        AV *av = newAV();
        mXPUSHs(newRV_noinc((SV*)av));
        av_push(av, newSVuv(offset));
        av_push(av, newSViv(sym_idx));
        av_push(av, newSVuv(rtype));
        av_push(av, newSViv(add));
        XSRETURN(1);
      }
    }
  }

MODULE = Elf::Reader		PACKAGE = Elf::Reader::NotesIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 PREINIT:
  U8 gimme = GIMME_V;
 INIT:
  auto *s = Elf_get_tmagic<IElfNotes>(self, 1, &Elf_magic_notes);
 PPCODE:
  if ( key >= s->nsa.get_notes_num() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  } else {
    // format of array for notes
    // 0 - name, string
    // 1 - type
    // 2 - desclen
    // 3 - desc, addr
    // 4 - desc, blob
    ELFIO::Elf_Word type = 0,
      desclen = 0;
    std::string name;
    void*       desc = nullptr;
    if ( !s->nsa.get_note( key, type, name, desc, desclen) )
     ST(0) = &PL_sv_undef;
    else {
      if ( gimme == G_ARRAY) {
        EXTEND(SP, 5);
        mPUSHp(name.c_str(), name.size());
        mPUSHu(type);
        mPUSHi(desclen);
        mPUSHu((unsigned long)desc);
        mPUSHp((const char *)desc, desclen);
      } else {
        // return ref to array
        AV *av = newAV();
        mXPUSHs(newRV_noinc((SV*)av));
        av_push(av, newSVpv(name.c_str(), name.size()) );
        av_push(av, newSVuv(type));
        av_push(av, newSVuv(desclen));
        av_push(av, newSVuv((unsigned long)desc));
        av_push(av, newSVpv((const char *)desc, desclen));
        XSRETURN(1);
      }
    }
  }

MODULE = Elf::Reader		PACKAGE = Elf::Reader::VersymsIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 PREINIT:
  U8 gimme = GIMME_V;
 INIT:
  auto *s = Elf_get_tmagic<IElfVersyms>(self, 1, &Elf_magic_versyms);
 PPCODE:
  if ( key >= s->vsa.get_entries_num() )
    ST(0) = &PL_sv_undef;
  else {
    // format of array for versyms
    // 0 - version
    // 1 - filename, string
    // 2 - hash
    // 3 - flags
    // 4 - other
    // 5 - dep_name, string
    ELFIO::Elf_Word hash = 0;
    ELFIO::Elf_Half version = 0,
     flags = 0,
     other = 0;
    std::string name, dep_name;
    if ( !s->vsa.get_entry( key, version, name, hash, flags, other, dep_name ) ) {
     ST(0) = &PL_sv_undef;
     XSRETURN(1);
    } else {
      if ( gimme == G_ARRAY) {
        EXTEND(SP, 6);
        mPUSHu(version);
        mPUSHp(name.c_str(), name.size());
        mPUSHu(hash);
        mPUSHu(flags);
        mPUSHu(other);
        mPUSHp(dep_name.c_str(), dep_name.size());
      } else {
        // return ref to array
        AV *av = newAV();
        mXPUSHs(newRV_noinc((SV*)av));
        av_push(av, newSVuv(version));
        av_push(av, newSVpv(name.c_str(), name.size()) );
        av_push(av, newSVuv(hash));
        av_push(av, newSVuv(flags));
        av_push(av, newSVuv(other));
        av_push(av, newSVpv(dep_name.c_str(), dep_name.size()));
        XSRETURN(1);
      }
    }
  }

MODULE = Elf::Reader		PACKAGE = Elf::Reader::ModinfoIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 PREINIT:
  U8 gimme = GIMME_V;
 INIT:
  auto *s = Elf_get_tmagic<IElfModinfo>(self, 1, &Elf_magic_modinfo);
 PPCODE:
  if ( key >= s->msa.get_attribute_num() )
    ST(0) = &PL_sv_undef;
  else {
    std::string name, value;
    if ( !s->msa.get_attribute(key, name, value) ) {
      ST(0) = &PL_sv_undef;
      XSRETURN(1);
    } else {
      if ( gimme == G_ARRAY) {
        EXTEND(SP, 2);
        mPUSHp(name.c_str(), name.size());
        mPUSHp(value.c_str(), value.size());
      } else {
        AV *av = newAV();
        mXPUSHs(newRV_noinc((SV*)av));
        av_push(av, newSVpv(name.c_str(), name.size()) );
        av_push(av, newSVpv(value.c_str(), value.size()) );
        XSRETURN(1);
      }
    }
  }

BOOT:
 s_host_encoding = get_host_encoding();
 HV *stash= gv_stashpvn("Elf::Reader", 11, 1);
 EXPORT_ENUM(CUDBG_SHT_MANAGED_MEM)
 EXPORT_ENUM(CUDBG_SHT_GLOBAL_MEM)
 EXPORT_ENUM(CUDBG_SHT_LOCAL_MEM)
 EXPORT_ENUM(CUDBG_SHT_SHARED_MEM)
 EXPORT_ENUM(CUDBG_SHT_DEV_REGS)
 EXPORT_ENUM(CUDBG_SHT_ELF_IMG)
 EXPORT_ENUM(CUDBG_SHT_RELF_IMG)
 EXPORT_ENUM(CUDBG_SHT_BT)
 EXPORT_ENUM(CUDBG_SHT_DEV_TABLE)
 EXPORT_ENUM(CUDBG_SHT_CTX_TABLE)
 EXPORT_ENUM(CUDBG_SHT_SM_TABLE)
 EXPORT_ENUM(CUDBG_SHT_GRID_TABLE)
 EXPORT_ENUM(CUDBG_SHT_CTA_TABLE)
 EXPORT_ENUM(CUDBG_SHT_WP_TABLE)
 EXPORT_ENUM(CUDBG_SHT_LN_TABLE)
 EXPORT_ENUM(CUDBG_SHT_MOD_TABLE)
 EXPORT_ENUM(CUDBG_SHT_DEV_PRED)
 EXPORT_ENUM(CUDBG_SHT_PARAM_MEM)
 EXPORT_ENUM(CUDBG_SHT_DEV_UREGS)
 EXPORT_ENUM(CUDBG_SHT_DEV_UPRED)
 EXPORT_ENUM(CUDBG_SHT_CB_TABLE)
 EXPORT_ENUM(CUDBG_SHT_META_DATA)
 EXPORT_ENUM(CUDBG_SHT_CBU_BAR)
 EXPORT_ENUM(DRV_VERSION)
