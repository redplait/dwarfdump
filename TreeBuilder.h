#pragma once
#include <string>
#include <map>
#include <list>
#include <vector>
#include <stack>

extern int g_opt_l, g_opt_v;
extern FILE *g_outf;

class TreeBuilder {
public:
  TreeBuilder();
  virtual ~TreeBuilder();

  enum ElementType {
    none,
    array_type,
    class_type,
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
    const_type,
    volatile_type,
    restrict_type,
    reference_type,
    rvalue_ref_type,
    subroutine_type,
    formal_param,
    subroutine,
    method,
    ptr2member,
    ns_start,
    ns_end,
  };
  void ProcessUnit(int last = 0);
  int add2stack();
  void pop_stack(uint64_t);
  void AddNone();
  void AddElement(ElementType element_type, uint64_t tag_id, int level);
  bool AddFormalParam(uint64_t tag_id, int level, bool);
  void SetElementName(const char* name);
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
  void SetSpec(uint64_t);
  void SetAbs(uint64_t);
  void SetInlined(int);

  uint64_t get_replaced_type(uint64_t) const;
  int check_dumped_type(const char *);
  // renderer methods
  bool get_replaced_name(uint64_t, std::string &);
  // compilation unit data
  const char *cu_name;
  const char *cu_comp_dir;
  const char *cu_producer;
  int cu_lang;
  // for names with direct string - seems that if name lesser pointer size they are directed
  // so renderer should be able to distinguish if some name located in string pool
  // in other case this name should be considered as direct string
  const char *debug_str_;
  size_t debug_str_size_;
  inline bool in_string_pool(const char *s)
  {
    return (s >= debug_str_) && (s < debug_str_ + debug_str_size_);
  }
protected:
  virtual void RenderUnit(int last)
  {}
  void put_file_hdr();
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
      decl_(false)
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

    inline bool is_abs() const
    {
      return (abs_) && (addr_);
    }
    inline bool can_have_decl() const
    {
      return (type_ == ElementType::class_type) ||
             (type_ == ElementType::enumerator_type) ||
             (type_ == ElementType::structure_type) ||
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
  };

  struct Method: public Element
  {
    Method(uint64_t id, int level, Element *o)
     : Element(method, id, level, o),
       virt_(0),
       vtbl_index_(0),
       this_arg_(0),
       art_(false),
       def_(false)
    {}
    int virt_;
    uint64_t vtbl_index_;
    uint64_t this_arg_;
    bool art_; // from DW_AT_artificial - for destructors
    bool def_; // DW_AT_defaulted
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
  // per compilation unit data
  bool m_hdr_dumped;
  Element *recent_ = nullptr;
  std::stack<Element *> m_stack;
  std::list<Element> elements_;
  std::map<uint64_t, dumped_type> m_replaced;

  // already dumped types
  std::map<UniqName, uint64_t> m_dumped_db;
  std::map<UniqName2, uint64_t> m_dumped_db2;
};