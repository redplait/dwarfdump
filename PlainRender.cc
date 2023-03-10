#include "PlainRender.h"
#include "dwarf32.h"

static const char *s_marg = "  ";

std::string &add_margin(std::string &s, int level)
{
  for ( int i = 0; i < level; ++i )
    s += s_marg;
  return s;
}

std::string &PlainRender::render_one_enum(std::string &s, EnumItem &en)
{
  s = en.name;
  s += " = "; 
  if ( en.value >= 0 && en.value < 10 )
    s += std::to_string((int)en.value);
  else {
    char buf[40];
    snprintf(buf, sizeof(buf), "0x%lX", en.value);
    s += buf; 
  }
  return s;
}

void PlainRender::RenderUnit(int last)
{
  for ( auto &e: elements_ )
    m_els[e.id_] = &e;
  dump_types();
  m_els.clear();
}

bool PlainRender::dump_type(uint64_t key, std::string &res, int level)
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
       el->second->type_ == ElementType::base_type ||
       el->second->type_ == ElementType::class_type
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
    else if ( el->second->m_comp != nullptr )
    {
      res += "{\n";
      render_fields(el->second, res, level + 1);
      add_margin(res, level);
      res += "}";
    }
    return true;
  }
  if ( el->second->type_ == ElementType::union_type )
  {
    res = "union ";
    if ( el->second->name_ )
      res += el->second->name_;
    else if ( el->second->m_comp != nullptr )
    {
      res += "{\n";
      render_fields(el->second, res, level + 1);
      add_margin(res, level);
      res += "}";
    }
    return true;
  }
  if ( el->second->type_ == ElementType::enumerator_type )
  {
    res = "enum ";
    if ( el->second->name_ )
      res += el->second->name_;
    else if ( el->second->m_comp != nullptr )
    {
      res += "{\n";
      int n = 0;
      for ( auto &en: el->second->m_comp->enums_ )
      {
        std::string one;
        if ( n )
          res += ",\n";
        add_margin(res, level);
        n++;
        res += render_one_enum(one, en);
      }
      res += "\n}";
    }
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
  if ( el->second->type_ == ElementType::rvalue_ref_type )
  {
    std::string tmp;
    dump_type(el->second->type_id_, tmp);
    res = tmp;
    res += "&&";
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

void PlainRender::dump_enums(Element *e)
{
  if ( !e->m_comp )
    return;
  int n = 0;
  std::string s;
  for ( auto &en: e->m_comp->enums_ )
  {
    std::string one;
    if ( n )
      s += ",\n";
    add_margin(s, 1);
    n++;
    s += render_one_enum(one, en);
  }
  fprintf(g_outf, "%s\n", s.c_str());
}

std::string &PlainRender::render_field(Element *e, std::string &s, int level)
{
  dump_type(e->type_id_, s, level);
  if ( e->name_ )
  {
     s += " ";
    s += e->name_;
  }
  // pdbdump format for bit fields :offset:size
  if ( e->bit_size_ )
  {
    s += ":";
    s += std::to_string(e->bit_offset_);
    s += ":";
    s += std::to_string(e->bit_size_);
  }
  return s;
}

std::string &PlainRender::render_fields(Element *e, std::string &s, int level)
{
  if ( !e->m_comp )
    return s;
  for ( auto &en: e->m_comp->members_ )
  {
    std::string tmp;
    add_margin(s, level);
    s += "// Offset 0x";
    char buf[40];
    snprintf(buf, sizeof(buf), "%lX\n", en.offset_);
    s += buf;
    add_margin(s, level);
    render_field(&en, tmp, level + 1);
    s += tmp + ";\n";
  }
  return s;
}

void PlainRender::dump_fields(Element *e)
{
  if ( !e->m_comp )
    return;
  for ( auto &en: e->m_comp->members_ )
  {
    std::string tmp;
    fprintf(g_outf, "// Offset 0x%lX\n", en.offset_);
    render_field(&en, tmp, 1);
    fprintf(g_outf, "%s;\n", tmp.c_str());
  }
}

void PlainRender::dump_methods(Element *e)
{
  if ( !e->has_methods() )
    return;
  fprintf(g_outf, "// --- methods\n");
  for ( auto &en: e->m_comp->methods_ )
  {
    std::string tmp;
    dump_method(&en, tmp);
    if ( g_opt_v )
      fprintf(g_outf, "// TypeId %lX\n", en.id_);
    fprintf(g_outf, "%s;\n", tmp.c_str());
  }
}

const char *access_name(int i)
{
  switch(i)
  {
    case 1: return "public ";
    case 2: return "protected ";
    case 3: return "private ";
  }
  return "";
}

void PlainRender::dump_method(Method *e, std::string &res)
{
  res = access_name(e->access_);
  if ( e->virt_ )
    res += "virtual ";
  if ( !e->this_arg_ )
    res += "static ";
  std::string tmp;
  if ( e->type_id_ )
    dump_type(e->type_id_, tmp);
  else {
    if ( !e->art_ )
      tmp = "void";
  }
  res += tmp + " ";
  res += e->name_;
  res += "(";
  if ( !e->m_comp || e->m_comp->params_.empty() )
    ;
  else {
    for ( size_t i = 0; i < e->m_comp->params_.size(); i++ )
    {
      tmp.clear();
      if ( e->m_comp->params_[i].ellipsis )
        tmp = "...";
      else  
        dump_type(e->m_comp->params_[i].id, tmp);
      if ( e->m_comp->params_[i].name )
        res += tmp + e->m_comp->params_[i].name;
      else
        res += tmp;
      if ( i+1 < e->m_comp->params_.size() )
        res += ",";
    }
  }
  res += ")";
  if ( e->virt_ == Dwarf32::Virtuality::DW_VIRTUALITY_pure_virtual )
    res += " = 0";  
}

void PlainRender::dump_func(Element *e)
{
  std::string tmp;
  if ( e->type_id_ )
    dump_type(e->type_id_, tmp);
  else
    tmp = "void";
  fprintf(g_outf, "%s %s(", tmp.c_str(), e->name_);
  if ( !e->m_comp || e->m_comp->params_.empty() )
    fprintf(g_outf, "void");
  else {
    for ( size_t i = 0; i < e->m_comp->params_.size(); i++ )
    {
      tmp.clear();
      if ( e->m_comp->params_[i].ellipsis )
        tmp = "...";
      else  
        dump_type(e->m_comp->params_[i].id, tmp);
      if ( e->m_comp->params_[i].name )
        fprintf(g_outf, "%s %s", tmp.c_str(), e->m_comp->params_[i].name);
      else
        fprintf(g_outf, "%s", tmp.c_str());
      if ( i+1 < e->m_comp->params_.size() )
        fprintf(g_outf, ",");
    }
  }
  fprintf(g_outf, ")");
}

void PlainRender::dump_types()
{
  for ( auto &e: elements_ )
  {
    if ( ElementType::ns_end == e.type_ )
    {
      fprintf(g_outf, "} // namespace %s\n", e.name_);
      continue;
    }
    if ( ElementType::ns_start == e.type_ )
    {
      fprintf(g_outf, "namespace %s {\n", e.name_);
      continue;
    }
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
        fprintf(g_outf, "enum %s", e.name_);
        if ( e.is_pure_decl() )
          break;
        fprintf(g_outf, " {\n");
        dump_enums(&e);
        fprintf(g_outf, "}");
        break;
      case ElementType::structure_type:
        fprintf(g_outf, "struct %s", e.name_);
        if ( e.is_pure_decl() )
          break;
        fprintf(g_outf, " {\n");
        dump_fields(&e);
        dump_methods(&e);
        fprintf(g_outf, "}");
        break;
      case ElementType::union_type:
        fprintf(g_outf, "union %s", e.name_);
        if ( e.is_pure_decl() )
          break;
        fprintf(g_outf, " {\n");
        dump_fields(&e);
        dump_methods(&e);
        fprintf(g_outf, "}");
        break;
      case ElementType::class_type:
        fprintf(g_outf, "class %s", e.name_);
        if ( e.is_pure_decl() )
          break;
        if ( e.m_comp && !e.m_comp->parents_.empty() )
        {
          fprintf(g_outf, " :\n");
          for ( size_t pi = 0; pi < e.m_comp->parents_.size(); pi++ )
          {
            fprintf(g_outf, "// offset %lX\n", e.m_comp->parents_[pi].offset);
            std::string pname;
            dump_type(e.m_comp->parents_[pi].id, pname);
            fprintf(g_outf, "%s%s", access_name(e.m_comp->parents_[pi].access), pname.c_str());
            if ( pi != e.m_comp->parents_.size() - 1 )
              fprintf(g_outf, ",\n");
            else
              fprintf(g_outf, "\n");
          }
        }
        fprintf(g_outf, "{\n");
        dump_fields(&e);
        dump_methods(&e);
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
