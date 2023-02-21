#pragma once
#include <string>
#include <map>
#include <vector>
#include <stack>

class TreeBuilder {
public:
  TreeBuilder();
  ~TreeBuilder();
  std::string GenerateJson();

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
    subroutine_type,
    formal_param,
    subroutine,
  };
  void ProcessUnit(int last = 0);
  int add2stack();
  void pop_stack();
  void AddNone();
  void AddElement(ElementType element_type, uint64_t tag_id, int level);
  bool AddFormalParam(uint64_t tag_id, int level);
  void SetElementName(const char* name);
  void SetLinkageName(const char* name);
  void SetElementSize(uint64_t size);
  void SetElementOffset(uint64_t offset);
  void SetElementType(uint64_t type_id);
  void SetElementCount(uint64_t count);
  void SetConstValue(uint64_t count);
  void SetAddr(uint64_t count);

  uint64_t get_replaced_type(uint64_t) const;
  int check_dumped_type(const char *);
  // renderer methods
  bool get_replaced_name(uint64_t, std::string &);
  // compilation unit data
  const char *cu_name;
  const char *cu_comp_dir;
  const char *cu_producer;
private:
  static std::string EscapeJsonString(const char* str);
  int merge_dumped();
  void dump_types();

  ElementType current_element_type_;
  ElementType previous_element_type_;

  typedef std::pair<ElementType, const char*> UniqName;
  struct dumped_type {
    ElementType type_;
    const char *name_;
    uint64_t id;
  };

  struct Parent {
    uint64_t id;
    size_t offset;
  };

  struct EnumItem {
    const char *name;
    uint64_t value;
  };

  struct FormalParam {
    const char *name;
    uint64_t id;
  };

  class Element {
  public:
    Element(ElementType type, uint64_t id, int level) : 
      type_(type), 
      id_(id),
      level_(level),
      name_(nullptr),
      link_name_(nullptr),
      size_(0), 
      type_id_(0), 
      offset_(0), 
      count_(0),
      addr_(0)
    {}
    const char* TypeName();
    std::string GenerateJson(TreeBuilder *tb);
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
    std::vector<Element> members_;
    std::vector<Parent> parents_;
    std::vector<EnumItem> enums_;
    std::vector<FormalParam> params_;
  };

  void dump_enums(Element *);
  void dump_fields(Element *);
  void dump_func(Element *);
  bool dump_type(uint64_t, std::string &);

  // per compilation unit data
  std::stack<Element *> m_stack;
  std::vector<Element> elements_;
  std::map<uint64_t, dumped_type> m_replaced;
  std::map<uint64_t, Element *> m_els;

  // already dumped types
  std::map<UniqName, uint64_t> m_dumped_db;
};