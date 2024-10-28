// #define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "ElfFile.h"
#include "../elf.inc"

extern int g_opt_f, g_opt_k, g_opt_F;

static HV *s_elem_pkg,
 *s_member_iter_pkg,
 *s_method_iter_pkg,
 *s_param_pkg, *s_param_iter_pkg,
 *s_parent_pkg, *s_parent_iter_pkg;

class PerlLog: public ErrLog
{
public:
  virtual void error(const char *fmt, ...)
   {
     va_list argp;
     va_start(argp, fmt);
     vcroak(fmt, &argp);
   }
   virtual void warning(const char *fmt, ...)
   {
     va_list argp;
     va_start(argp, fmt);
     vwarn(fmt, &argp);
   }
} pg_log;

class BorrowedElf: public ElfFile
{
  public:
   BorrowedElf(IElf *e, TreeBuilder *tb, bool &success)
    : ElfFile(tb)
   {
     m_e = e;
     reader = e->rdr;
     cmn_read(success);
   }
   virtual ~BorrowedElf()
   {
     m_e->release();
   }
  protected:
   IElf *m_e;
};

// forward declaration - this object hold PerlRenderer and use for reference counting
struct IDwarf;

class PerlRenderer: public TreeBuilder
{
 public:
   PerlRenderer(): TreeBuilder(&pg_log)
   { }
   // objects for binding
#define PDWARF(name, type) struct name { \
   name(struct IDwarf *_e, type *_t): e(_e), t(_t) {} \
   ~name(); \
   struct IDwarf *e; \
   type *t;

   PDWARF(DParam, FormalParam) };
   PDWARF(DParamIter, std::vector<FormalParam>) };
   PDWARF(DParent, Parent) };
   PDWARF(DParentIter, std::vector<Parent>) };
   PDWARF(DMemberIter, std::vector<Element>) };
   struct DMethodIter {
     ~DMethodIter();
     DMethodIter(struct IDwarf *_e, std::list<Method> &m): e(_e) {
       t = new std::vector<Element *>;
       t->reserve( m.size() );
       for ( auto &im: m ) t->push_back( &im );
     }
     struct IDwarf *e;
     std::vector<Element *> *t;
   };
   PDWARF(DElem, Element)
    IV rank() const
    { return t->get_rank(); }
    // methods attributes
    IV mvirt() const
    {
      if ( t->type_ != method ) return 0;
      auto method = (Method *)t;
      return method->virt_;
    }
    UV mvtbl_idx() const
    {
      if ( t->type_ != method ) return 0;
      auto method = (Method *)t;
      return method->vtbl_index_;
    }
    UV mthis_arg() const
    {
      if ( t->type_ != method ) return 0;
      auto method = (Method *)t;
      return method->this_arg_;
    }
    bool mart() const
    {
      if ( t->type_ != method ) return 0;
      auto method = (Method *)t;
      return method->art_;
    }
    bool mdef() const
    {
      if ( t->type_ != method ) return 0;
      auto method = (Method *)t;
      return method->def_;
    }
    bool mexpl() const
    {
      if ( t->type_ != method ) return 0;
      auto method = (Method *)t;
      return method->expl_;
    }
   };
   // find methods
   Element *by_addr(uint64_t addr)
   {
     auto ait = m_addresses.find(addr);
     if ( ait == m_addresses.end() ) return nullptr;
     return ait->second;
   }
   Element *by_id(uint64_t id)
   {
     auto iit = m_id.find(id);
     if ( iit == m_id.end() ) return nullptr;
     return iit->second;
   }
   Element *by_name(SV *sv)
   {
     STRLEN len;
     char *ptr = SvPVbyte(sv, len);
     if ( !ptr || !len ) return nullptr;
     std::string_view key{ ptr, len };
     auto res = m_names.find(key);
     if ( res == m_names.end() ) return nullptr;
     return res->second;
   }
   // some stat
   size_t id_cnt() const
   { return m_id.size(); }
   size_t names_cnt() const
   { return m_names.size(); }
   size_t addr_cnt() const
   { return m_addresses.size(); }
   // get tag arrays with some type
   AV *named(IV type)
   {
     AV *av = newAV();
     for ( auto &n: m_names ) {
       if ( n.second->type_ != type ) continue;
       av_push(av, newSVuv( n.second->id_ ));
     }
     return av;
   }
 protected:
   virtual void RenderUnit(int last) override;
   virtual void store_addr(Element *e, uint64_t addr) override {
     if ( e->spec_ ) {
       m_pending_addr[addr] = e->spec_;
     } else {
       auto present = m_addresses.find(addr);
       if ( present == m_addresses.end() )
         m_addresses[addr] = e;
       else /* we don't have rank here so just put in m_unranked_addr */
         m_unranked_addr.push_back( {addr, e} );
     }
   }
   std::list< std::list<Element> > m_storage;
   /* main maps for names -> Element & ID -> Element */
   std::unordered_map< std::string_view, Element *> m_names;
   std::unordered_map< uint64_t, Element *> m_id;
   /* addresses */
   std::unordered_map<uint64_t, Element *> m_addresses; // key - address
   // typical sample for plain C:
   // dw_tag_variable with couple attribs: dw_at_specification & dw_at_location
   std::unordered_map<uint64_t, int64_t> m_pending_addr; // key - address, value - ID
   std::list< std::pair<uint64_t, Element *> > m_unranked_addr;
};

struct IDwarf
{
  // must be first field so PerlRenderer* can be converted to IDwarf*
  PerlRenderer pr;
  BorrowedElf *be;
  int ref_cnt = 1;
  ~IDwarf()
  {
    if ( be ) delete be;
  }
  void add_ref() {
    ref_cnt++;
  }
  void release() {
    if ( !--ref_cnt ) delete this;
  }
};

void PerlRenderer::RenderUnit(int last)
{
  m_storage.push_back( std::move(elements_) );
  auto &curr = m_storage.back();
  // fill m_names & m_id
  for ( auto &el: curr )
  {
    if ( is_ns(el) ) continue;
    if ( el.type_ == inheritance || el.type_ == enumerator || el.type_ == formal_param ) continue;
    // named elements
    auto name = el.mangled();
    if ( name != nullptr ) {
      std::string_view s{ name, strlen(name) };
      auto niter = m_names.find(s);
      if ( niter == m_names.end() )
        m_names[s] = &el;
      else {
        // add element with biggest rank
        auto new_rank = el.get_rank();
        auto old_rank = niter->second->get_rank();
        if ( new_rank > old_rank )
          m_names[s] = &el;
      }
    }
    // by id
    m_id[ el.id_] = &el;
  }
  // specs - addr/id
  for ( auto sic: m_pending_addr )
  {
    auto idi = m_id.find( sic.second );
    if ( idi != m_id.end() )
      m_addresses[ sic.first ] = idi->second;
    else
     pg_log.warning("cannot find spec ID %X for address %p\n", sic.second, sic.first);
  }
  // process pending addresses - compare their rankks
  for ( auto pu: m_unranked_addr )
  {
    auto ait = m_addresses.find(pu.first);
    if ( ait == m_addresses.end() ) continue;
    auto new_rank = pu.second->get_rank();
    auto old_rank = ait->second->get_rank();
    if ( new_rank > old_rank )
      m_addresses[ pu.first ] = pu.second;
  }
  // clear pending addresses
  m_pending_addr.clear();
  m_unranked_addr.clear();
}

#define DESTR(name) PerlRenderer::name::~name() { e->release(); }

DESTR(DElem)
DESTR(DParam)
DESTR(DParamIter)
DESTR(DParent)
DESTR(DParentIter)
DESTR(DMemberIter)

PerlRenderer::DMethodIter::~DMethodIter()
{
  e->release();
  if ( t ) delete t;
}


template <typename T>
static int dwarf_magic_release(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        auto *m = (T *)mg->mg_ptr;
        if ( m ) m->release();
        mg->mg_ptr= NULL;
    }
    return 0; // ignored anyway
}

template <typename T>
static int dwarf_magic_del(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        auto *m = (T *)mg->mg_ptr;
        if ( m ) delete m;
        mg->mg_ptr= NULL;
    }
    return 0; // ignored anyway
}

// magic table for Dwarf::Loader
static MGVTBL dwarf_magic_vt = {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        dwarf_magic_release<IDwarf>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

static const char *s_delem = "Dwarf::Loader::Element";
// magic table for Dwarf::Loader::Element
static MGVTBL delem_magic_vt = {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        dwarf_magic_del<PerlRenderer::DElem>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

static const char *s_fparam = "Dwarf::Loader::Param";
// magic table for Dwarf::Loader::Param
static MGVTBL dparam_magic_vt = {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        dwarf_magic_del<PerlRenderer::DParam>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

static const char *s_fparent = "Dwarf::Loader::Parent";
// magic table for Dwarf::Loader::Parent
static MGVTBL dparent_magic_vt = {
        0, /* get */
        0, /* write */
        0, /* length */
        0, /* clear */
        dwarf_magic_del<PerlRenderer::DParent>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

// iterators
template<typename T>
static U32 dwarf_magic_size(pTHX_ SV* sv, MAGIC* mg) {
    if (mg->mg_ptr) {
        auto *m = (T *)mg->mg_ptr;
        return m->t->size()-1;
    }
    return 0; // ignored anyway
}

static const char *s_fparams = "Dwarf::Loader::ParamIterator";
// magic table for Dwarf::Loader::Param
static MGVTBL dparam_iter_vt = {
        0, /* get */
        0, /* write */
        dwarf_magic_size<PerlRenderer::DParamIter>, /* length */
        0, /* clear */
        dwarf_magic_del<PerlRenderer::DParamIter>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

static const char *s_fparents = "Dwarf::Loader::ParentIterator";
// magic table for Dwarf::Loader::ParentIterator
static MGVTBL dparent_iter_vt = {
        0, /* get */
        0, /* write */
        dwarf_magic_size<PerlRenderer::DParentIter>, /* length */
        0, /* clear */
        dwarf_magic_del<PerlRenderer::DParentIter>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

static const char *s_members = "Dwarf::Loader::MemberIterator";
// magic table for Dwarf::Loader::MemberIterator
static MGVTBL dmember_iter_vt = {
        0, /* get */
        0, /* write */
        dwarf_magic_size<PerlRenderer::DMemberIter>, /* length */
        0, /* clear */
        dwarf_magic_del<PerlRenderer::DMemberIter>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

static const char *s_methods = "Dwarf::Loader::MethodIterator";
// magic table for Dwarf::Loader::MethodIterator
static MGVTBL dmethod_iter_vt = {
        0, /* get */
        0, /* write */
        dwarf_magic_size<PerlRenderer::DMethodIter>, /* length */
        0, /* clear */
        dwarf_magic_del<PerlRenderer::DMethodIter>,
        0, /* copy */
        0 /* dup */
#ifdef MGf_LOCAL
        ,0
#endif
};

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
static T *dwarf_magic_ext(SV *obj, int die, MGVTBL *tab)
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
static T *dwarf_magic_tied(SV *obj, int die, MGVTBL *tab)
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

#define EXPORT_ENUM(name, x) newCONSTSUB(stash, name, new_enum_dualvar(aTHX_ x, newSVpvs_share(name)));
#define EXPORT_TENUM(name, x) newCONSTSUB(stash, name, new_enum_dualvar(aTHX_ TreeBuilder::ElementType::x, newSVpvs_share(name)));
static SV * new_enum_dualvar(pTHX_ IV ival, SV *name) {
        SvUPGRADE(name, SVt_PVNV);
        SvIV_set(name, ival);
        SvIOK_on(name);
        SvREADONLY_on(name);
        return name;
}

MODULE = Dwarf::Loader		PACKAGE = Dwarf::Loader

void
new(obj_or_pkg, SV *elsv)
  SV *obj_or_pkg
 INIT:
  HV *pkg = NULL;
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  struct IElf *e= extract(elsv);
  IDwarf *res = nullptr;
  bool succ = false;
 PPCODE:
  if (SvPOK(obj_or_pkg) && (pkg= gv_stashsv(obj_or_pkg, 0))) {
    if (!sv_derived_from(obj_or_pkg, "Dwarf::Loader"))
        croak("Package %s does not derive from Dwarf::Loader", SvPV_nolen(obj_or_pkg));
  } else
    croak("new: first arg must be package name or blessed object");
  // make new IDwarf
  res = new IDwarf();
  // try to read
  e->add_ref();
  res->be = new BorrowedElf(e, &res->pr, succ);
  if ( !succ ) {
    pg_log.warning("cannot read DWARF\n");
    delete res;
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  // read
  res->be->GetAllClasses();
  // bless
  DWARF_EXT(dwarf_magic_vt, pkg, res)

void
by_name(SV *self, SV *key)
 INIT:
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  auto *d = dwarf_magic_ext<IDwarf>(self, 1, &dwarf_magic_vt);
 PPCODE:
  auto del = d->pr.by_name(key);
  if ( !del ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  d->add_ref();
  auto res = new PerlRenderer::DElem(d, del);
  // bless
  DWARF_EXT(delem_magic_vt, s_elem_pkg, res)

void
by_addr(SV *self, UV key)
 INIT:
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  auto *d = dwarf_magic_ext<IDwarf>(self, 1, &dwarf_magic_vt);
 PPCODE:
  auto del = d->pr.by_addr(key);
  if ( !del ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  d->add_ref();
  auto res = new PerlRenderer::DElem(d, del);
  // bless
  DWARF_EXT(delem_magic_vt, s_elem_pkg, res)

void
by_id(SV *self, UV tag)
 INIT:
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  auto *d = dwarf_magic_ext<IDwarf>(self, 1, &dwarf_magic_vt);
 PPCODE:
  auto del = d->pr.by_id(tag);
  if ( !del ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  d->add_ref();
  auto res = new PerlRenderer::DElem(d, del);
  // bless
  DWARF_EXT(delem_magic_vt, s_elem_pkg, res)

UV
id_cnt(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<IDwarf>(self, 1, &dwarf_magic_vt);
 CODE:
  RETVAL = d->pr.id_cnt();
 OUTPUT:
  RETVAL

UV
names_cnt(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<IDwarf>(self, 1, &dwarf_magic_vt);
 CODE:
  RETVAL = d->pr.names_cnt();
 OUTPUT:
  RETVAL

UV
addr_cnt(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<IDwarf>(self, 1, &dwarf_magic_vt);
 CODE:
  RETVAL = d->pr.addr_cnt();
 OUTPUT:
  RETVAL

void
named(SV *self, int type)
 INIT:
  auto *d = dwarf_magic_ext<IDwarf>(self, 1, &dwarf_magic_vt);
 PPCODE:
  mXPUSHs( newRV_noinc((SV*)d->pr.named(type)) );
  XSRETURN(1);

MODULE = Dwarf::Loader		PACKAGE = Dwarf::Loader::Element

UV
tag(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 CODE:
  RETVAL = d->t->id_;
 OUTPUT:
  RETVAL

UV
abs(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 CODE:
  RETVAL = d->t->abs_;
 OUTPUT:
  RETVAL

IV
rank(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 CODE:
  RETVAL = d->rank();
 OUTPUT:
  RETVAL

IV
type(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 CODE:
  RETVAL = d->t->type_;
 OUTPUT:
  RETVAL

IV
size(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 CODE:
  RETVAL = d->t->size_;
 OUTPUT:
  RETVAL

UV
type_id(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 CODE:
  RETVAL = d->t->type_id_;
 OUTPUT:
  RETVAL

UV
offset(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 CODE:
  RETVAL = d->t->offset_;
 OUTPUT:
  RETVAL

UV
align(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 CODE:
  RETVAL = d->t->align_;
 OUTPUT:
  RETVAL

IV
inlined(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 CODE:
  RETVAL = d->t->inlined_;
 OUTPUT:
  RETVAL

IV
access(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 CODE:
  RETVAL = d->t->access_;
 OUTPUT:
  RETVAL

IV
bit_size(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 CODE:
  RETVAL = d->t->bit_size_;
 OUTPUT:
  RETVAL

IV
bit_offset(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 CODE:
  RETVAL = d->t->bit_offset_;
 OUTPUT:
  RETVAL

void
owner(SV *self)
 INIT:
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( !d->t->owner_ ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  // wrap owner to DElem
  d->e->add_ref();
  auto res = new PerlRenderer::DElem(d->e, d->t->owner_);
  // bless
  DWARF_EXT(delem_magic_vt, s_elem_pkg, res)

void
name(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( !d->t->name_ ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  ST(0) = sv_2mortal( newSVpv( d->t->name_, strlen(d->t->name_) ) );
  XSRETURN(1);

void
link_name(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( !d->t->link_name_ ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  ST(0) = sv_2mortal( newSVpv( d->t->link_name_, strlen(d->t->link_name_) ) );
  XSRETURN(1);

void
fname(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( d->t->fullname_.empty() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  ST(0) = sv_2mortal( newSVpv( d->t->fullname_.c_str(), d->t->fullname_.size() ) );
  XSRETURN(1);

void
mvirt(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( d->t->type_ != TreeBuilder::ElementType::method ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  ST(0) = sv_2mortal( newSViv( d->mvirt() ) );
  XSRETURN(1);

void
mvtbl_idx(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( d->t->type_ != TreeBuilder::ElementType::method ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  ST(0) = sv_2mortal( newSVuv( d->mvtbl_idx() ) );
  XSRETURN(1);

void
mthis_arg(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( d->t->type_ != TreeBuilder::ElementType::method ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  ST(0) = sv_2mortal( newSVuv( d->mthis_arg() ) );
  XSRETURN(1);

void
mart(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( d->t->type_ != TreeBuilder::ElementType::method ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  if ( d->mart() )
    XSRETURN_YES;
  else
    XSRETURN_NO;

void
mdef(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( d->t->type_ != TreeBuilder::ElementType::method ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  if ( d->mdef() )
    XSRETURN_YES;
  else
    XSRETURN_NO;

void
mexpl(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( d->t->type_ != TreeBuilder::ElementType::method ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  if ( d->mexpl() )
    XSRETURN_YES;
  else
    XSRETURN_NO;

void
params(SV *self)
 INIT:
  AV *fake;
  SV *objref= NULL;
  MAGIC* magic;
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( !d->t->m_comp || d->t->m_comp->params_.empty() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  // make new PerlRenderer::DParamIter
  d->e->add_ref();
  auto res = new PerlRenderer::DParamIter(d->e, &d->t->m_comp->params_);
  // bless
  DWARF_TIE(dparam_iter_vt, s_param_iter_pkg, res)

void
parents(SV *self)
 INIT:
  AV *fake;
  SV *objref= NULL;
  MAGIC* magic;
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( !d->t->m_comp || d->t->m_comp->parents_.empty() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  // make new PerlRenderer::DParentIter
  d->e->add_ref();
  auto res = new PerlRenderer::DParentIter(d->e, &d->t->m_comp->parents_);
  // bless
  DWARF_TIE(dparent_iter_vt, s_parent_iter_pkg, res)

void
members(SV *self)
 INIT:
  AV *fake;
  SV *objref= NULL;
  MAGIC* magic;
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( !d->t->m_comp || d->t->m_comp->members_.empty() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  // make new PerlRenderer::DMemberIter
  d->e->add_ref();
  auto res = new PerlRenderer::DMemberIter(d->e, &d->t->m_comp->members_);
  // bless
  DWARF_TIE(dmember_iter_vt, s_member_iter_pkg, res)

void
methods(SV *self)
 INIT:
  AV *fake;
  SV *objref= NULL;
  MAGIC* magic;
  auto *d = dwarf_magic_ext<PerlRenderer::DElem>(self, 1, &delem_magic_vt);
 PPCODE:
  if ( !d->t->m_comp || d->t->m_comp->methods_.empty() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  // make new PerlRenderer::DMethodIter
  d->e->add_ref();
  auto res = new PerlRenderer::DMethodIter(d->e, d->t->m_comp->methods_);
  // bless
  DWARF_TIE(dmethod_iter_vt, s_method_iter_pkg, res)


MODULE = Dwarf::Loader		PACKAGE = Dwarf::Loader::Param

void
name(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DParam>(self, 1, &dparam_magic_vt);
 PPCODE:
  if ( !d->t->name ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  ST(0) = sv_2mortal( newSVpv( d->t->name, strlen(d->t->name) ) );
  XSRETURN(1);

IV
type_id(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DParam>(self, 1, &dparam_magic_vt);
 CODE:
  RETVAL = d->t->id;
 OUTPUT:
  RETVAL

void
ellips(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DParam>(self, 1, &dparam_magic_vt);
 PPCODE:
  if ( d->t->ellipsis )
    XSRETURN_YES;
  else
    XSRETURN_NO;

void
var(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DParam>(self, 1, &dparam_magic_vt);
 PPCODE:
  if ( d->t->var_ )
    XSRETURN_YES;
  else
    XSRETURN_NO;

void
art(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DParam>(self, 1, &dparam_magic_vt);
 PPCODE:
  if ( d->t->art_ )
    XSRETURN_YES;
  else
    XSRETURN_NO;

void
opt(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DParam>(self, 1, &dparam_magic_vt);
 PPCODE:
  if ( d->t->optional_ )
    XSRETURN_YES;
  else
    XSRETURN_NO;

MODULE = Dwarf::Loader		PACKAGE = Dwarf::Loader::ParamIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 INIT:
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  auto *d = dwarf_magic_tied<PerlRenderer::DParamIter>(self, 1, &dparam_iter_vt);
 PPCODE:
  if ( key >= d->t->size() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  auto &p = d->t->at(key);
  // bless result into Dwarf::Loader::Param
  d->e->add_ref();
  auto res = new PerlRenderer::DParam(d->e, &p);
  // bless
  DWARF_EXT(dparam_magic_vt, s_param_pkg, res)

MODULE = Dwarf::Loader		PACKAGE = Dwarf::Loader::Parent

UV
type_id(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DParent>(self, 1, &dparent_magic_vt);
 CODE:
  RETVAL = d->t->id;
 OUTPUT:
  RETVAL

IV
offset(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DParent>(self, 1, &dparent_magic_vt);
 CODE:
  RETVAL = d->t->offset;
 OUTPUT:
  RETVAL

IV
access(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DParent>(self, 1, &dparent_magic_vt);
 CODE:
  RETVAL = d->t->access;
 OUTPUT:
  RETVAL

void
virtual(SV *self)
 INIT:
  auto *d = dwarf_magic_ext<PerlRenderer::DParent>(self, 1, &dparent_magic_vt);
 PPCODE:
  if ( d->t->virtual_ )
    XSRETURN_YES;
  else
    XSRETURN_NO;

MODULE = Dwarf::Loader		PACKAGE = Dwarf::Loader::ParentIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 INIT:
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  auto *d = dwarf_magic_tied<PerlRenderer::DParentIter>(self, 1, &dparent_iter_vt);
 PPCODE:
  if ( key >= d->t->size() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  auto &p = d->t->at(key);
  // bless result into Dwarf::Loader::Parent
  d->e->add_ref();
  auto res = new PerlRenderer::DParent(d->e, &p);
  // bless
  DWARF_EXT(dparent_magic_vt, s_parent_pkg, res)


MODULE = Dwarf::Loader		PACKAGE = Dwarf::Loader::MemberIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 INIT:
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  auto *d = dwarf_magic_tied<PerlRenderer::DMemberIter>(self, 1, &dmember_iter_vt);
 PPCODE:
  if ( key >= d->t->size() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  auto &p = d->t->at(key);
  // bless result into Dwarf::Loader::Element
  d->e->add_ref();
  auto res = new PerlRenderer::DElem(d->e, &p);
  // bless
  DWARF_EXT(delem_magic_vt, s_elem_pkg, res)

MODULE = Dwarf::Loader		PACKAGE = Dwarf::Loader::MethodIterator

void
FETCH(self, key)
  SV *self;
  IV key;
 INIT:
  SV *msv;
  SV *objref= NULL;
  MAGIC* magic;
  auto *d = dwarf_magic_tied<PerlRenderer::DMethodIter>(self, 1, &dmethod_iter_vt);
 PPCODE:
  if ( key >= d->t->size() ) {
    ST(0) = &PL_sv_undef;
    XSRETURN(1);
  }
  auto p = d->t->at(key);
  // bless result into Dwarf::Loader::Element
  d->e->add_ref();
  auto res = new PerlRenderer::DElem(d->e, p);
  // bless
  DWARF_EXT(delem_magic_vt, s_elem_pkg, res)

BOOT:
 // store frequently used packages
 s_elem_pkg = gv_stashpv(s_delem, 0);
 if ( !s_elem_pkg )
    croak("Package %s does not exists", s_delem);
 s_param_iter_pkg = gv_stashpv(s_fparams, 0);
 if ( !s_param_iter_pkg )
    croak("Package %s does not exists", s_fparams);
 s_param_pkg = gv_stashpv(s_fparam, 0);
 if ( !s_param_pkg )
    croak("Package %s does not exists", s_fparam);
 s_parent_pkg = gv_stashpv(s_fparent, 0);
 if ( !s_parent_pkg )
    croak("Package %s does not exists", s_fparent);
 s_parent_iter_pkg = gv_stashpv(s_fparents, 0);
 if ( !s_parent_iter_pkg )
    croak("Package %s does not exists", s_fparents);
 s_member_iter_pkg = gv_stashpv(s_members, 0);
 if ( !s_member_iter_pkg )
    croak("Package %s does not exists", s_members);
 s_method_iter_pkg = gv_stashpv(s_methods, 0);
 if ( !s_method_iter_pkg )
    croak("Package %s does not exists", s_methods);


 HV *stash= gv_stashpvn("Dwarf::Loader", 13, 1);
 g_opt_f = g_opt_k = g_opt_F = 1;
 // dump TreeBuilder::ElementType
 EXPORT_TENUM("TArray", array_type)
 EXPORT_TENUM("TClass", class_type)
 EXPORT_TENUM("TInterface", interface_type)
 EXPORT_TENUM("TEnum", enumerator_type)
 EXPORT_TENUM("TMember", member)
 EXPORT_TENUM("TPtr", pointer_type)
 EXPORT_TENUM("TStruct", structure_type)
 EXPORT_TENUM("TUnion", union_type)
 EXPORT_TENUM("TTypedef", typedef2)
 EXPORT_TENUM("TBase", base_type)
 EXPORT_TENUM("TConst", const_type)
 EXPORT_TENUM("TVolatile", volatile_type)
 EXPORT_TENUM("TRestrict", restrict_type)
 EXPORT_TENUM("TDynamic", dynamic_type)
 EXPORT_TENUM("TAtomic", atomic_type)
 EXPORT_TENUM("TImmutable", immutable_type)
 EXPORT_TENUM("TRef", reference_type)
 EXPORT_TENUM("TRValue", rvalue_ref_type)
 EXPORT_TENUM("TSubType", subroutine_type)
 EXPORT_TENUM("TArg", formal_param)
 EXPORT_TENUM("TSub", subroutine)
 EXPORT_TENUM("TMethod", method)
 EXPORT_TENUM("TPtr2Member", ptr2member)
 EXPORT_TENUM("TUnspec", unspec_type)
 EXPORT_TENUM("TVar", var_type)
 EXPORT_TENUM("TVariant", variant_type)
