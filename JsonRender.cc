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
  if ( last )
  {
    json.pop_back();
    json.pop_back();
  }
  if ( !json.empty() )
    fprintf(g_outf, "%s", json.c_str());
}

std::string JsonRender::GenerateJson() {
  std::string result;
  if ( elements_.empty() )
    return result;
  for ( auto &e: elements_ ) {
    auto jres = GenerateJson(e);
    if ( !jres.empty() )
    {
      result += jres;
      result += ",\n";
    }
  }
  return result;
}

std::string JsonRender::GenerateJson(Element &e) {
  std::string result;

  if (e.type_ == ElementType::ns_end) {
    return "";
  }
  // A member is a special case
  if (e.type_ == ElementType::member) {
    result = "{";

    if (e.type_id_) {
      result += "\"type_id\":\""+std::to_string(get_replaced_type(e.type_id_))+"\",";
    }
    if (e.name_) {
      result += "\"name\":\""+EscapeJsonString(e.name_)+"\",";
    }
    if ( e.level_ && g_opt_l ) {
      result += "\"level\":\""+std::to_string(e.level_)+"\",";
    }
    if ( e.access_ ) {
      result += "\"access\":"+std::to_string(e.access_)+",";
    }
    if ( e.bit_offset_ )
      result += "\"bit_offset\":"+std::to_string(e.bit_offset_)+",";
    if ( e.bit_size_ )
      result += "\"bit_size\":"+std::to_string(e.bit_size_)+",";
    result += "\"offset\":"+std::to_string(e.offset_);

    result += "}";
    return result;
  }

  // The others are generic
  result += "\""+std::to_string(e.id_)+"\":";
  result += "{\"type\":\""+std::string(e.TypeName())+"\",";
  if ( e.align_ )
    result += "\"alignment\":\""+std::to_string(e.align_)+"\",";
  if (e.type_id_) {
    result += "\"type_id\":\""+std::to_string(e.type_id_)+"\",";
  }
  if (e.name_) {
      if (e.type_ == ElementType::ns_start) {
        result += "\"name\":\""+EscapeJsonString(e.name_)+"\"}";
        return result;
      }
    result += "\"name\":\""+EscapeJsonString(e.name_)+"\",";
  }
  if ( e.link_name_ && e.link_name_ != e.name_ ) {
    result += "\"link_name\":\""+EscapeJsonString(e.link_name_)+"\",";
  }
  if (e.size_) {
    result += "\"size\":"+std::to_string(e.size_)+",";
  }
  if ( e.addr_ ) {
    result += "\"addr\":"+std::to_string(e.addr_)+",";
  }
  if (e.count_) {
    result += "\"count\":"+std::to_string(e.count_)+",";
  }
  if ( e.m_comp && !e.m_comp->parents_.empty() ) {
    result += "\"parents\":[";
    for (size_t i = 0; i < e.m_comp->parents_.size(); i++) {
      result += "{\"id\":\""+std::to_string(get_replaced_type(e.m_comp->parents_[i].id))+"\",";
      if ( e.m_comp->parents_[i].access )
        result += "\"access\":"+std::to_string(e.m_comp->parents_[i].access)+"\",";
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
      if ( e.m_comp->params_[i].name )
        result += "{\"name\":\""+EscapeJsonString(e.m_comp->params_[i].name)+"\",";
      else
        result += "{";
      if (e.m_comp->params_[i].ellipsis)
      {
        result += "\"ellipsis\"}";
      } else {
        if ( e.m_comp->params_[i].id )
          result += "\"type_id\":"+std::to_string(e.m_comp->params_[i].id)+"}";
      }
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
      result += "{\"name\":\""+EscapeJsonString(e.m_comp->enums_[i].name)+"\",";
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

  if (result.back() == ',') {
    result.pop_back();
  }
  result += "}";
  return result;
}