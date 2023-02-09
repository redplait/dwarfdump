#include "TreeBuilder.h"

extern int g_opt_l, g_opt_v;
extern FILE *g_outf;

TreeBuilder::TreeBuilder() = default;
TreeBuilder::~TreeBuilder() = default;

std::string TreeBuilder::GenerateJson() {
  std::string result;
  if ( elements_.empty() )
    return result;
  for (size_t i = 0; i < elements_.size(); i++) {
    result += elements_[i].GenerateJson();
    result += ",\n";
  }

  return result;
}

// called on each processed compilation unit
void TreeBuilder::ProcessUnit(int last)
{
  if ( !m_stack.empty() )
  {
    // fprintf(stderr, "ProcessUnit: stack is not empty\n");
    m_stack = {};
  }
  if ( g_opt_v && !elements_.empty() )
  {
    if ( cu_name )
      fprintf(g_outf, "\n// Name: %s\n", cu_name);
    if ( cu_comp_dir )
      fprintf(g_outf, "// CompDir: %s\n", cu_comp_dir);
    if ( cu_producer )
      fprintf(g_outf, "// Producer: %s\n", cu_producer);
  }
  auto json = GenerateJson();
  // if this was last unit - cut final comma
  if ( last )
  {
    json.pop_back();
    json.pop_back();
  }
  if ( !json.empty() )
    fprintf(g_outf, "%s", json.c_str());
  elements_.clear();
  cu_name = cu_comp_dir = cu_producer = NULL;
}

void TreeBuilder::AddNone() {
  current_element_type_ = ElementType::none; 
}

int TreeBuilder::add2stack()
{
  if (!elements_.size()) {
    // fprintf(stderr, "Can't add a member if the element list is empty\n");
    return 0;
  }
  m_stack.push( &elements_.back() );
  return 1;
}

void TreeBuilder::pop_stack()
{
  if ( m_stack.empty() ) {
    // fprintf(stderr, "stack is empty\n");
    return;
  }
  m_stack.pop();
}

void TreeBuilder::AddElement(ElementType element_type, uint64_t tag_id, int level) {
  switch(element_type) {
    case ElementType::member:       // Member
      if (current_element_type_ == ElementType::none) {
        return;
      }
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't add a member when stack is empty\n");
        return;
      }
      if (!elements_.size()) {
        fprintf(stderr, "Can't add a member if the element list is empty\n");
        return;
      }
      m_stack.top()->members_.push_back(Element(element_type, tag_id, level));
      break;
    case ElementType::inheritance:    // Parent
      if (current_element_type_ == ElementType::none) {
        return;
      }  
      if (!elements_.size()) {
        fprintf(stderr, "Can't add a parent if the element list is empty\n");
        return;
      }
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't add a parent when stack is empty\n");
        return;
      }
      m_stack.top()->parents_.push_back({tag_id, 0});
      break;
    case ElementType::subrange_type:  // Subrange
      break;                          // Just update the current element type
    default:
      elements_.push_back(Element(element_type, tag_id, level));
  }

  current_element_type_ = element_type; 
}

void TreeBuilder::SetElementName(const char* name) {
  if (current_element_type_ == ElementType::none) {
    return;
  }
  if (!elements_.size()) {
    fprintf(stderr, "Can't set an element name if the element list is empty\n");
    return;
  }

  if (current_element_type_ == ElementType::member) {
    if ( m_stack.empty() ) {
      fprintf(stderr, "Can't set the member name when stack is empty\n");
      return;
    }
    if (!m_stack.top()->members_.size()) {
      fprintf(stderr, "Can't set the member name if the members list is empty\n");
      return;
    }

    m_stack.top()->members_.back().name_ = name;
    return;
  }

  elements_.back().name_ = name;
}

void TreeBuilder::SetElementSize(uint64_t size) {
   if (current_element_type_ == ElementType::none) {
    return;
  }
  if (!elements_.size()) {
    fprintf(stderr, "Can't set an element size if the element list is "
      "empty\n");
    return;
  }

  if (current_element_type_ == ElementType::member) {
    if ( m_stack.empty() ) {
      fprintf(stderr, "Can't set an member size when stack is empty\n");
      return;
    }
    if (!m_stack.top()->members_.size()) {
      fprintf(stderr, "Can't set the member size if the members list is "
        "empty\n");
      return;
    }

    m_stack.top()->members_.back().size_ = size;
    return;
  }

  elements_.back().size_ = size; 
}

void TreeBuilder::SetElementOffset(uint64_t offset) {
   if (current_element_type_ == ElementType::none) {
    return;
  }
  if (!elements_.size()) {
    fprintf(stderr, "Can't set an element offset if the element list is "
      "empty\n");
    return;
  }

  switch (current_element_type_) {
    case ElementType::member:
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't set the member offset when stack is empty\n");
        return;
      }
      if (!m_stack.top()->members_.size()) {
        fprintf(stderr, "Can't set the member offset if the members list is empty\n");
        break;
      }
      m_stack.top()->members_.back().offset_ = offset;
      break;
    case ElementType::inheritance:
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't set the parent offset when stack is empty\n");
        return;
      }
      if (!m_stack.top()->parents_.size()) {
        fprintf(stderr, "Can't set the parent offset if the parents list is empty\n");
        break;
      }
      m_stack.top()->parents_.back().offset = offset;
      break;
    default:
      break;
  }
}

void TreeBuilder::SetElementType(uint64_t type_id) {
  if (current_element_type_ == ElementType::none) {
    return;
  }
  if (!elements_.size()) {
    fprintf(stderr, "Can't set an element type if the element list is empty\n");
    return;
  }

  switch (current_element_type_) {
    case ElementType::member:
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't set the member type when stack is empty\n");
        return;
      }
      if (!m_stack.top()->members_.size()) {
        fprintf(stderr, "Can't set the member type if the members list is empty\n");
        break;
      }
      m_stack.top()->members_.back().type_id_ = type_id;
      break;
    case ElementType::inheritance:
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't set the parent type when stack is empty\n");
        return;
      }
      if (!m_stack.top()->parents_.size()) {
        fprintf(stderr, "Can't set the parent type if the parents list is empty\n");
        break;
      }
      m_stack.top()->parents_.back().id = type_id;
      break;
    case ElementType::subrange_type:
      break; // do nothing
    default:
      elements_.back().type_id_ = type_id;
      break;
  }
}

void TreeBuilder::SetElementCount(uint64_t count) {
  if (current_element_type_ != ElementType::subrange_type) {
    return;
  }
  if (!elements_.size()) {
    fprintf(stderr, "Can't set an element count if the element list is"
      " empty\n");
    return;
  }
  elements_.back().count_ = count;
}

// static
std::string TreeBuilder::EscapeJsonString(const char* str) {
  std::string result;
  size_t str_len = std::char_traits<char>::length(str); 

  for (size_t i = 0; i < str_len; i++) {
    if (str[i] == '\\' || str[i] == '"') {
      result += '\\';
    }

    result += str[i];
  }

  return result;
}

const char* TreeBuilder::Element::TypeName() {
  switch (type_) {
    case ElementType::none: return "none";
    case ElementType::array_type: return "array";
    case ElementType::class_type: return "class";
    case ElementType::enumerator_type: return "enumerator";
    case ElementType::member: return "member";
    case ElementType::pointer_type: return "pointer";
    case ElementType::structure_type: return "structure";
    case ElementType::typedef2: return "typedef";
    case ElementType::union_type: return "union";
    case ElementType::inheritance: return "inheritance";
    case ElementType::base_type: return "base";
    case ElementType::const_type: return "const";
    default: return "unk";
  }
}

std::string TreeBuilder::Element::GenerateJson() {
  std::string result;

  // A member is a special case
  if (type_ == ElementType::member) {
    result = "{";

    if (type_id_) {
      result += "\"type_id\":\""+std::to_string(type_id_)+"\",";
    }
    if (name_) {
      result += "\"name\":\""+EscapeJsonString(name_)+"\",";
    }
    if ( level_ && g_opt_l ) {
      result += "\"level\":\""+std::to_string(level_)+"\",";
    }
    result += "\"offset\":"+std::to_string(offset_);

    result += "}";
    return result;
  }

  // The others are generic
  result += "\""+std::to_string(id_)+"\":";
  result += "{\"type\":\""+std::string(TypeName())+"\",";
  if (type_id_) {
    result += "\"type_id\":\""+std::to_string(type_id_)+"\",";
  }
  if (name_) {
    result += "\"name\":\""+EscapeJsonString(name_)+"\",";
  }
  if (size_) {
    result += "\"size\":"+std::to_string(size_)+",";
  }
  if (count_) {
    result += "\"count\":"+std::to_string(count_)+",";
  }
  if (parents_.size() > 0) {
    result += "\"parents\":[";
    for (size_t i = 0; i < parents_.size(); i++) {
      result += "{\"id\":\""+std::to_string(parents_[i].id)+"\",";
      result += "\"offset\":"+std::to_string(parents_[i].offset)+"}";
      if (i+1 < parents_.size()) {
        result += ",";
      }
    }
    result += "],";
  }
  if (members_.size() > 0) {
    result += "\"members\":[";
    for (size_t i = 0; i < members_.size(); i++) {
      result += members_[i].GenerateJson();
      if (i+1 < members_.size()) {
        result += ",";
      }
    }
    result += "],";
  }

  if (result.back() == ',') {
    result.pop_back();
  }
  result += "}";
  return result;
}