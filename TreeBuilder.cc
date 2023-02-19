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
    result += elements_[i].GenerateJson(this);
    result += ",\n";
  }

  return result;
}

// add dumped types to m_dumped_db
int TreeBuilder::merge_dumped()
{
  if ( elements_.empty() )
    return 0;
  int res = 0;
  for ( auto &e: elements_ )
  {
    if ( !e.name_ )
      continue; // and type must be named
//  fprintf(g_outf, "dumped type %d with name %s level %d\n", e.type_, e.name_, e.level_);
    if ( e.level_ > 1 )
      continue; // we heed only high-level types definitions
    UniqName key { e.type_, e.name_};
    auto added = m_dumped_db.find(key);
    if ( added != m_dumped_db.end() )
      continue; // this type already in m_dumped_db
    m_dumped_db[key] = e.id_;
    res++; 
  }
  return res;
}

int TreeBuilder::check_dumped_type(const char *name)
{
  UniqName key { current_element_type_, name };
  const auto ci = m_dumped_db.find(key);
  if ( ci == m_dumped_db.cend() )
    return 0;
// fprintf(g_outf, "found type %d with name %s\n", key.first, key.second);    
  // put fake type into m_replaced
  // don`t replace functions
  if ( current_element_type_ != subroutine )
  {
    dumped_type dt { current_element_type_, name, ci->second };
    m_replaced[elements_.back().id_] = dt;
  }
  // remove current element
  elements_.pop_back();
  return 1;
}

uint64_t TreeBuilder::get_replaced_type(uint64_t id) const
{
  const auto ci = m_replaced.find(id);
  if ( ci == m_replaced.cend() )
    return id;
  return ci->second.id;
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

  merge_dumped();
  elements_.clear();
  m_replaced.clear();
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

// formal parameter - level should be +1 to parent
bool TreeBuilder::AddFormalParam(uint64_t tag_id, int level) {
  current_element_type_ = ElementType::formal_param;
  if (!elements_.size()) {
    fprintf(stderr, "Can't add a formal parameter if the element list is empty\n");
    return false;
  }
  if ( m_stack.empty() ) {
    fprintf(stderr, "Can't add a formal parameter when stack is empty\n");
    return false;
  }
//  fprintf(g_outf, "f level %d level %d\n", m_stack.top()->level_, level);
  if ( m_stack.top()->level_ != level - 1 )
    return false;
  m_stack.top()->params_.push_back({NULL, 0});
  return true;
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
    // Subrange
    case ElementType::subrange_type:
      break;                          // Just update the current element type

    // part of enum - check that parent is 
    case ElementType::enumerator:
      if (current_element_type_ == ElementType::none) {
        return;
      }  
      if (!elements_.size()) {
        fprintf(stderr, "Can't add a enumerator if the element list is empty\n");
        return;
      }
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't add a enumerator when stack is empty\n");
        return;
      }
      m_stack.top()->enums_.push_back({NULL, 0});
      break;

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

  if ( current_element_type_ == ElementType::formal_param )
  {
    if ( m_stack.empty() ) {
      fprintf(stderr, "Can't set the formal param name when stack is empty\n");
      return;
    }
    if (!m_stack.top()->params_.size()) {
      fprintf(stderr, "Can't set the formal param name if the params list is empty\n");
      return;
    }

    m_stack.top()->params_.back().name = name;
    return;    
  }
  if ( current_element_type_ == ElementType::enumerator )
  {
    if ( m_stack.empty() ) {
      fprintf(stderr, "Can't set the enum name when stack is empty\n");
      return;
    }
    if (!m_stack.top()->enums_.size()) {
      fprintf(stderr, "Can't set the enum name if the enums list is empty\n");
      return;
    }

    m_stack.top()->enums_.back().name = name;
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
    case ElementType::formal_param:
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't set the formal param type when stack is empty\n");
        return;
      }
      if (!m_stack.top()->params_.size()) {
        fprintf(stderr, "Can't set the formal param type if the members list is empty\n");
        break;
      }
      m_stack.top()->params_.back().id = type_id;
      break;
    case ElementType::subrange_type:
      break; // do nothing
    default:
      elements_.back().type_id_ = type_id;
      break;
  }
}

void TreeBuilder::SetConstValue(uint64_t count) {
  if (current_element_type_ != ElementType::enumerator) {
    return;
  }
  if ( m_stack.empty() ) {
    fprintf(stderr, "Can't set ConstValue when stack is empty\n");
    return;
  }
  if (!m_stack.top()->enums_.size()) {
    fprintf(stderr, "Can't set ConstValue if the enums list is empty\n");
    return;
  }
  m_stack.top()->enums_.back().value = count;
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
    case ElementType::subroutine_type: return "function_type";
    case ElementType::subroutine: return "function";
    default: return "unk";
  }
}

std::string TreeBuilder::Element::GenerateJson(TreeBuilder *tb) {
  std::string result;

  // A member is a special case
  if (type_ == ElementType::member) {
    result = "{";

    if (type_id_) {
      result += "\"type_id\":\""+std::to_string(tb->get_replaced_type(type_id_))+"\",";
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
      result += "{\"id\":\""+std::to_string(tb->get_replaced_type(parents_[i].id))+"\",";
      result += "\"offset\":"+std::to_string(parents_[i].offset)+"}";
      if (i+1 < parents_.size()) {
        result += ",";
      }
    }
    result += "],";
  }
  if ( params_.size() > 0 )
  {
    result += "\"params\":[";
    for (size_t i = 0; i < params_.size(); i++) {
      if ( params_[i].name )
        result += "{\"name\":\""+EscapeJsonString(params_[i].name)+"\",";
      else
        result += "{";
      result += "\"type_id\":"+std::to_string(params_[i].id)+"}";
      if (i+1 < params_.size()) {
        result += ",";
      }
    }
    result += "],";
  }
  if ( enums_.size() > 0 )
  {
    result += "\"enums\":[";
    for (size_t i = 0; i < enums_.size(); i++) {
      result += "{\"name\":\""+EscapeJsonString(enums_[i].name)+"\",";
      result += "\"value\":"+std::to_string(enums_[i].value)+"}";
      if (i+1 < enums_.size()) {
        result += ",";
      }
    }
    result += "],";
  }
  if (members_.size() > 0) {
    result += "\"members\":[";
    for (size_t i = 0; i < members_.size(); i++) {
      result += members_[i].GenerateJson(tb);
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