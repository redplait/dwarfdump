#pragma once
#include <string>
#include <map>
#include <list>
#include <vector>
#include <stack>
#include "Err.h"
#include "regnames.h"
#include "GoTypes.h"

extern int g_opt_d, g_opt_g, g_opt_k, g_opt_l, g_opt_s, g_opt_v, g_opt_x;
extern FILE *g_outf;

enum param_op_type
{
  reg = 1,
  breg,
  fbreg,
  deref,
  call_frame_cfa,
  plus_uconst,
  tls_index, // tls index in offset
  deref_size, // size in idx
  deref_type, // type id in conv, size in offset
  regval_type, // type id in conv, reg in offset
  convert,    // type id in conv
  fvalue, // litXX, value in idx
  svalue, // signed value in sv
  uvalue, // unsigned value in conv
  fpiece, // DW_OP_piece, value in idx
  imp_value, // DW_OP_implicit_value, value in idx
  fneg,   // DW_OP_neg
  fnot,   // DW_OP_not
  fand,   // DW_OP_and
  fabs,   // DW_OP_abs
  fminus, // DW_OP_minus
  f_or,   // DW_OP_or
  fplus,  // DW_OP_plus
  fshl,   // DW_OP_shl
  fshr,   // DW_OP_shr
  fshra,  // DW_OP_shra
  fxor,   // DW_OP_xor
  fmul,   // DW_OP_mul
  fdiv,   // DW_OP_div
  fmod,   // DW_OP_mod
  fstack, // DW_OP_stack_value
};

struct one_param_loc
{
  enum param_op_type type;
  union
  {
    unsigned int idx;
    uint64_t conv;
    int64_t sv;
  };
  int offset;
  bool operator==(const one_param_loc &c) const
  {
    if ( type != c.type )
      return false;
    if ( offset != c.offset )
      return false;
    if ( type == svalue )
      return sv == c.sv;
    else if ( type == convert || type == uvalue || type == deref_type || type == regval_type )
      return conv == c.conv;
    return idx == c.idx;
  }
};

struct param_loc
{
  std::list<one_param_loc> locs;
  bool operator==(const param_loc &c) const
  {
    return locs == c.locs;
  }
  inline bool empty() const
  {
    return locs.empty();
  }
  inline bool is_tls() const
  {
    return 1 == locs.size() && locs.front().type == tls_index;
  }
  // for DW_OP_form_tls_address - like push_tls but checks for svalue/uvalue in stack
  inline bool push_tls_addr()
  {
    if ( locs.empty() )
      return false;
    auto &last = locs.back();
    if ( last.type == svalue )
    {
      last.type = tls_index;
      last.offset = (int)last.sv;
      last.sv = 0;
      return true;
    }
    if ( last.type == uvalue )
    {
      last.type = tls_index;
      last.offset = (int)last.sv;
      last.sv = 0;
      return true;
    }
    return false;
  }
  inline bool push_tls()
  {
    if ( locs.empty() )
      return false;
    auto &last = locs.back();
    if ( last.type == fvalue )
    {
      last.type = tls_index;
      last.offset = (int)last.idx;
      last.idx = 0;
      return true;
    }
    if ( last.type == svalue )
    {
      last.type = tls_index;
      last.offset = (int)last.conv;
      last.conv = 0;
      return true;
    }
    return false;
  }
  void push_exp(enum param_op_type type)
  {
    locs.push_back( { type, 0, 0 } );
  }
  void push_value(unsigned int v)
  {
    locs.push_back( { fvalue, v, 0 } );
  }
  void push_svalue(int64_t s)
  {
    one_param_loc tmp;
    tmp.type = svalue;
    tmp.sv = s;
    tmp.offset = 0;
    locs.push_back(tmp);
  }
  void push_uvalue(uint64_t s)
  {
    one_param_loc tmp;
    tmp.type = uvalue;
    tmp.conv = s;
    tmp.offset = 0;
    locs.push_back(tmp);
  }
  void push_conv(uint64_t v)
  {
    one_param_loc tmp;
    tmp.type = convert;
    tmp.conv = v;
    tmp.offset = 0;
    locs.push_back(tmp);
  }
  void push_deref_type(uint64_t v, int size)
  {
    one_param_loc tmp;
    tmp.type = deref_type;
    tmp.conv = v;
    tmp.offset = size;
    locs.push_back(tmp);
  }
  void push_regval_type(uint64_t v, int reg)
  {
    one_param_loc tmp;
    tmp.type = regval_type;
    tmp.conv = v;
    tmp.offset = reg;
    locs.push_back(tmp);
  }
};

struct LocListXItem
{
  uint64_t start;
  uint64_t end;
  param_loc loc;
  LocListXItem()
  {
    start = end = 0;
  }
  LocListXItem(uint64_t s, uint64_t e)
  {
    start = s;
    end = e;
  }
};

struct cu
{
  const char *cu_name;
  const char *cu_comp_dir;
  const char *cu_producer;
  const char *cu_package;
  int cu_lang;
  uint64_t cu_base_addr;
  uint64_t cu_base_addr_idx;
  bool need_base_addr_idx;
};

class TreeBuilder {
public:
  TreeBuilder(ErrLog *e);
  virtual ~TreeBuilder();

  enum ElementType {
    none,
    array_type,
    class_type,
    interface_type,
    enumerator_type,
    enumerator,
    member,
    pointer_type,
    structure_type,
    typedef2,
    union_type,
    inheritance,
    subrange_type,
    base_type,
    // there are 7 modifier/qualifier type tags
    const_type,
    volatile_type,
    restrict_type,
    dynamic_type,
    atomic_type, // _Atomic - since C11
    immutable_type,
    // I don`t have samples for DW_TAG_packed_type & DW_TAG_shared_type
    reference_type,
    rvalue_ref_type,
    subroutine_type,
    formal_param,
    subroutine,
    method,
    ptr2member,
    unspec_type, // DW_TAG_unspecified_type
    lexical_block, // when -L option is set
    var_type,
    variant_type, // DW_TAG_variant_part
    ns_start,
    ns_end,
  };
  inline bool need_filename() const
  {
    switch(current_element_type_)
    {
      case none:
      case enumerator:
      case inheritance:
      case formal_param:
      case ns_start:
      case ns_end:
      case lexical_block:
       return false;
      default: return true;
    }
  }
  inline bool is_enum() const
  {
    return current_element_type_ == enumerator;
  }
  inline bool is_formal_param() const
  {
    return current_element_type_ == formal_param;
  }
  bool is_local_var() const;
  void ProcessUnit(int last = 0);
  bool PostProcessTag();
  int add2stack();
  void pop_stack(uint64_t);
  void AddNone();
  void set_range(uint64_t, unsigned char addr_size);
  void AddElement(ElementType element_type, uint64_t tag_id, int level);
  bool AddVariant();
  bool AddFormalParam(uint64_t tag_id, int level, bool);
  void SetFilename(std::string &, const char *);
  void SetElementName(const char* name, uint64_t off);
  void SetLinkageName(const char* name);
  void SetElementSize(uint64_t size);
  void SetElementOffset(uint64_t offset);
  void SetElementType(uint64_t type_id);
  void SetElementCount(uint64_t count);
  void SetConstValue(uint64_t count);
  void SetVarConstValue(uint64_t count);
  void SetAlignment(uint64_t);
  void SetAddr(uint64_t);
  void SetParamDirection(unsigned char);
  void SetContainingType(uint64_t);
  void SetBitSize(int);
  void SetAddressClass(int, uint64_t off);
  void SetBitOffset(int);
  void SetParentAccess(int);
  void SetVirtuality(int);
  void SetObjPtr(uint64_t);
  void SetNoReturn();
  void SetDeclaration();
  void SetArtiticial();
  void SetVtblIndex(uint64_t);
  void SetDefaulted();
  void SetExplicit();
  void SetRef_();
  void SetRValRef_();
  void SetConstExpr();
  void SetEnumClass();
  void SetGNUVector();
  void SetTensor();
  void SetSpec(uint64_t);
  void SetAbs(uint64_t);
  void SetLocX(uint64_t);
  void SetDiscr(uint64_t);
  void SetInlined(int);
  void SetVarParam(bool);
  void SetOptionalParam();
  void SetAte(unsigned char);
  void SetLocation(param_loc *);
  void SetTlsIndex(param_loc *);
  void SetLocVarLocation(param_loc *);
  // go extended attributes - stored in m_go_attrs
  void SetGoKind(uint64_t, int);
  void SetGoKey(uint64_t, uint64_t);
  void SetGoDictIndex(uint64_t, int);
  void SetGoElem(uint64_t, uint64_t);
  void SetGoRType(uint64_t, const void *);

  uint64_t get_replaced_type(uint64_t) const;
  void collect_go_types();
  // renderer methods
  bool get_replaced_name(uint64_t, std::string &);
  bool get_replaced_name(uint64_t, std::string &, unsigned char *ate);
  // error logger
  ErrLog *e_;
  // compilation unit data
  struct cu cu;
  bool is_go() const;
  RegNames *m_rnames = nullptr;
  ISectionNames *m_snames = nullptr;
  IGetLoclistX *m_locX = nullptr;
  // for names with direct string - seems that if name lesser pointer size they are directed
  // so renderer should be able to distinguish if some name located in string pool
  // in other case this name should be considered as direct string
  const unsigned char *debug_str_ = nullptr;
  size_t debug_str_size_ = 0;
  bool has_rngx = false; // ranges in .debug_rnglists (so use m_rng2 for lookup) or in old .debug_ranges - then m_rng
  inline bool in_string_pool(const char *s)
  {
    return (s >= (const char *)debug_str_) && (s < (const char *)debug_str_ + debug_str_size_);
  }
protected:
  virtual bool conv2str(uint64_t key, std::string &)
  { return false; }
  virtual void RenderUnit(int last)
  {}
  void put_file_hdr();
  void put_file_hdr(struct cu *);
  int merge_dumped();
  const char *locs_no_ops(param_op_type);
  int can_have_methods(int level);
  int is_signed_ate(unsigned char ate) const;
  void dump_location(std::string &s, param_loc &pl);
  uint64_t calc_redudant_locs(const param_loc &pl);

  ElementType current_element_type_;
  int ns_count = 0;

  typedef std::pair<ElementType, const char*> UniqName;
  // string_view is not very effective - it holds 8 bytes for ptr + 8 bytes for length
  typedef std::pair<ElementType, const char *> UniqName2;

  struct Uniq2Comparator {
    bool operator()(const UniqName2 &a, const UniqName2 &b) const {
      if ( a.first < b.first ) return true;
      if ( a.first == b.first ) {
        return 0 < strcmp(a.second, b.second);
      }
      return false;
    }
  };
  struct CSComparator {
   bool operator()(const char *a, const char *b) const
    { return 0 < strcmp(a, b); }
  };

  struct dumped_type {
    ElementType type_;
    const char *name_;
    unsigned char ate_;
    uint64_t id;
  };

  struct Parent {
    uint64_t id;
    size_t offset;
    int access = 0;
    bool virtual_ = false;
  };

  Parent *get_parent(const char *why);

  struct EnumItem {
    const char *name;
    uint64_t value;
  };

  struct ConstValue {
    uint64_t value; // if ptr is not null then this is length
    const unsigned char *ptr;
    ConstValue(uint64_t v):
      value(v)
    { ptr = nullptr; }
    ConstValue(uint64_t v, const unsigned char *data):
      value(v), ptr(data)
    {}
  };

  struct FormalParam {
    const char *name = nullptr;
    uint64_t param_id = 0,
     id = 0;
    unsigned char pdir = 0; // param direction - 2 in 3 out
    bool ellipsis = false,
     var_ = false, // from DW_AT_variable_parameter - go mostly?
     art_ = false, // seems that dwarf5 mark this arg as articial and don`t have DW_AT_object_pointer
     optional_ = false; // DW_AT_is_optional
    param_loc loc;
  };

  FormalParam *get_param(const char *why);

  struct Compound;
  struct NSpace;

  struct Element {
    void move(Element &e)
    {
      owner_ = e.owner_; e.owner_ = nullptr;
      ns_ = e.ns_; e.ns_ = nullptr;
      type_ = e.type_;
      id_ = e.id_;
      level_ = e.level_;
      fname_ = e.fname_;
      fullname_ = std::move(e.fullname_);
      name_ = e.name_;
      link_name_ = e.link_name_;
      size_ = e.size_;
      type_id_ = e.type_id_;
      offset_ = e.offset_;
      count_ = e.count_;
      addr_ = e.addr_;
      align_ = e.align_;
      cont_type_ = e.cont_type_;
      spec_ = e.spec_;
      abs_ = e.abs_;
      locx_ = e.locx_;
      inlined_ = e.inlined_;
      access_ = e.access_;
      bit_size_ = e.bit_size_;
      bit_offset_ = e.bit_offset_;
      addr_class_ = e.addr_class_;
      m_comp = e.m_comp; e.m_comp = nullptr;
      ate_ = e.ate_;
      noret_ = e.noret_;
      decl_ = e.decl_;
      const_expr_ = e.const_expr_;
      has_range_ = e.has_range_;
      enum_class_ = e.enum_class_;
      gnu_vector_ = e.gnu_vector_;
      tensor_ = e.tensor_;
      dumped_ = e.dumped_;
    }
    Element(Element &&e)
    {
      move(e);
    }
    Element& operator=(Element &&e)
    {
      move(e);
      return *this;
    }
    Element& operator=(const Element &) = delete;
    ~Element()
    {
      if ( m_comp != nullptr )
      {
        delete m_comp;
        m_comp = nullptr;
      }
    }
    Element(ElementType type, uint64_t id, int level, Element *o, NSpace *n) :
      owner_(o),
      ns_(n),
      type_(type),
      id_(id),
      level_(level)
    {}
    const char* TypeName() const;
    Element *owner_;
    NSpace *ns_;
    ElementType type_;
    uint64_t id_;
    int level_;
    const char *fname_ = nullptr,
     *name_ = nullptr,
     *link_name_ = nullptr; // set in SetLinkageName
    std::string fullname_; // when -F option was used
    size_t size_ = 0;
    uint64_t type_id_ = 0,
      offset_ = 0,
      count_ = 0,
      addr_ = 0,
      align_ = 0,
      cont_type_ = 0, // for ptr2member
      spec_ = 0,
      abs_ = 0, // for DW_AT_abstract_origin
      locx_ = 0; // offset to debug_loclists section
    int inlined_ = 0,
     access_ = 0,
     bit_size_ = 0,
     bit_offset_ = 0,
     addr_class_ = 0; // from DW_AT_address_class
    Compound *m_comp = nullptr;
    unsigned char ate_ = 0; // DW_AT_encoding
    bool noret_ = false,
     decl_ = false,
     const_expr_ = false,
     has_range_ = false,
     enum_class_ = false,
     gnu_vector_ = false,
     tensor_ = false,
     dumped_ = false;

    inline bool is_abs() const
    {
      return (abs_) && (addr_);
    }
    inline const char *mangled() const
    {
      return link_name_ ? link_name_ : name_;
    }
    inline bool can_have_static_vars() const
    {
      return (type_ == ElementType::class_type) ||
             (type_ == ElementType::interface_type)  ||
             (type_ == ElementType::structure_type)
      ;
    }
    inline bool can_have_decl() const
    {
      return (type_ == ElementType::class_type) ||
             (type_ == ElementType::interface_type)  ||
             (type_ == ElementType::enumerator_type) ||
             (type_ == ElementType::structure_type)  ||
             (type_ == ElementType::union_type)
      ;
    }
    inline bool is_pure_decl() const
    {
      return decl_ && (m_comp == nullptr);
    }
    inline bool has_methods() const
    {
      if ( m_comp == nullptr )
        return false;
      return !m_comp->methods_.empty();
    }
    inline bool has_lvars() const
    {
      if ( m_comp == nullptr )
        return false;
      return !m_comp->lvars_.empty();
    }
    size_t get_rank() const
    {
      if ( m_comp == nullptr )
        return 0;
      return m_comp->members_.size() +
             m_comp->parents_.size() +
             m_comp->enums_.size() +
             m_comp->params_.size() +
             m_comp->methods_.size();
    }
  };

  Element *get_member(const char *why);

  struct Method: public Element
  {
    Method(uint64_t id, int level, Element *o, NSpace *n)
     : Element(method, id, level, o, n)
    {}
    void cpy(Method &e)
    {
      virt_ = e.virt_;
      vtbl_index_ = e.vtbl_index_;
      this_arg_ = e.this_arg_;
      art_ = e.art_;
      def_ = e.def_;
      expl_ = e.expl_;
      ref_ = e.ref_;
      rval_ref_ = e.rval_ref_;
    }
    Method(Method &&e): Element(std::move(e))
    {
      cpy(e);
    }
    Method& operator=(Method &&e)
    {
      Element::operator=(std::move(e));
      cpy(e);
      return *this;
    }
    int virt_ = 0;
    uint64_t vtbl_index_ = 0;
    uint64_t this_arg_ = 0;
    bool art_ = false,  // from DW_AT_artificial - for destructors
     def_ = false,  // DW_AT_defaulted
     expl_ = false, // DW_AT_explicit
     // see details at https://dwarfstd.org/issues/131105.1.html
     ref_ = false,  // DW_AT_reference
     rval_ref_ = false; // DW_AT_rvalue_reference
  };

  struct Compound {
    Compound() = default;
    ~Compound() = default;
    Compound& operator=(Compound &&) = default;
    Compound(Compound &&) = default;

    std::vector<Element> members_;
    std::vector<Parent> parents_;
    std::vector<EnumItem> enums_;
    std::vector<FormalParam> params_;
    std::list<Method> methods_;
    std::list<Element *> lvars_; // local vars with -x option
    std::map<Element *, param_loc> lvar_locs_; // from DecodeAddrLocation when -x option was used
  };

  int check_dumped_type(Element&);
  Element *get_owner();
  Element *get_top_func() const;
  int should_keep(Element *);
  int exclude_types(ElementType et, Element &);
  // per compilation unit data
  bool m_hdr_dumped = false,
   sub_filtered = false;
  Element *last_var_ = nullptr;
  Element *recent_ = nullptr;
  std::stack<Element *> m_stack;
  std::stack<NSpace *> ns_stack;
  std::list<Element> elements_;
  std::map<uint64_t, dumped_type> m_replaced;
  // values for const_expr - cleared for each compilation unit if option -g not used
  std::map<Element *, uint64_t> m_lvalues;

  // ranges. 2 maps need bcs there may be 2 sections:
  // debug_rnglists - self-contained and parsed in parse_rnglists
  // debug_ranges - need address_size (from CU) + base (also CU specific)
  struct buggy_rng {
    unsigned long off;
    unsigned long base;
    unsigned char addr_size;
  };
  // in both keys are tag_id
  std::map<uint64_t, unsigned long> m_rng2;
  std::map<uint64_t, buggy_rng> m_rng;
  bool lookup_range(uint64_t, std::list<std::pair<uint64_t, uint64_t> > &);

  struct NSpace {
   Element *ns_el_ = nullptr; // to get name - in ns_el->name_, for root - null
   NSpace *parent_ = nullptr; // chains of namespaces, for root - null
   std::map<const char *, NSpace *, CSComparator> nested;
   // already dumped types
   std::map<UniqName, std::pair<uint64_t, size_t> > m_dumped_db;
   std::map<UniqName2, std::pair<uint64_t, size_t>, Uniq2Comparator> m_dumped_db2;
   bool empty = true;
  };
  NSpace ns_root;
  void clear_namespaces(std::map<const char *, NSpace *, CSComparator> &m)
  {
    for ( auto mi: m )
    {
      clear_namespaces(mi.second->nested);
      delete mi.second;
    }
  }
  inline NSpace *top_ns()
  {
    if ( ns_stack.empty() ) return &ns_root;
    return ns_stack.top();
  }
  // go names - actually this is only for backward refs, for forward use -g option
  std::map<uint64_t, const char *> m_go_types;
  std::map<uint64_t, go_ext_attr>  m_go_attrs;
  // tls indexes
  std::map<uint64_t, int> m_tls;
};