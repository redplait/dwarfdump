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
    if ( e.type_ == ElementType::var_type && e.dumped_ ) continue;
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

template <>
std::string &JsonRender::put(std::string &res, const char *name, const void *v)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "0x%p", v);
  res += "\"";
  res += name;
  res += "\":\"";
  res += buf;
  res += "\",";
  return res;
}

inline void cut_last_comma(std::string &result) {
    if (result.back() == '\n')
      result.pop_back();
    if (result.back() == ',')
      result.pop_back();
}

void JsonRender::render_location(std::string &s, param_loc &pl)
{
  int idx = 0;
  s += "\"loc\":[";
  for ( auto &l: pl.locs )
  {
    if ( idx ) s.push_back(',');
    idx++;
    s.push_back('{');
    auto op_name = locs_no_ops(l.type);
    if ( op_name ) {
      put(s, "op", op_name);
      s.push_back('}');
      continue;
    }
    const char *reg_name = nullptr;
    switch(l.type)
    {
      case regval_type:
        // op
        put(s, "op", "OP_breg");
        // reg idx
        put(s, "reg", l.idx);
        if ( m_rnames )
          reg_name = m_rnames->reg_name(l.idx);
        if ( reg_name )
          put(s, "reg_name", reg_name);
        // offset
        if ( l.offset )
          put(s, "offset", l.offset);
       break;
      case reg:
        put(s, "op", "OP_reg");
        // reg idx
        put(s, "reg", l.idx);
        if ( m_rnames )
          reg_name = m_rnames->reg_name(l.idx);
        if ( reg_name )
          put(s, "reg_name", reg_name);
       break;
      case breg:
        put(s, "op", "OP_breg");
        // reg idx
        put(s, "reg", l.idx);
        if ( m_rnames )
          reg_name = m_rnames->reg_name(l.idx);
        if ( reg_name )
          put(s, "reg_name", reg_name);
        // offset
        if ( l.offset )
          put(s, "offset", l.offset);
       break;
      case fpiece:
        put(s, "op", "piece");
        put(s, "value", l.idx);
       break;
      case imp_value:
        put(s, "op", "implicit_value");
        put(s, "value", l.idx);
       break;
      case svalue:
        put(s, "op", "svalue");
        put(s, "value", l.sv);
       break;
      case fvalue:
        put(s, "op", "fvalue");
        put(s, "value", l.sv);
       break;
      case uvalue:
        put(s, "op", "uvalue");
        put(s, "value", l.conv);
       break;
      case deref_type:
        put(s, "op", "deref_type");
        put(s, "size", l.offset);
        if ( l.conv )
          put(s, "value", l.conv);
       break;
      case convert:
        put(s, "op", "convert");
        put(s, "idx", l.conv);
       break;
      case deref_size:
        put(s, "op", "deref_size");
        put(s, "size", l.idx);
       break;
      case fbreg:
        put(s, "op", "fbreg");
        put(s, "value", l.offset);
       break;
      case plus_uconst:
        put(s, "op", "plus_uconst");
        put(s, "value", l.offset);
       break;
      case tls_index:
        put(s, "op", "TlsIndex");
        put(s, "value", l.offset);
       break;
      default: e_->error("unknown location op %d\n", l.type);
    }
    cut_last_comma(s);
    s.push_back('}');
  }
  s += ']';
}

void JsonRender::RenderGoAttrs(std::string &res, uint64_t id)
{
  auto giter = m_go_attrs.find(id);
  if ( giter == m_go_attrs.end() )
    return;
  auto &g = giter->second;
  if ( g.kind )
    put(res, "go_kind", g.kind);
  if ( g.key )
    put(res, "go_key", g.key);
  if ( g.elem )
    put(res, "go_elem", g.elem);
  if ( g.rt_type )
    put(res, "go_rt_type", g.rt_type);
  if ( g.dict_index )
    put(res, "go_index", g.dict_index);
}

std::string JsonRender::GenerateJson(Element &e) {
  std::string result;

  if (e.type_ == ElementType::ns_end)
    return "";
  if ( ElementType::lexical_block == e.type_ ) return "";
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
    if ( e.addr_class_ )
      put(result, "addr_class", e.addr_class_);
    if ( e.has_go )
      RenderGoAttrs(result, e.type_id_);
    result += "\"offset\":"+std::to_string(e.offset_);
    result += "}";
    return result;
  }

  // The others are generic
  result += "\""+std::to_string(e.id_)+"\":{";
  put(result, "type", e.TypeName());
  if ( e.dumped_ )
    put(result, "dumped", e.dumped_);
  if ( e.has_go )
    RenderGoAttrs(result, e.type_id_);
  if ( e.ate_ )
    put(result, "ate", e.ate_);
  if ( !e.fullname_.empty() )
    put(result, "file", e.fullname_.c_str());
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
    // frame size
    if ( e.type_ == ElementType::subroutine && m_locX )
    {
      uint64_t fsize = 0;
      if ( m_locX->find_dfa(e.addr_, fsize) )
        put(result, "frame_size", fsize);
    }
  } else if ( e.has_range_ )
  {
    std::list<std::pair<uint64_t, uint64_t> > ranges;
    if ( lookup_range(e.id_, ranges) )
    {
      result += "\"addr_ranges\":[";
      for ( auto &r: ranges ) {
        result += "{";
        put(result, "start", r.first);
        put(result, "end", r.second);
        if ( g_opt_s && m_snames != nullptr )
        {
          auto sname = m_snames->find_sname(r.first);
          if ( sname != nullptr )
            put(result, "section", sname);
        }
        result += "},";
      }
      if (result.back() == ',')
        result.pop_back();
      result += "],";
      // try get frame size for any range
      if ( m_locX) for ( auto &r: ranges ) {
        uint64_t fsize = 0;
        if ( m_locX->find_dfa(r.first, fsize) ) {
          put(result, "frame_size", fsize);
          break;
        }
      }
    }
  }
  if ( e.inlined_ )
    put(result, "inline", e.inlined_);
  if ( e.const_expr_ )
    put(result, "const_expr", e.const_expr_);
  if ( e.enum_class_ )
    put(result, "enum_class", e.enum_class_);
  if ( e.gnu_vector_ )
    put(result, "gnu_vector", e.gnu_vector_);
  if ( e.tensor_ )
    put(result, "tensor", e.tensor_);
  if ( e.addr_class_ )
      put(result, "addr_class", e.addr_class_);
  if ( e.type_ == ElementType::var_type )
  {
    if ( g_opt_l )
      put(result, "level", e.level_);
    auto ti = m_tls.find(e.id_);
    if ( ti != m_tls.end() )
      put(result, "tls_index", ti->second);
    if ( e.locx_ && m_locX )
    {
      std::list<LocListXItem> locs;
      if ( m_locX->get_loclistx(e.locx_, locs, cu.cu_base_addr) )
      {
        // dump list of locations
        result += "\"loc_list\":[";
        for ( auto &l: locs )
        {
          std::string loc;
          render_location(loc, l.loc);
          if ( !loc.empty() )
          {
            result += "{";
            put(result, "start", l.start);
            put(result, "end", l.end);
            result += loc;
            cut_last_comma(result);
            result += "},";
          }
        }
        cut_last_comma(result);
        result += "],";
      }
    } else if ( e.owner_ && e.owner_->m_comp ) {
      auto liter = e.owner_->m_comp->lvar_locs_.find(&e);
      if ( liter != e.owner_->m_comp->lvar_locs_.end() )
      {
        std::string loc;
        render_location(loc, liter->second);
        if ( !loc.empty() ) result += loc;
      }
    }
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
    if ( m.ref_ )
      put(result, "ref_", m.ref_ );
    if ( m.rval_ref_ )
      put(result, "rval_ref_", m.rval_ref_);
  }
  if (e.count_)
    put(result, "count", e.count_);
  // parents
  if ( e.m_comp && !e.m_comp->parents_.empty() ) {
    result += "\"parents\":[";
    for (size_t i = 0; i < e.m_comp->parents_.size(); i++) {
      result += "{\"id\":\""+std::to_string(get_replaced_type(e.m_comp->parents_[i].id))+"\",";
      if ( e.m_comp->parents_[i].virtual_ )
        put(result, "virtual", e.m_comp->parents_[i].virtual_);
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
      if ( e.m_comp->params_[i].pdir )
        put(result, "pdir", e.m_comp->params_[i].pdir);
      if ( e.m_comp->params_[i].optional_ )
        put(result, "optinal", e.m_comp->params_[i].optional_);
      if (e.m_comp->params_[i].ellipsis)
      {
        put(result, "ellipsis", e.m_comp->params_[i].ellipsis);
      } else {
        if ( e.m_comp->params_[i].id )
          put(result, "type_id", e.m_comp->params_[i].id);
      }
      if ( !e.m_comp->params_[i].loc.empty() )
      {
        std::string ploc;
        render_location(ploc, e.m_comp->params_[i].loc);
        if ( !ploc.empty() ) result += ploc;
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
  if ( e.m_comp && !e.m_comp->lvars_.empty() ) {
    result += "\"lvars\":[";
    for ( auto m: e.m_comp->lvars_ )
    {
      result += GenerateJson(*m) + ",\n";
      m->dumped_ = 1;
    }
    cut_last_comma(result);
    result += "],";
  }
  if ( e.m_comp && !e.m_comp->methods_.empty() ) {
    result += "\"methods\":[";
    for ( auto &m: e.m_comp->methods_ )
      result += GenerateJson(m) + ",\n";
    cut_last_comma(result);
    result += "],";
  }
  if (result.back() == ',') {
    result.pop_back();
  }
  result += "}";
  return result;
}