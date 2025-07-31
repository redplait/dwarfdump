// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "elfio/elfio.hpp"
#include "../elf.inc"
#include <unordered_map>

void my_warn(const char * pat, ...) {
 va_list args;
 va_start(args, pat);
 vwarn(pat, &args);
}

// ripped from https://github.com/chei90/RemoteRendering/blob/master/inc/fatBinaryCtl.h
#define FATBINC_MAGIC   0x466243B1
typedef struct {
  int magic;
  int version;
  const unsigned long long* data;
  void *filename_or_fatbins;  /* version 1: offline filename,
                               * version 2: array of prelinked fatbins */
} __fatBinC_Wrapper_t;

#define FATBIN_CONTROL_SECTION_NAME     ".nvFatBinSegment"
/*
 * The section that contains the fatbin data itself
 * (put in separate section so easy to find)
 */
#define FATBIN_DATA_SECTION_NAME        ".nv_fatbin"

// from https://github.com/chei90/RemoteRendering/blob/master/inc/fatbinary.h
struct fatBinaryHeader
{
  unsigned int           magic;
  unsigned short         version;
  unsigned short         headerSize;
  unsigned long long int fatSize;
} __attribute__ ((aligned (8)));

#define FATBIN_MAGIC 0xBA55ED50

typedef enum {
  FATBIN_KIND_PTX      = 0x0001,
  FATBIN_KIND_ELF      = 0x0002,
  FATBIN_KIND_OLDCUBIN = 0x0004,
} fatBinaryCodeKind;

/* Flags */
#define FATBIN_FLAG_64BIT     0x0000000000000001LL
#define FATBIN_FLAG_DEBUG     0x0000000000000002LL
#define FATBIN_FLAG_CUDA      0x0000000000000004LL
#define FATBIN_FLAG_OPENCL    0x0000000000000008LL
#define FATBIN_FLAG_LINUX     0x0000000000000010LL
#define FATBIN_FLAG_MAC       0x0000000000000020LL
#define FATBIN_FLAG_WINDOWS   0x0000000000000040LL
#define FATBIN_FLAG_HOST_MASK 0x00000000000000f0LL
#define FATBIN_FLAG_OPT_MASK  0x0000000000000f00LL /* optimization level */
#define FATBIN_FLAG_COMPRESS  0x0000000000001000LL
#define FATBIN_FLAG_COMPRESS2 0x0000000000002000LL

struct  __attribute__((__packed__)) fat_text_header
{
    uint16_t kind;
    uint16_t unknown1;
    uint32_t header_size;
    uint64_t size;
    uint32_t compressed_size;       // Size of compressed data
    uint32_t unknown2;              // Address size for PTX?
    uint16_t minor;
    uint16_t major;
    uint32_t arch;
    uint32_t obj_name_offset;
    uint32_t obj_name_len;
    uint64_t flags;
    uint64_t zero;                  // Alignment for compression?
    uint64_t decompressed_size;
};

class CFatBin {
 public:
   CFatBin(IElf *_e, const char *fname) {
     m_e = _e;
     _e->add_ref();
     rdr_fname = fname;
   }
   ~CFatBin() {
     m_e->release();
   }
   size_t count() const {
     return m_map.size();
   }
   // returns non-zero when succeed
   int open();
   // extract file at index idx to file of
   int extract(int idx, const char *of);
   // try to replace file at index idx to file rf
   int try_replace(int idx, const char *rf);
   // perl specific methods
   SV *fetch(int idx);
   inline unsigned int ctrl_idx() const {
     return m_ctrl;
   }
   inline unsigned int fb_idx() const {
     return m_fb;
   }
 protected:
   typedef std::unordered_map<int, std::pair<ptrdiff_t, fat_text_header> > FBItems;
   FBItems m_map;
   int _extract(const FBItems::iterator &, const char *, FILE *);
   int _replace(ptrdiff_t, const fat_text_header&, FILE *);
   inline bool compressed(const fat_text_header &ft) const {
     return ft.flags & FATBIN_FLAG_COMPRESS || ft.flags & FATBIN_FLAG_COMPRESS2;
   }
   // from https://zhuanlan.zhihu.com/p/29424681490
   size_t decompress(const uint8_t *input, size_t input_size, uint8_t *output, size_t output_size);
   ELFIO::Elf_Half n_sec = 0, m_ctrl = 0, m_fb = 0;
   unsigned long fb_size;
   IElf *m_e;
   std::string rdr_fname;
};

size_t CFatBin::decompress(const uint8_t *input, size_t input_size, uint8_t *output, size_t output_size)
{
    size_t ipos = 0, opos = 0;
    uint64_t next_nclen;  // length of next non-compressed segment
    uint64_t next_clen;   // length of next compressed segment
    uint64_t back_offset; // negative offset where redudant data is located, relative to current opos

    while (ipos < input_size) {
        next_nclen = (input[ipos] & 0xf0) >> 4;
        next_clen = 4 + (input[ipos] & 0xf);
        if (next_nclen == 0xf) {
            do {
                next_nclen += input[++ipos];
            } while (input[ipos] == 0xff);
        }

        if (memcpy(output + opos, input + (++ipos), next_nclen) == NULL) {
            my_warn("Error copying data");
            return 0;
        }
#ifdef FATBIN_DECOMPRESS_DEBUG
        printf("%#04zx/%#04zx nocompress (len:%#zx):\n", opos, ipos, next_nclen);
        HexDump(stdout, output + opos, next_nclen);
#endif
        ipos += next_nclen;
        opos += next_nclen;
        if (ipos >= input_size || opos >= output_size) {
            break;
        }
        back_offset = input[ipos] + (input[ipos + 1] << 8);
        ipos += 2;
        if (next_clen == 0xf + 4) {
            do {
                next_clen += input[ipos++];
            } while (input[ipos - 1] == 0xff);
        }
#ifdef FATBIN_DECOMPRESS_DEBUG
        printf("%#04zx/%#04zx compress (decompressed len: %#zx, back_offset %#zx):\n", opos, ipos, next_clen,
               back_offset);
#endif
        if (next_clen <= back_offset) {
            if (memcpy(output + opos, output + opos - back_offset, next_clen) == NULL) {
                my_warn("Error copying data");
                return 0;
            }
        } else {
            if (memcpy(output + opos, output + opos - back_offset, back_offset) == NULL) {
                my_warn("Error copying data");
                return 0;
            }
            for (size_t i = back_offset; i < next_clen; i++) {
                output[opos + i] = output[opos + i - back_offset];
            }
        }
#ifdef FATBIN_DECOMPRESS_DEBUG
        HexDump(stdout, output + opos, next_clen);
#endif
        opos += next_clen;
    }
    return opos;
}

int CFatBin::open()
{
  // try to find control section
  n_sec = m_e->rdr->sections.size();
  for ( ELFIO::Elf_Half i = 0; i < n_sec; ++i ) {
    ELFIO::section *sec = m_e->rdr->sections[i];
    auto st = sec->get_type();
    if ( st == ELFIO::SHT_NOBITS || !sec->get_size() ) continue;
    auto sn = sec->get_name();
    if ( sn == FATBIN_CONTROL_SECTION_NAME ) {
      m_ctrl = i;
      if ( sec->get_size() < sizeof(__fatBinC_Wrapper_t) ) {
        my_warn("control section is too small: %lX\n", sec->get_size());
        return 0;
      }
      break;
    }
  }
  if ( !m_ctrl ) {
    my_warn("cannot find control section\n");
    return 0;
  }
  ELFIO::section *sec = m_e->rdr->sections[m_ctrl];
  auto fbc = (const __fatBinC_Wrapper_t *)sec->get_data();
  if ( fbc->magic != FATBINC_MAGIC ) {
    my_warn("invalid ctrl section magic %X\n", fbc->magic);
    return 0;
  }
  // try to find section at address fbc->data
  for ( ELFIO::Elf_Half i = 0; i < n_sec; ++i ) {
    ELFIO::section *sec = m_e->rdr->sections[i];
    auto st = sec->get_type();
    if ( st == ELFIO::SHT_NOBITS || !sec->get_size() ) continue;
    auto sa = sec->get_address();
    if ( sa == (ELFIO::Elf64_Addr)fbc->data ||
         ((sa < (ELFIO::Elf64_Addr)fbc->data) && (sa + sec->get_size() > (ELFIO::Elf64_Addr)fbc->data))
       )
    {
      m_fb = i;
      if ( sec->get_size() < sizeof(fatBinaryHeader) ) {
        my_warn("fatbim section is too small: %lX\n", sec->get_size());
        return 0;
      }
      break;
    }
  }
  if ( !m_fb ) {
    my_warn("cannot find fatbin section\n");
    return 0;
  }
  // read fatBinaryHeader
  sec = m_e->rdr->sections[m_fb];
  fb_size = sec->get_size();
  auto data = sec->get_data();
  auto dend = data + fb_size;
  auto fb_hdr = (const fatBinaryHeader *)data;
  int idx = 0;
  while ( (const char *)fb_hdr < dend ) {
    if ( fb_hdr->magic != FATBIN_MAGIC ) {
      my_warn("unknown magic %X\n", fb_hdr->magic);
      return 0;
    }
    // printf("version %d hdr_size %X fat_size %lX\n", fb_hdr->version, fb_hdr->headerSize, fb_hdr->fatSize);
    if ( fb_hdr->version != 1 || fb_hdr->headerSize != sizeof(fatBinaryHeader) ) {
      my_warn("don't know sich fatbin header\n");
      return 0;
    }
    // try to parse adjacent fat_text_header
    if ( fb_size < fb_hdr->headerSize + sizeof(fat_text_header) ) {
      my_warn("too short fatbin section, len %lX\n", fb_size);
      return 0;
    }
    const char *next_fb = (const char *)(fb_hdr + 1) + fb_hdr->fatSize;
    const fat_text_header *fth = (const fat_text_header *)((const char *)fb_hdr + fb_hdr->headerSize);
    while ( (const char *)(fth + 1) < next_fb )
    {
      if ( fth->header_size != sizeof(fat_text_header) ) break;
      // keep all fth data in single line for easy grepping
      //printf("[%d] kind %X flag %lX header_size %X size %lX arch %X major %d minor %d",
      //    idx, fth->kind, fth->flags, fth->header_size,
      //    fth->size, fth->arch, fth->major, fth->minor
      //);
      m_map[idx] = { (const char *)fth - data, *fth };
      idx++;
      fth = (const fat_text_header *)((const char *)(fth + 1) + fth->size);
    }
    fb_hdr = (const fatBinaryHeader *)next_fb;
  }
  return 1;
}

int CFatBin::try_replace(int idx, const char *rf)
{
  // check idx
  auto ii = m_map.find(idx);
  if ( ii == m_map.end() ) {
    my_warn("invalid index %d\n", idx);
    return 0;
  }
  if ( compressed(ii->second.second) ) {
    my_warn("file with index %d compressed\n", idx);
    return 0;
  }
  // calculate offset
  auto sec = m_e->rdr->sections[m_fb];
  ptrdiff_t off = sec->get_offset() + ii->second.first + ii->second.second.header_size;
  // open rf
  FILE *fp = fopen(rf, "rb");
  if ( !fp ) {
    my_warn("cannot open %s, errno %d (%s)\n", rf, errno, strerror(errno));
    return 0;
  }
  // check file size
  struct stat st;
  if ( fstat(fileno(fp), &st) ) {
    my_warn("cannot fstat %s, errno %d (%s)\n", rf, errno, strerror(errno));
    fclose(fp);
    return 0;
  }
  if ( st.st_size != (long int)ii->second.second.size ) {
    my_warn("sizes mismatch: file %s has size %lX, in fat binary %lX \n", rf, st.st_size, ii->second.second.size );
    fclose(fp);
    return 0;
  }
  int res = _replace(off, ii->second.second, fp);
  fclose(fp);
  return res;
}

int CFatBin::_replace(ptrdiff_t off, const fat_text_header &ft, FILE *inf)
{
  // try open original fat binary
  FILE *outf = fopen(rdr_fname.c_str(), "r+b");
  if ( !outf ) {
    my_warn("cannot open fat binary %s, errno %d (%s)\n", rdr_fname.c_str(), errno, strerror(errno));
    return 0;
  }
  // alloc buffer
  char *buf = (char *)safemalloc(ft.size);
  if ( !buf ) {
    my_warn("cannot alloc %lX bytes\n", ft.size);
    fclose(outf);
    return 0;
  }
  // try to read whole content of inf file
  if ( 1 != fread(buf, ft.size, 1, inf) ) {
    my_warn("read error %d (%s)\n", errno, strerror(errno));
    fclose(outf);
    Safefree(buf);
    return 0;
  }
  int res = 0;
  // write
  fseek(outf, off, SEEK_SET);
  if ( 1 == fwrite(buf, ft.size, 1, outf) )
    res = 1;
  else
    my_warn("write error %d (%s)\n", errno, strerror(errno));
  fclose(outf);
  Safefree(buf);
  return res;
}

int CFatBin::extract(int idx, const char *of)
{
  // check idx
  auto ii = m_map.find(idx);
  if ( ii == m_map.end() ) {
    my_warn("invalid index %d\n", idx);
    return 0;
  }
  // try open of
  FILE *ofp = fopen(of, "wb");
  if ( !ofp ) {
    my_warn("cannot open %s, error %d (%s)\n", of, errno, strerror(errno));
    return 0;
  }
  int res = _extract(ii, of, ofp);
  fclose(ofp);
  return res;
}

int CFatBin::_extract(const FBItems::iterator &ii, const char *of, FILE *ofp)
{
  // seek to item in ii
  auto sec = m_e->rdr->sections[m_fb];
  auto data = sec->get_data() + ii->second.first + ii->second.second.header_size;
  if ( compressed(ii->second.second) ) {
    uint8_t *out_buf = (uint8_t *)safemalloc(ii->second.second.decompressed_size);
    if ( !out_buf ) {
      my_warn("cannot alloc %lX bytes for decompressed buffer\n", ii->second.second.decompressed_size);
      return 0;
    }
    int res = decompress((const uint8_t*)data, ii->second.second.compressed_size, out_buf, ii->second.second.decompressed_size);
    if ( !res ) {
      my_warn("cannot decompress\n");
      Safefree(out_buf);
      return 0;
    }
    if ( 1 != fwrite(out_buf, ii->second.second.decompressed_size, 1, ofp) ) {
      my_warn("fwrite decompressed failed, error %d (%s)\n", errno, strerror(errno));
      Safefree(out_buf);
      return 0;
    }
    Safefree(out_buf);
  } else {
    if ( 1 != fwrite(data, ii->second.second.size, 1, ofp) ) {
      my_warn("fwrite failed, error %d (%s)\n", errno, strerror(errno));
      return 0;
    }
  }
  return 1;
}

SV *CFatBin::fetch(int idx)
{
  // check idx
  auto ii = m_map.find(idx);
  if ( ii == m_map.end() ) {
    my_warn("invalid index %d\n", idx);
    return &PL_sv_undef;
  }
  // create and fill HV
  HV *hv = newHV();
  hv_store(hv, "kind", 4, newSViv(ii->second.second.kind), 0);
  if ( ii->second.second.unknown1 )
    hv_store(hv, "unk1", 4, newSViv(ii->second.second.unknown1), 0);
  hv_store(hv, "hsize", 5, newSViv(ii->second.second.header_size), 0);
  hv_store(hv, "size", 4, newSVuv(ii->second.second.size), 0);
  if ( ii->second.second.compressed_size )
    hv_store(hv, "csize", 5, newSVuv(ii->second.second.compressed_size), 0);
  if ( ii->second.second.unknown2 )
    hv_store(hv, "unk2", 4, newSViv(ii->second.second.unknown2), 0);
  hv_store(hv, "minor", 5, newSViv(ii->second.second.minor), 0);
  hv_store(hv, "major", 5, newSViv(ii->second.second.major), 0);
  hv_store(hv, "arch", 4, newSViv(ii->second.second.arch), 0);
  if ( ii->second.second.obj_name_offset )
    hv_store(hv, "name_off", 8, newSViv(ii->second.second.obj_name_offset), 0);
  if ( ii->second.second.obj_name_len )
    hv_store(hv, "name_len", 8, newSViv(ii->second.second.obj_name_len), 0);
  hv_store(hv, "flags", 5, newSVuv(ii->second.second.flags), 0);
  if ( ii->second.second.zero )
    hv_store(hv, "zero", 4, newSVuv(ii->second.second.zero), 0);
  if ( ii->second.second.decompressed_size )
    hv_store(hv, "decsize", 7, newSVuv(ii->second.second.decompressed_size), 0);
  return newRV_noinc((SV*)hv);
}

template <typename T>
static int magic_del(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        auto *m = (T *)mg->mg_ptr;
        if ( m ) delete m;
        mg->mg_ptr= NULL;
    }
    return 0; // ignored anyway
}

#ifdef MGf_LOCAL
#define TAB_TAIL ,0
#else
#define TAB_TAIL
#endif

static U32 my_len(pTHX_ SV *sv, MAGIC* mg);

// magic table for Elf::FatBinary
static const char *s_fatbin = "Elf::FatBinary";
static HV *s_fatbin_pkg = nullptr;
static MGVTBL fb_magic_vt = {
        0, /* get */
        0, /* write */
        my_len, /* length */
        0, /* clear */
        magic_del<CFatBin>,
        0, /* copy */
        0 /* dup */
        TAB_TAIL
};

// blessing macros
#define DWARF_EXT(vtab, pkg, what) \
  msv = newSViv(0); \
  objref = newRV_noinc((SV*)msv); \
  sv_bless(objref, pkg); \
  magic = sv_magicext((SV*)msv, NULL, PERL_MAGIC_ext, &vtab, (const char *)what, 0); \
  SvREADONLY_on((SV*)msv); \
  ST(0) = objref; \
  XSRETURN(1);

#define DWARF_TIE(vtab, pkg, what) \
  fake = newAV(); \
  objref = newRV_noinc((SV*)fake); \
  sv_bless(objref, pkg); \
  magic = sv_magicext((SV*)fake, NULL, PERL_MAGIC_tied, &vtab, (const char *)what, 0); \
  SvREADONLY_on((SV*)fake); \
  ST(0) = objref; \
  XSRETURN(1);

template <typename T>
static T *fb_magic_tied(SV *obj, int die, MGVTBL *tab)
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

static U32 my_len(pTHX_ SV *sv, MAGIC* mg)
{
  CFatBin *d = nullptr;
  if (SvMAGICAL(sv)) {
    MAGIC* magic;
    for (magic= SvMAGIC(sv); magic; magic = magic->mg_moremagic)
      if (magic->mg_type == PERL_MAGIC_tied && magic->mg_virtual == &fb_magic_vt) {
        d = (CFatBin*) magic->mg_ptr;
        break;
      }
  }
  if ( !d ) {
    my_warn("my_len %d\n", SvTYPE(sv));
    return 0;
  }
  return (U32)d->count();
}


MODULE = Elf::FatBinary		PACKAGE = Elf::FatBinary

void
new(obj_or_pkg, SV *elsv, const char *fname)
  SV *obj_or_pkg
 INIT:
  HV *pkg = NULL;
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  struct IElf *e= extract(elsv);
  CFatBin *res = nullptr;
  AV *fake;
 PPCODE:
  if (SvPOK(obj_or_pkg) && (pkg= gv_stashsv(obj_or_pkg, 0))) {
    if (!sv_derived_from(obj_or_pkg, s_fatbin))
        croak("Package %s does not derive from %s", SvPV_nolen(obj_or_pkg), s_fatbin);
  } else
    croak("new: first arg must be package name or blessed object");
  // make new CFatBin
  res = new CFatBin(e, fname);
  DWARF_TIE(fb_magic_vt, s_fatbin_pkg, res)

UV
read(SV *self)
 INIT:
  auto *d = fb_magic_tied<CFatBin>(self, 1, &fb_magic_vt);
 CODE:
  RETVAL = d->open();
 OUTPUT:
  RETVAL

UV
count(SV *self)
 INIT:
  auto *d = fb_magic_tied<CFatBin>(self, 1, &fb_magic_vt);
 CODE:
  RETVAL = d->count();
 OUTPUT:
  RETVAL

IV
extract(SV *self, int idx, const char *out_fn)
 INIT:
  auto *d = fb_magic_tied<CFatBin>(self, 1, &fb_magic_vt);
 CODE:
  RETVAL = d->extract(idx, out_fn);
 OUTPUT:
  RETVAL

IV
replace(SV *self, int idx, const char *in_fn)
 INIT:
  auto *d = fb_magic_tied<CFatBin>(self, 1, &fb_magic_vt);
 CODE:
  RETVAL = d->try_replace(idx, in_fn);
 OUTPUT:
  RETVAL

SV *
FETCH(SV *self, int idx)
 INIT:
  auto *d = fb_magic_tied<CFatBin>(self, 1, &fb_magic_vt);
 CODE:
  RETVAL = d->fetch(idx);
 OUTPUT:
  RETVAL

UV
ctrl_idx(SV *self)
 INIT:
  auto *d = fb_magic_tied<CFatBin>(self, 1, &fb_magic_vt);
 CODE:
  RETVAL = d->ctrl_idx();
 OUTPUT:
  RETVAL

UV
fb_idx(SV *self)
 INIT:
  auto *d = fb_magic_tied<CFatBin>(self, 1, &fb_magic_vt);
 CODE:
  RETVAL = d->fb_idx();
 OUTPUT:
  RETVAL

BOOT:
 s_fatbin_pkg = gv_stashpv(s_fatbin, 0);
 if ( !s_fatbin_pkg )
    croak("Package %s does not exists", s_fatbin);