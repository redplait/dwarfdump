#include "TreeBuilder.h"

extern int g_opt_j, g_opt_l, g_opt_v;
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
    if ( in_string_pool(e.name_) )
    {
      UniqName key { e.type_, e.name_};
      auto added = m_dumped_db.find(key);
      if ( added != m_dumped_db.end() )
        continue; // this type already in m_dumped_db
      m_dumped_db[key] = e.id_;
    } else {
      UniqName2 key { e.type_, e.name_};
      auto added = m_dumped_db2.find(key);
      if ( added != m_dumped_db2.end() )
        continue; // this type already in m_dumped_db2
      m_dumped_db2[key] = e.id_;
    }
    res++; 
  }
  return res;
}

bool TreeBuilder::get_replaced_name(uint64_t key, std::string &res)
{
   const auto ci = m_replaced.find(key);
   if ( ci == m_replaced.end() )
     return false;
   switch(ci->second.type_)
   {
     case ElementType::typedef2:
     case ElementType::class_type:
     case ElementType::base_type:
       res = ci->second.name_;
       return true;
     case ElementType::enumerator_type:
       res = "enum ";
       res += ci->second.name_;
       return true;
     case ElementType::structure_type:
       res = "struct ";
       res += ci->second.name_;
       return true;
     case ElementType::union_type:
       res = "union ";
       res += ci->second.name_;
       return true;
     default:
       return false;
   }
}

bool TreeBuilder::dump_type(uint64_t key, std::string &res)
{
  if ( get_replaced_name(key, res) )
    return true;
  if ( !key )
  {
    res = "void";
    return true;
  }
  auto el = m_els.find(key);
  if ( el == m_els.end() )
  {
    res = "missed type ";
    res += std::to_string(key);
    return true;
  }
  if ( el->second->type_ == ElementType::typedef2 ||
       el->second->type_ == ElementType::base_type
     )
  {
    res = el->second->name_;
    return true;
  }
  if ( el->second->type_ == ElementType::structure_type )
  {
    res = "struct ";
    if ( el->second->name_ )
      res += el->second->name_;
    return true;
  }
  if ( el->second->type_ == ElementType::union_type )
  {
    res = "union ";
    if ( el->second->name_ )
      res += el->second->name_;
    return true;
  }
  if ( el->second->type_ == ElementType::enumerator_type )
  {
    res = "enum ";
    if ( el->second->name_ )
      res += el->second->name_;
    return true;
  }
  if ( el->second->type_ == ElementType::pointer_type )
  {
    std::string tmp;
    dump_type(el->second->type_id_, tmp);
    res = tmp;
    res += "*";
    return true;
  }
  if ( el->second->type_ == ElementType::volatile_type )
  {
    std::string tmp;
    dump_type(el->second->type_id_, tmp);
    res = "volatile ";
    res += tmp;
    return true;
  }
  if ( el->second->type_ == ElementType::restrict_type )
  {
    std::string tmp;
    dump_type(el->second->type_id_, tmp);
    res = "restrict ";
    res += tmp;
    return true;
  }
  if ( el->second->type_ == ElementType::reference_type )
  {
    std::string tmp;
    dump_type(el->second->type_id_, tmp);
    res = tmp;
    res += "&";
    return true;
  }
  if ( el->second->type_ == ElementType::const_type )
  {
    std::string tmp;
    dump_type(el->second->type_id_, tmp);
    res = "const ";
    res += tmp;
    return true;
  }
  if ( el->second->type_ == ElementType::array_type )
  {
    dump_type(el->second->type_id_, res);
    res += "[";
    res += std::to_string(el->second->count_);
    res += "]";
    return true;
  }
  res = "dump_type";
  res += std::to_string(el->second->type_);
  return false;
}

void TreeBuilder::dump_enums(Element *e)
{
  for ( auto &en: e->enums_ )
  {
    if ( en.value >= 0 && en.value < 10 )
      fprintf(g_outf, "  %s = %d,\n", en.name, (int)en.value );
    else
      fprintf(g_outf, "  %s = 0x%lX,\n", en.name, en.value );
  }
}

void TreeBuilder::dump_fields(Element *e)
{
  for ( auto &en: e->members_ )
  {
    std::string tmp;
    fprintf(g_outf, "// Offset 0x%lX\n", en.offset_);
    dump_type(en.type_id_, tmp);
    fprintf(g_outf, "%s %s;\n", tmp.c_str(), en.name_);  
  }
}

void TreeBuilder::dump_func(Element *e)
{
  std::string tmp;
  if ( e->type_id_ )
    dump_type(e->type_id_, tmp);
  else
    tmp = "void";
  fprintf(g_outf, "%s %s(", tmp.c_str(), e->name_);
  if ( e->params_.empty() )
    fprintf(g_outf, "void");
  else {
    for ( size_t i = 0; i < e->params_.size(); i++ )
    {
      tmp.clear();
      dump_type(e->params_[i].id, tmp);
      if ( e->params_[i].name )
        fprintf(g_outf, "%s %s", tmp.c_str(), e->params_[i].name);
      else
        fprintf(g_outf, "%s", tmp.c_str());
      if ( i+1 < e->params_.size() )
        fprintf(g_outf, ",");
    }
  }
  fprintf(g_outf, ")");
}

void TreeBuilder::dump_types()
{
  for ( auto &e: elements_ )
  {
    if ( !e.name_ )
      continue;
    if ( e.level_ > 1 )
      continue;
    // skip base types
    if ( ElementType::base_type == e.type_ )
      continue;
    if ( ElementType::const_type == e.type_ )
      continue;
    // skip replaced types
    const auto ci = m_replaced.find(e.id_);
    if ( ci != m_replaced.end() )
      continue;
    if ( e.addr_ )
      fprintf(g_outf, "// 0x%lX\n", e.addr_);
    if ( e.size_ )
      fprintf(g_outf, "// Size 0x%lX\n", e.size_);
    if ( g_opt_v )
      fprintf(g_outf, "// TypeId %lX\n", e.id_);
    if ( e.link_name_ && e.link_name_ != e.name_ )
      fprintf(g_outf, "// LinkageName: %s\n", e.link_name_);
    switch(e.type_)
    {
      case ElementType::enumerator_type:
        fprintf(g_outf, "enum %s {\n", e.name_);
        dump_enums(&e);
        fprintf(g_outf, "}");
        break;
      case ElementType::structure_type:
        fprintf(g_outf, "struct %s {\n", e.name_);
        dump_fields(&e);
        fprintf(g_outf, "}");
        break;
      case ElementType::union_type:
        fprintf(g_outf, "union %s {\n", e.name_);
        dump_fields(&e);
        fprintf(g_outf, "}");
        break;
      case ElementType::subroutine:
        dump_func(&e);
        break;
      case ElementType::typedef2:
        {
          std::string tname;
          dump_type(e.type_id_, tname);
          fprintf(g_outf, "typedef %s %s", tname.c_str(), e.name_);
          break;
        }
      default:
        fprintf(g_outf, "unknown type %d name %s", e.type_, e.name_);
    }
    fprintf(g_outf, ";\n\n");
  }
}

int TreeBuilder::check_dumped_type(const char *name)
{
  uint64_t rep_id;
  if ( in_string_pool(name) )
  {
    UniqName key { current_element_type_, name };
    const auto ci = m_dumped_db.find(key);
    if ( ci == m_dumped_db.cend() )
      return 0;
    rep_id = ci->second;
  } else {
    UniqName2 key { current_element_type_, name };
    const auto ci = m_dumped_db2.find(key);
    if ( ci == m_dumped_db2.cend() )
      return 0;
    rep_id = ci->second;
  }    
  // put fake type into m_replaced
  // don`t replace functions
  if ( current_element_type_ != subroutine )
  {
    dumped_type dt { current_element_type_, name, rep_id };
    m_replaced[elements_.back().id_] = dt;
  }
  // remove current element
  elements_.pop_back();
  AddNone();
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
  if ( g_opt_j )
  {
    auto json = GenerateJson();
    // if this was last unit - cut final comma
    if ( last )
    {
      json.pop_back();
      json.pop_back();
    }
    if ( !json.empty() )
      fprintf(g_outf, "%s", json.c_str());
  } else {
    for ( auto &e: elements_ )
      m_els[e.id_] = &e;
    dump_types();
  }
  merge_dumped();
  elements_.clear();
  m_replaced.clear();
  m_els.clear();
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

void TreeBuilder::SetParentAccess(int a)
{
  if ( m_stack.empty() ) {
    fprintf(stderr, "Can't add a access when stack is empty\n");
    return;
  }
  if ( current_element_type_ == ElementType::member )
  {
    if ( m_stack.top()->members_.empty()) {
      fprintf(stderr, "Can't set the member access if the members list is empty\n");
      return;
    }
  // fprintf(g_outf, "SetParentAccess %s a %d\n", m_stack.top()->members_.back().name_, a);
    m_stack.top()->members_.back().access_ = a;
    return;
  } else {
    if ( m_stack.top()->parents_.empty() ) {
      fprintf(stderr, "Can't add a parent access when parents list is empty\n");
      return;
    }
    m_stack.top()->parents_.back().access = a;
  }
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

void TreeBuilder::SetLinkageName(const char* name) {
  if (current_element_type_ == ElementType::none) {
    return;
  }
  if (!elements_.size()) {
    fprintf(stderr, "Can't set an linkage name if the element list is empty\n");
    return;
  }
  elements_.back().link_name_ = name;
  return;
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

void TreeBuilder::SetAddr(uint64_t count) {
  if (current_element_type_ != ElementType::subroutine) {
    return;
  }
  if (!elements_.size()) {
    fprintf(stderr, "Can't set address when element list is empty\n");
    return;
  }
  elements_.back().addr_ = count;
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
    fprintf(stderr, "Can't set an element count if the element list is empty\n");
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
    case ElementType::volatile_type: return "volatile";
    case ElementType::restrict_type: return "restrict";
    case ElementType::reference_type: return "reference";
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
    if ( access_ ) {
      result += "\"access\":"+std::to_string(access_)+",";
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
  if ( link_name_ && link_name_ != name_ ) {
    result += "\"link_name\":\""+EscapeJsonString(link_name_)+"\",";
  }
  if (size_) {
    result += "\"size\":"+std::to_string(size_)+",";
  }
  if ( addr_ ) {
    result += "\"addr\":"+std::to_string(addr_)+",";
  }
  if (count_) {
    result += "\"count\":"+std::to_string(count_)+",";
  }
  if (parents_.size() > 0) {
    result += "\"parents\":[";
    for (size_t i = 0; i < parents_.size(); i++) {
      result += "{\"id\":\""+std::to_string(tb->get_replaced_type(parents_[i].id))+"\",";
      if ( parents_[i].access )
        result += "\"access\":"+std::to_string(parents_[i].access)+"\",";
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