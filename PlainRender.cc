#include "PlainRender.h"
#include "dwarf32.h"
#include <string.h>

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

std::list<TreeBuilder::Element *> *PlainRender::get_specs(uint64_t id)
{
  auto fi = m_specs.find(id);
  if ( fi == m_specs.end() )
    return nullptr;
  return &fi->second;
}

void PlainRender::prepare(std::list<Element> &els)
{
  int need_abs = 0;
  for ( auto &e: els )
  {
    m_els[e.id_] = &e;
    if ( e.spec_ && e.addr_ )
    {
      // fprintf(g_outf, "spec %lX for %lX\n", e.id_, e.spec_);
      m_specs[e.spec_].push_back(&e);
    }
    if ( e.is_abs() )
      need_abs |= 1;
    // add methods too
    if ( e.has_methods() )
      for ( auto &m: e.m_comp->methods_ )
      {
        m_els[m.id_] = &m;
        if ( m.spec_ && m.addr_ )
          m_specs[m.spec_].push_back(&m);
        if ( m.is_abs() )
          need_abs |= 1;
      }
  }
  if ( need_abs )
  {
    // collect addresses for functions with abstract_origin
    for ( auto &e: els )
    {
      if ( !e.is_abs() )
        continue;
      // fprintf(g_outf, "type %lX abs %lX\n", e.id_, e.abs_);
      auto f = m_els.find(e.abs_);
      if ( f == m_els.end() )
      {
        if ( g_opt_v )
          fprintf(stderr, "cannot find origin with type %lX for %lX\n", e.abs_, e.id_);
        continue;
      }
      if ( !f->second->spec_ )
      {
        // fprintf(stderr, "invalid origin type %lX for %lX\n", e.abs_, e.id_);
        m_specs[f->second->id_].push_back(&e);
        continue;
      } else
        m_specs[f->second->spec_].push_back(&e);
    }
  }
}

void PlainRender::cmn_vars()
{
  if ( !m_vars.empty() )
  {
    fprintf(g_outf, "/// vars\n");
    dump_vars();
    m_vars.clear();
  }
}

void PlainRender::RenderUnit(int last)
{
  if ( !g_opt_g )
  {
    prepare(elements_);
    dump_types(elements_, &cu);
    cmn_vars();
    m_els.clear();
    m_specs.clear();
  } else {
    if ( !elements_.empty() )
      m_all.push_back({ cu, std::move(elements_)});
    if ( !last )
      return;
    for ( auto &p: m_all )
      prepare(p.second);
    for ( auto &p: m_all )
    {
      // fprintf(g_outf, "new unit %p\n", &p.first);
      m_hdr_dumped = false;
      dump_types(p.second, &p.first);
      cmn_vars();
    }
  }
}

bool PlainRender::dump_type(uint64_t key, std::string &res, named *n, int level)
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
    if ( is_go() )
    {
      auto go_el = m_go_types.find(key);
      if ( go_el != m_go_types.end() )
      {
        res = go_el->second;
        return true;
      }
    }
    res = "missed type ";
    res += std::to_string(key);
    return true;
  }
  if ( el->second->type_ == ElementType::typedef2 ||
       el->second->type_ == ElementType::base_type ||
       el->second->type_ == ElementType::class_type ||
       el->second->type_ == ElementType::unspec_type
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
    dump_type(el->second->type_id_, tmp, n);
    res = tmp;
    // probably wrong assumption: if we had subroutine_type somewhere below (and this is the only place where used_ field become true)
    // then we don`t need to add yet one asterisk
    if ( !n->used_ )
      res += "*";
    return true;
  }
  if ( el->second->type_ == ElementType::volatile_type )
  {
    std::string tmp;
    dump_type(el->second->type_id_, tmp, n);
    res = "volatile ";
    res += tmp;
    return true;
  }
  if ( el->second->type_ == ElementType::restrict_type )
  {
    std::string tmp;
    dump_type(el->second->type_id_, tmp, n);
    res = "restrict ";
    res += tmp;
    return true;
  }
  if ( el->second->type_ == ElementType::reference_type )
  {
    std::string tmp;
    dump_type(el->second->type_id_, tmp, n);
    res = tmp;
    res += "&";
    return true;
  }
  if ( el->second->type_ == ElementType::rvalue_ref_type )
  {
    std::string tmp;
    dump_type(el->second->type_id_, tmp, n);
    res = tmp;
    res += "&&";
    return true;
  }
  if ( el->second->type_ == ElementType::const_type )
  {
    std::string tmp;
    dump_type(el->second->type_id_, tmp, n);
    res = "const ";
    res += tmp;
    return true;
  }
  if ( el->second->type_ == ElementType::array_type )
  {
    dump_type(el->second->type_id_, res, n);
    res += "[";
    res += std::to_string(el->second->count_);
    res += "]";
    return true;
  }
  if ( el->second->type_ == ElementType::ptr2member )
  {
    std::string cname, tname, tmp;
    dump_type(el->second->cont_type_, cname, n);
    if ( n->name() != nullptr )
    {
      tmp = cname + "::*" + n->name();
      named n2 { tmp.c_str() };
      n2.no_ptr_ = true;
      dump_type(el->second->type_id_, tname, &n2);
      if ( n2.used_ )
      {
        n->used_ = true;
        res = tname;
        return true;
      } 
    } else
      dump_type(el->second->type_id_, tname, n);
    res = tname + " " + cname + "::*";
    return true;
  }
  if ( el->second->type_ == ElementType::subroutine_type )
  {
    auto sname = n->name();
    n->used_ = true;
    if ( el->second->type_id_ )
    {
      std::string tmp;
      dump_type(el->second->type_id_, tmp, n);
      res += tmp;
    } else
      res += "void";
    res += " (";
    if ( !n->no_ptr_ )
      res += "*";
    if ( sname != nullptr )
      res += sname;
    res += ")(";
    if ( !el->second->m_comp || el->second->m_comp->params_.empty() )
      ;
    else {
      std::string params;
      res += render_params(el->second, 0, params);
    }
    res += ")";
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
  named n { e->name_ };
  dump_type(e->type_id_, s, &n, level);
  auto name = n.name();
  if ( name != nullptr )
  {
     s += " ";
    s += name;
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

void PlainRender::dump_spec(Element *en)
{
  auto slist = get_specs(en->id_);
  if ( slist == nullptr )
    return;
  auto s = slist->size();
  if ( s > 1 )
    fprintf(g_outf, "// specifications: %ld\n", s);
  else
    fprintf(g_outf, "// specification\n");
  for ( auto e: *slist )
  {
    fprintf(g_outf, "//  addr %lX type_id %lX", e->addr_, e->id_);
    if ( e->link_name_ )
      fprintf(g_outf, " %s", e->link_name_);
    fprintf(g_outf, "\n");
  }
}

void PlainRender::dump_methods(Element *e)
{
  if ( !e->has_methods() )
    return;
  fprintf(g_outf, "// --- methods\n");
  for ( auto &en: e->m_comp->methods_ )
  {
    std::string tmp, plocs;
    dump_method(&en, e, tmp);
    if ( g_opt_v )
      fprintf(g_outf, "// TypeId %lX\n", en.id_);
    if ( en.vtbl_index_ )
      fprintf(g_outf, "// Vtbl index %lX\n", en.vtbl_index_);
    dump_spec(&en);
    if ( en.m_comp && dump_params_locations(en.m_comp->params_, plocs) )
      fprintf(g_outf, "%s", plocs.c_str());
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

bool PlainRender::is_constructor(const Element *e, const Element *owner) const
{
  if ( nullptr == e->name_ || owner->name_ == nullptr )
    return false;
  if ( e->name_ == owner->name_ )
    return true;
  return !strcmp(e->name_, owner->name_);
}

void PlainRender::dump_method(Method *e, const Element *owner, std::string &res)
{
  res = access_name(e->access_);
  if ( e->inlined_ )
    res += "inline ";
  if ( e->virt_ )
    res += "virtual ";
  if ( !e->this_arg_ )
    res += "static ";
  if ( e->expl_ )
    res += "explicit ";
  std::string tmp;
  if ( e->type_id_ )
  {
    named n { e->name_ };
    dump_type(e->type_id_, tmp, &n);
  } else {
    if ( !e->art_ && ! is_constructor(e, owner) )
      tmp = "void";
  }
  res += tmp + " ";
  if ( e->name_ )
    res += e->name_;
  res += "(";
  if ( !e->m_comp || e->m_comp->params_.empty() )
    ;
  else {
    std::string params;
    res += render_params(e, e->this_arg_, params);
  }
  res += ")";
  if ( e->def_ )
    res += " default";
  if ( e->virt_ == Dwarf32::Virtuality::DW_VIRTUALITY_pure_virtual )
    res += " = 0";  
}

std::string &PlainRender::render_params(Element *e, uint64_t this_arg, std::string &s)
{
  for ( size_t i = 0; i < e->m_comp->params_.size(); i++ )
  {
    std::string tmp;
    named n { e->m_comp->params_[i].param_id == this_arg ? "this" : e->m_comp->params_[i].name };
    if ( e->m_comp->params_[i].ellipsis )
      tmp = "...";
    else { 
      dump_type(e->m_comp->params_[i].id, tmp, &n);
    }
    auto name = n.name();
    if ( name != nullptr )
    {
      s += tmp + " ";
      s += name;
    } else
      s += tmp;
    if ( i+1 < e->m_comp->params_.size() )
      s += ",";
  }
  return s;
}

bool PlainRender::dump_params_locations(std::vector<FormalParam> &params, std::string &s)
{
  if ( params.empty() )
    return false;
  if ( std::all_of(params.cbegin(), params.cend(), [](const FormalParam &fp) -> bool { return fp.loc.locs.empty();}) )
    return false;
  bool res = false;
  size_t idx = 0;
  for ( auto &p: params )
  {
    idx++;
    if ( p.loc.empty() )
      continue;
    res = true;
    s += "// ";
    if ( p.name != nullptr )
      s += p.name;
    else
      s += "arg" + std::to_string(idx);
    s += " id ";
    char id_buf[40];
    snprintf(id_buf, sizeof(id_buf), "%lX", p.param_id);
    s += id_buf;
    s += ": ";
    for ( auto &l: p.loc.locs )
    {
      switch(l.type)
      {
        case deref: s += "OP_deref";
         break;
        case call_frame_cfa: s += "OP_call_frame_cfa";
         break;
        case reg: {
          s += "OP_reg";
          bool need_reg = true;          
          if ( m_rnames != nullptr )
          {
            auto rn = m_rnames->reg_name(l.idx);
            if ( rn )
            {
              need_reg = false;
              s += " ";
              s += rn;
            }
          }
          if ( need_reg )
            s += std::to_string(l.idx);
         }
         break;
        case breg: {
          s += "OP_breg";
          bool need_reg = true;          
          if ( m_rnames != nullptr )
          {
            auto rn = m_rnames->reg_name(l.idx);
            if ( rn )
            {
              need_reg = false;
              s += " ";
              s += rn;
            }
          }
          if ( need_reg )
            s += std::to_string(l.idx);
          s += " ";
          s += std::to_string(l.offset);
         }
         break;
        case fbreg:
          s += "OP_fbreg ";
          s += std::to_string(l.offset);
         break;
      }
      s += " ";
    }
    s += "\n";
  }
  return res;
}

void PlainRender::dump_func(Element *e)
{
  std::string tmp;
  dump_spec(e);
  if ( e->m_comp && dump_params_locations(e->m_comp->params_, tmp) )
  {
    fprintf(g_outf, "%s", tmp.c_str());
    tmp.clear();
  }
  if ( e->type_id_ )
  {
    named n { e->name_ };
    dump_type(e->type_id_, tmp, &n);
  } else
    tmp = "void";
  if ( e->inlined_ )
    fprintf(g_outf, "inline "); 
  fprintf(g_outf, "%s %s(", tmp.c_str(), e->name_);
  if ( !e->m_comp || e->m_comp->params_.empty() )
    fprintf(g_outf, "void");
  else {
    std::string params;
    render_params(e, 0, params);
    fprintf(g_outf, "%s", params.c_str());
  }
  fprintf(g_outf, ")");
}

void PlainRender::dump_var(Element *e)
{
  if ( e->link_name_ && e->link_name_ != e->name_ )
    fprintf(g_outf, "// LinkageName: %s\n", e->link_name_);
  std::string tname;
  named n { e->name_ };
  dump_type(e->type_id_, tname, &n);
  auto tn = n.name();
  if ( tn != nullptr )
    fprintf(g_outf, "%s %s;\n", tname.c_str(), e->name_);
  else
    fprintf(g_outf, "%s;\n", tname.c_str());
}

void PlainRender::dump_vars()
{
  for ( auto &e: m_vars )
  {
    if ( e->addr_ )
      fprintf(g_outf, "// Addr 0x%lX\n", e->addr_);
    if ( g_opt_v )
      fprintf(g_outf, "// TypeId %lX\n", e->id_);
    if ( e->name_ )
      dump_var(e);
    else if ( e->spec_ )
    {
      auto el = m_els.find(e->spec_);
      if ( el == m_els.end() )
        fprintf(g_outf, "// cannot find var with spec %lX\n", e->spec_);
      else
        dump_var(el->second);
    } else if ( e->abs_ )
    {
      auto el = m_els.find(e->abs_);
      if ( el == m_els.end() )
        fprintf(g_outf, "// cannot find var with abs %lX\n", e->abs_);
      else
        dump_var(el->second);
    } else
      fprintf(g_outf, "// unknown var id %lX\n", e->id_);
  }
}

void PlainRender::dump_types(std::list<Element> &els, struct cu *rcu)
{
  for ( auto &e: els )
  {
    if ( ElementType::var_type == e.type_ )
    {
      if ( e.addr_ )
        m_vars.push_back(&e);
      continue;
    }
    if ( ElementType::ns_end == e.type_ )
    {
      fprintf(g_outf, "}; // namespace %s\n", e.name_);
      continue;
    }
    if ( ElementType::ns_start == e.type_ )
    {
      put_file_hdr(rcu);
      fprintf(g_outf, "namespace %s {\n", e.name_);
      continue;
    }
    if ( ElementType::lexical_block == e.type_ )
      continue;
    if ( !e.name_ )
      continue;
    if ( e.level_ > 1 )
      continue;
    // skip base types
    if ( ElementType::base_type == e.type_ || ElementType::unspec_type == e.type_ )
      continue;
    if ( ElementType::const_type == e.type_ )
      continue;
    if ( g_opt_k && e.dumped_ && !should_keep(&e) )
      continue;
    // skip replaced types
    const auto ci = m_replaced.find(e.id_);
    if ( ci != m_replaced.end() )
      continue;
    put_file_hdr(rcu);
    if ( e.addr_ )
      fprintf(g_outf, "// Addr 0x%lX\n", e.addr_);
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
            named pn;
            dump_type(e.m_comp->parents_[pi].id, pname, &pn);
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
          named n { e.name_ };
          dump_type(e.type_id_, tname, &n);
          auto tn = n.name();
          if ( tn != nullptr )
            fprintf(g_outf, "typedef %s %s", tname.c_str(), e.name_);
          else
            fprintf(g_outf, "typedef %s", tname.c_str());
          break;
        }
      default:
        fprintf(g_outf, "// unknown type %d name %s\n", e.type_, e.name_);
        {
          std::string tname;
          named n { e.name_ };
          dump_type(e.type_id_, tname, &n);
          auto tn = n.name();
          if ( tn != nullptr )
            fprintf(g_outf, "%s %s", tname.c_str(), e.name_);
          else
            fprintf(g_outf, "%s", tname.c_str());          
        }
    }
    fprintf(g_outf, ";\n\n");
  }
}
