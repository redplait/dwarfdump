#include "JsonRender.h"

// static
std::string JsonRender::EscapeJsonString(const char* str) {
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

void JsonRender::RenderUnit(int last)
{
  auto json = GenerateJson();
  // if this was last unit - cut final comma
  if ( last && !json.empty() )
  {
    json.pop_back();
    json.pop_back();
  }
  if ( !json.empty() )
  {
    put_file_hdr();
    fprintf(g_outf, "%s", json.c_str());
  }
}

std::string JsonRender::GenerateJson() {
  std::string result;
  if ( elements_.empty() )
    return result;
  for ( auto &e: elements_ ) {
    if ( e.type_ == ElementType::var_type && !e.addr_ )
    {
      auto ti = m_tls.find(e.id_);
      if ( ti == m_tls.end() )
        continue;
    }
    auto jres = GenerateJson(e);
    if ( !jres.empty() )
    {
      result += jres;
      result += ",\n";
    }
  }
  return result;
}

template <class T>
std::string &JsonRender::put(std::string &res, const char *name, T v)
{
  res += "\"";
  res += name;
  res += "\":\""+std::to_string(v);
  res += "\",";
  return res;
}

template <>
std::string &JsonRender::put(std::string &res, const char *name, const char *v)
{
  res += "\"";
  res += name;
  res += "\":\""+EscapeJsonString(v);
  res += "\",";
  return res;
}

template <>
std::string &JsonRender::put(std::string &res, const char *name, bool v)
{
  res += "\"";
  res += name;
  res += "\":\"1\",";
  return res;
}

std::string JsonRender::GenerateJson(Element &e) {
  std::string result;

  if (e.type_ == ElementType::ns_end) {
    return "";
  }
  // A member is a special case
  if (e.type_ == ElementType::member) {
    result = "{";

    if (e.type_id_)
      put(result, "type_id", get_replaced_type(e.type_id_));
    if (e.name_)
      put(result, "name", e.name_);
    if ( e.level_ && g_opt_l )
      put(result, "level", e.level_);
    if ( e.access_ )
      put(result, "access", e.access_);
    if ( e.bit_size_ )
    {
      put(result, "bit_offset", e.bit_offset_);
      put(result, "bit_size", e.bit_size_);
    }
    result += "\"offset\":"+std::to_string(e.offset_);
    result += "}";
    return result;
  }

  // The others are generic  
  result += "\""+std::to_string(e.id_)+"\":{";
  put(result, "type", e.TypeName());
  if ( e.dumped_ )
    put(result, "dumped", e.dumped_);
  if ( e.type_ == ElementType::ptr2member && e.cont_type_ )
    put(result, "cont_type", e.cont_type_);
  if ( e.owner_ != nullptr )
    put(result, "owner", e.owner_->id_);
  if ( e.noret_ )
    put(result, "noreturn", e.noret_);
  if ( e.align_ )
    put(result, "alignment", e.align_);
  if (e.type_id_)
    put(result, "type_id", e.type_id_);
  if (e.name_) {
      if (e.type_ == ElementType::ns_start) {
        result += "\"name\":\""+EscapeJsonString(e.name_)+"\"}";
        return result;
      }
    put(result, "name", e.name_);
  }
  if ( e.link_name_ && e.link_name_ != e.name_ )
    put(result, "link_name", e.link_name_);
  if (e.spec_)
    put(result, "spec", e.spec_);
  if ( e.abs_ )
    put(result, "abs", e.abs_);
  if (e.size_)
    put(result, "size", e.size_);
  if ( e.addr_ )
  {
    put(result, "addr", e.addr_);
    if ( g_opt_s && m_snames != nullptr )
    {
      auto sname = m_snames->find_sname(e.addr_);
      if ( sname != nullptr )
        put(result, "section", sname);
    }
  }
  if ( e.inlined_ )
    put(result, "inline", e.inlined_);
  if ( e.type_ == ElementType::var_type )
  {
    auto ti = m_tls.find(e.id_);
    if ( ti != m_tls.end() )
      put(result, "tls_index", ti->second);
  }
  if ( e.type_ == ElementType::method )
  {
    if ( g_opt_l )
      put(result, "level", e.level_);
    Method &m = static_cast<Method &>(e);
    if ( m.vtbl_index_ )
      put(result, "vtbl_index", m.vtbl_index_);
    if ( m.virt_ )
      put(result, "virt", m.virt_);
    if ( m.this_arg_ )
      put(result, "this_arg", m.this_arg_);
    if ( m.def_ )
      put(result, "default", m.def_);
    if ( m.expl_ )
      put(result, "explicit", m.expl_);
  }
  if (e.count_)
    put(result, "count", e.count_);
  // parents
  if ( e.m_comp && !e.m_comp->parents_.empty() ) {
    result += "\"parents\":[";
    for (size_t i = 0; i < e.m_comp->parents_.size(); i++) {
      result += "{\"id\":\""+std::to_string(get_replaced_type(e.m_comp->parents_[i].id))+"\",";
      if ( e.m_comp->parents_[i].access )
        put(result, "access", e.m_comp->parents_[i].access);
      result += "\"offset\":"+std::to_string(e.m_comp->parents_[i].offset)+"}";
      if (i+1 < e.m_comp->parents_.size()) {
        result += ",";
      }
    }
    result += "],";
  }
  if ( e.m_comp && !e.m_comp->params_.empty() )
  {
    result += "\"params\":[";
    for (size_t i = 0; i < e.m_comp->params_.size(); i++) {
      result += "{";
      if ( e.m_comp->params_[i].name )
        put(result, "name", e.m_comp->params_[i].name);
      if ( e.m_comp->params_[i].param_id )
        put(result, "id", e.m_comp->params_[i].param_id);
      if ( e.m_comp->params_[i].var_ )
        put(result, "variable", e.m_comp->params_[i].var_);
      if (e.m_comp->params_[i].ellipsis)
      {
        put(result, "ellipsis", e.m_comp->params_[i].ellipsis);
      } else {
        if ( e.m_comp->params_[i].id )
          put(result, "type_id", e.m_comp->params_[i].id);
      }
      if (result.back() == ',')
        result.pop_back();
      result += "}";
      if (i+1 < e.m_comp->params_.size()) {
        result += ",";
      }
    }
    result += "],";
  }
  if ( e.m_comp && !e.m_comp->enums_.empty() )
  {
    result += "\"enums\":[";
    for (size_t i = 0; i < e.m_comp->enums_.size(); i++) {
      put(result, "name", e.m_comp->enums_[i].name);
      result += "\"value\":"+std::to_string(e.m_comp->enums_[i].value)+"}";
      if (i+1 < e.m_comp->enums_.size()) {
        result += ",";
      }
    }
    result += "],";
  }
  if ( e.m_comp && !e.m_comp->members_.empty() ) {
    result += "\"members\":[";
    for (size_t i = 0; i < e.m_comp->members_.size(); i++) {
      result += GenerateJson(e.m_comp->members_[i]);
      if (i+1 < e.m_comp->members_.size()) {
        result += ",";
      }
    }
    result += "],";
  }
  if ( e.m_comp && !e.m_comp->methods_.empty() ) {
    result += "\"methods\":[";
    for ( auto &m: e.m_comp->methods_ )
      result += GenerateJson(m) + ",\n"; 

    if (result.back() == '\n')
      result.pop_back();
    if (result.back() == ',')
      result.pop_back();
    result += "],";
  }
  if (result.back() == ',') {
    result.pop_back();
  }
  result += "}";
  return result;
}