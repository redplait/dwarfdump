#pragma once
#include <string>
#include <map>
#include <vector>

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
    member,
    pointer_type,
    structure_type,
    typedef2,
    union_type,
    inheritance,
    subrange_type,
    base_type,
    const_type
  };
  void AddNone();
  void AddElement(ElementType element_type, uint64_t tag_id, int level);
  void SetElementName(const char* name);
  void SetElementSize(uint64_t size);
  void SetElementOffset(uint64_t offset);
  void SetElementType(uint64_t type_id);
  void SetElementCount(uint64_t count);

private:
  static std::string EscapeJsonString(const char* str);

  ElementType current_element_type_;
  ElementType previous_element_type_;

  struct Parent {
    uint64_t id;
    size_t offset;
  };

  class Element {
  public:
    Element(ElementType type, uint64_t id, int level) : 
      type_(type), 
      id_(id),
      level_(level),
      name_(nullptr), 
      size_(0), 
      type_id_(0), 
      offset_(0), 
      count_(0) 
    {}
    const char* TypeName();
    std::string GenerateJson();
    ElementType type_;
    uint64_t id_;
    int level_;
    const char* name_;
    size_t size_;
    uint64_t type_id_;
    uint64_t offset_;
    uint64_t count_;
    std::vector<Element> members_;
    std::vector<Parent> parents_;
  };

  std::vector<Element> elements_;
};