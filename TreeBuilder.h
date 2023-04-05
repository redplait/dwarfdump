#pragma once
#include <string>
#include <map>
#include <list>
#include <vector>
#include <stack>
#include "regnames.h"

extern int g_opt_g, g_opt_k, g_opt_l, g_opt_s, g_opt_v;
extern FILE *g_outf;

enum param_op_type
{
  reg = 1,
  breg,
  fbreg,
  deref,
  call_frame_cfa,
  plus_uconst,
  tls_index,
};

struct one_param_loc
{
  enum param_op_type type;
  unsigned int idx;
  int offset;
};

struct param_loc
{
  std::list<one_param_loc> locs;
  uint64_t loc_off = 0; // if non-zero - offset into .debug_loc section
  inline bool empty() const
  {
    return locs.empty() && !loc_off;
  }
  inline bool is_tls() const
  {
    return 1 == locs.size() && locs.front().type == tls_index;
  }
};

struct cu
{
  const char *cu_name;
  const char *cu_comp_dir;
  const char *cu_producer;
  int cu_lang;
};

class TreeBuilder {
public:
  TreeBuilder();
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
  inline bool is_enum() const
  {
    return current_element_type_ == enumerator;
  }
  inline bool is_formal_param() const
  {
    return current_element_type_ == formal_param;
  }
  void ProcessUnit(int last = 0);
  int add2stack();
  void pop_stack(uint64_t);
  void AddNone();
  void AddElement(ElementType element_type, uint64_t tag_id, int level);
  bool AddVariant();
  bool AddFormalParam(uint64_t tag_id, int level, bool);
  void SetElementName(const char* name, uint64_t off);
  void SetLinkageName(const char* name);
  void SetElementSize(uint64_t size);
  void SetElementOffset(uint64_t offset);
  void SetElementType(uint64_t type_id);
  void SetElementCount(uint64_t count);
  void SetConstValue(uint64_t count);
  void SetAlignment(uint64_t);
  void SetAddr(uint64_t);
  void SetContainingType(uint64_t);
  void SetBitSize(int);
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
  void SetSpec(uint64_t);
  void SetAbs(uint64_t);
  void SetDiscr(uint64_t);
  void SetInlined(int);
  void SetVarParam(bool);
  void SetLocation(param_loc *);
  void SetTlsIndex(param_loc *);

  uint64_t get_replaced_type(uint64_t) const;
  int check_dumped_type(const char *);
  void collect_go_types();
  // renderer methods
  bool get_replaced_name(uint64_t, std::string &);
  // compilation unit data
  struct cu cu;
  bool is_go() const;
  RegNames *m_rnames = nullptr;
  ISectionNames *m_snames = nullptr;
  // for names with direct string - seems that if name lesser pointer size they are directed
  // so renderer should be able to distinguish if some name located in string pool
  // in other case this name should be considered as direct string
  const unsigned char *debug_str_;
  size_t debug_str_size_;
  inline bool in_string_pool(const char *s)
  {
    return (s >= (const char *)debug_str_) && (s < (const char *)debug_str_ + debug_str_size_);
  }
protected:
  virtual void RenderUnit(int last)
  {}
  void put_file_hdr();
  void put_file_hdr(struct cu *);
  int merge_dumped();
  int can_have_methods(int level);

  ElementType current_element_type_;
  int ns_count = 0;
  
  typedef std::pair<ElementType, const char*> UniqName;
  typedef std::pair<ElementType, std::string> UniqName2;
  struct dumped_type {
    ElementType type_;
    const char *name_;
    uint64_t id;
  };

  struct Parent {
    uint64_t id;
    size_t offset;
    int access = 0;
  };

  struct EnumItem {
    const char *name;
    uint64_t value;
  };

  struct FormalParam {
    const char *name;
    uint64_t param_id;
    uint64_t id;
    bool ellipsis;
    bool var_; // from DW_AT_variable_parameter - go mostly?
    param_loc loc;
  };

  struct Compound;

  struct Element {
    void move(Element &e)
    {
      owner_ = e.owner_;
      e.owner_ = nullptr;
      type_ = e.type_;
      id_ = e.id_;
      level_ = e.level_;
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
      inlined_ = e.inlined_;
      access_ = e.access_;
      bit_size_ = e.bit_size_;
      bit_offset_ = e.bit_offset_;
      m_comp = e.m_comp;
      e.m_comp = nullptr;
      noret_ = e.noret_;
      decl_ = e.decl_;
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
    Element(ElementType type, uint64_t id, int level, Element *o) :
      owner_(o), 
      type_(type), 
      id_(id),
      level_(level),
      name_(nullptr),
      link_name_(nullptr),
      size_(0), 
      type_id_(0), 
      offset_(0), 
      count_(0),
      addr_(0),
      align_(0),
      cont_type_(0),
      spec_(0),
      abs_(0),
      inlined_(0),
      access_(0),
      bit_size_(0),
      bit_offset_(0),
      m_comp(nullptr),
      noret_(false),
      decl_(false),
      dumped_(false)
    {}
    const char* TypeName();
    Element *owner_;
    ElementType type_;
    uint64_t id_;
    int level_;
    const char* name_;
    const char *link_name_;
    size_t size_;
    uint64_t type_id_;
    uint64_t offset_;
    uint64_t count_;
    uint64_t addr_;
    uint64_t align_;
    uint64_t cont_type_; // for ptr2member
    uint64_t spec_;
    uint64_t abs_; // for DW_AT_abstract_origin
    int inlined_;
    int access_;
    int bit_size_;
    int bit_offset_;
    Compound *m_comp;
    bool noret_;
    bool decl_;
    bool dumped_;

    inline bool is_abs() const
    {
      return (abs_) && (addr_);
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

  struct Method: public Element
  {
    Method(uint64_t id, int level, Element *o)
     : Element(method, id, level, o),
       virt_(0),
       vtbl_index_(0),
       this_arg_(0),
       art_(false),
       def_(false),
       expl_(false)
    {}
    void cpy(Method &e)
    {
      virt_ = e.virt_;
      vtbl_index_ = e.vtbl_index_;
      this_arg_ = e.this_arg_;
      art_ = e.art_;
      def_ = e.def_;
      expl_ = e.expl_;
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
    int virt_;
    uint64_t vtbl_index_;
    uint64_t this_arg_;
    bool art_;  // from DW_AT_artificial - for destructors
    bool def_;  // DW_AT_defaulted
    bool expl_; // DW_AT_explicit
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
  };

  Element *get_owner();
  int should_keep(Element *);
  // per compilation unit data
  bool m_hdr_dumped;
  Element *recent_ = nullptr;
  std::stack<Element *> m_stack;
  std::list<Element> elements_;
  std::map<uint64_t, dumped_type> m_replaced;

  // already dumped types
  std::map<UniqName, std::pair<uint64_t, size_t> > m_dumped_db;
  std::map<UniqName2, std::pair<uint64_t, size_t> > m_dumped_db2;
  // go names
  std::map<uint64_t, const char *> m_go_types;
  // tls indexes
  std::map<uint64_t, int> m_tls;
};