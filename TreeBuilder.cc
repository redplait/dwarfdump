#include "TreeBuilder.h"
#include "dwarf32.h"

// with name as std::string (args -kfxF)
// codeql/extractor:       total heap usage: 2,471,773 allocs, 2,471,773 frees, 383,592,855 bytes allocated
// codeql/extractor.swift: total heap usage: 5,514,815 allocs, 5,514,815 frees, 1,058,580,054 bytes allocated

TreeBuilder::TreeBuilder(ErrLog *e): e_(e)
{ }

TreeBuilder::~TreeBuilder()
{
  if ( m_rnames != nullptr )
    delete m_rnames;
  clear_namespaces(ns_root.nested);
}

void TreeBuilder::collect_go_types()
{
  for ( auto &e: elements_ )
  {
    if ( !e.name_ )
      continue; // and type must be named
    // no namespaces
    if ( is_ns(e) )
      continue;
    m_go_types[e.id_] = e.name_;
  }
}

// return 0 if element should be excluded from dump db/checking
int TreeBuilder::exclude_types(ElementType et, Element &e)
{
  if ( et == TreeBuilder::ElementType::formal_param ||
       et == TreeBuilder::ElementType::enumerator ||
       et == TreeBuilder::ElementType::member ||
       et == TreeBuilder::ElementType::ns_end )
    return 0;
  // skip local vars
  if ( et == TreeBuilder::ElementType::var_type && nullptr != e.owner_ )
    return 0;
  return 1;
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
    // no namespaces
    if ( is_ns(e) )
      continue;
    // no enum/vars/formal params
    if ( !exclude_types(e.type_, e) ) continue;
    // skip pure forward declarations
    if ( e.is_pure_decl() && e.type_ != structure_type )
      continue;
//  fprintf(g_outf, "dumped type %d with name %s level %d\n", e.type_, e.name_, e.level_);
    if ( e.level_ > 1 )
      continue; // we heed only high-level types definitions
    size_t rank = e.get_rank();
    auto ns = e.ns_;
    auto ename = e.mangled();
    if ( in_string_pool(ename) )
    {
      UniqName key { e.type_, ename };
      auto added = ns->m_dumped_db.find(key);
      if ( added != ns->m_dumped_db.end() )
      {
        if ( e.dumped_ && rank <= added->second.second )
          continue; // this type already in m_dumped_db
      }
      ns->m_dumped_db[key] = { e.id_, rank };
    } else {
      UniqName2 key { e.type_, ename };
      auto added = ns->m_dumped_db2.find(key);
      if ( added != ns->m_dumped_db2.end() )
      {
        if ( e.dumped_ && rank <= added->second.second )
          continue; // this type already in m_dumped_db2
      }
      ns->m_dumped_db2[key] = { e.id_, rank };
    }
    res++; 
  }
  return res;
}

inline bool is_2op(param_op_type op)
{
  switch(op)
  {
    case fand:
    case Dfdiv:
    case fminus:
    case fplus:
    case f_or:
    case fxor:
    case Dfmul:
    case Dfmod:
    case fshl:
    case fshr:
     return true;
    default:
     return false;
  }
  return false;
}

uint64_t TreeBuilder::calc_redudant_locs(const param_loc &pl)
{
  // state is amount of sequential imm values
  int state = 0;
  uint64_t res = 0;
  for ( auto &l: pl.locs )
  {
    if ( l.type == svalue || l.type == fvalue || l.type == uvalue )
    {
      state++;
      continue;
    }
    if ( state && (l.type == fneg || l.type == Dfabs || l.type == fnot) )
    {
      res += 2;
      continue;
    }
    if ( state > 1 && is_2op(l.type) )
    {
      res += 3;
      // after simplification we have in stack some const value instead of 2
      state--;
      continue;
    }
    state = 0;
  }
  return res;
}

void TreeBuilder::dump_location(std::string &s, param_loc &pl)
{
  int idx = 0;
  char buf[40];
    for ( auto &l: pl.locs )
    {
      if ( idx )
        s += " ; ";
      ++idx;
      auto op_name = locs_no_ops(l.type);
      if ( op_name ) {
        s += op_name;
        continue;
      }
      switch(l.type)
      {
        case regval_type: {
          s += "regval_type";
          bool need_reg = true;
          if ( m_rnames != nullptr )
          {
            auto rn = m_rnames->reg_name(l.offset);
            if ( rn )
            {
              need_reg = false;
              s += " ";
              s += rn;
            }
          }
          if ( need_reg )
            s += std::to_string(l.offset);
         }
         snprintf(buf, sizeof(buf), " %lX", l.conv);
         s += buf;
         if ( l.conv )
         {
           std::string ts;
           if ( conv2str(l.conv, ts) )
           {
             s += " (";
             s += ts;
             s += ")";
           }
         }
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
          s += "+";
          s += std::to_string(l.offset);
         }
         break;
        case fpiece:
           s += "piece ";
           s += std::to_string(l.idx);
          break;
        case imp_value:
           s += "implicit_value ";
           s += std::to_string(l.idx);
          break;
        case svalue:
          s += std::to_string(l.sv);
          break;
        case fvalue:
          s += std::to_string(l.idx);
          break;
        case uvalue:
          snprintf(buf, sizeof(buf), "0x%lX", l.conv);
          s += buf;
          break;
        case deref_type:
          s += "deref_type size ";
          s += std::to_string(l.offset);
          snprintf(buf, sizeof(buf), " %lX", l.conv);
          s += buf;
          if ( l.conv )
          {
            std::string ts;
            if ( conv2str(l.conv, ts) )
            {
              s += " (";
              s += ts;
              s += ")";
            }
          }
          break;
        case convert:
          s += "convert_to ";
          snprintf(buf, sizeof(buf), "%lX", l.conv);
          s += buf;
          if ( l.conv )
          {
            std::string ts;
            if ( conv2str(l.conv, ts) )
            {
              s += " (";
              s += ts;
              s += ")";
            }
          }
          break;
        case deref_size:
          s += "OP_deref_size ";
          s += std::to_string(l.idx);
          break;
        case fbreg:
          s += "OP_fbreg ";
          s += std::to_string(l.offset);
         break;
        case plus_uconst:
          s += "OP_plus_uconst ";
          s += std::to_string(l.offset);
         break; 
        case tls_index:
          s += "TlsIndex ";
          s += std::to_string(l.offset);
         break;
        default:
         e_->warning("unknown location op %d\n", l.type);
      }
    }
}

const char *TreeBuilder::locs_no_ops(param_op_type op)
{
  switch(op)
  {
    case call_frame_cfa: return "OP_call_frame_cfa";
    case deref: return "OP_deref";
    case fneg: return "neg";
    case fnot: return "not";
    case Dfabs: return "abs";
    case fand: return "and";
    case fminus: return "minus";
    case f_or: return "or";
    case fplus: return "plus";
    case fshl: return "shl";
    case fshr: return "shr";
    case fshra: return "shra";
    case fxor: return "xor";
    case Dfmul: return "mul";
    case Dfdiv: return "div";
    case Dfmod: return "mod";
    case fstack: return "stack_value";
    default: return nullptr;
  }
}

bool TreeBuilder::get_replaced_name(uint64_t key, std::string &res, unsigned char *ate)
{
  const auto ci = m_replaced.find(key);
  if ( ci == m_replaced.end() )
    return false;
  *ate = ci->second.ate_;
  switch(ci->second.type_)
  {
    case ElementType::typedef2:
    case ElementType::class_type:
    case ElementType::interface_type:
    case ElementType::base_type:
    case ElementType::unspec_type:
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

bool TreeBuilder::get_replaced_name(uint64_t key, std::string &res)
{
   const auto ci = m_replaced.find(key);
   if ( ci == m_replaced.end() )
     return false;
   switch(ci->second.type_)
   {
     case ElementType::typedef2:
     case ElementType::class_type:
     case ElementType::interface_type:
     case ElementType::base_type:
     case ElementType::unspec_type:
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

// return 1 if we want to dump this type
int TreeBuilder::should_keep(Element *e)
{
  if ( !e->dumped_ )
    return 1;
  size_t old_rank = 0;
  auto ns = e->ns_;
  auto ename = e->mangled();
  if ( in_string_pool(ename) )
  {
    UniqName key { e->type_, ename };
    const auto ci = ns->m_dumped_db.find(key);
    if ( ci == ns->m_dumped_db.cend() )
      return 0;
    old_rank = ci->second.second;
  } else {
    if ( !ename )
      return 0;
    UniqName2 key { e->type_, ename };
    const auto ci = ns->m_dumped_db2.find(key);
    if ( ci == ns->m_dumped_db2.cend() )
      return 0;
    old_rank = ci->second.second;
  }
  return e->get_rank() > old_rank;
}

bool TreeBuilder::PostProcessTag()
{
  auto &e = elements_.back();
  // namespace
  if ( current_element_type_ == ElementType::ns_start )
  {
    auto ns = top_ns();
    auto name = e.name_;
    if ( !name )
    {
      e_->warning("namespace without name, tag %lX\n", e.id_);
      name = "";
    }
    auto np = ns->nested.find(name);
    if ( np == ns->nested.end() )
    { // some new namespace
      NSpace *cur = new NSpace();
      cur->ns_el_ = &e;
      cur->parent_ = ns;
      ns->nested.insert(std::pair{name, cur});
      ns_stack.push(cur);
      e.ns_ = cur;
    } else {
      // push existing namespace to ns_stack
      ns_stack.push(np->second);
      e.ns_ = np->second;
    }
    return true;
  }
  return 0 == check_dumped_type(e);
}

int TreeBuilder::check_dumped_type(Element &e)
{
  if ( !exclude_types(current_element_type_, e) ) return 0;
  if ( !e.name_ ) return 0;
  uint64_t rep_id;
  auto ns = e.ns_;
  if ( !ns ) return 0;
  auto name = e.mangled();
  if ( in_string_pool(name) )
  {
    UniqName key { current_element_type_, name };
    const auto ci = ns->m_dumped_db.find(key);
    if ( ci == ns->m_dumped_db.cend() )
      return 0;
    rep_id = ci->second.first;
  } else {
 // fprintf(stderr, "check_dumped_type %p\n", name);
    UniqName2 key { current_element_type_, name };
    const auto ci = ns->m_dumped_db2.find(key);
    if ( ci == ns->m_dumped_db2.cend() )
      return 0;
    rep_id = ci->second.first;
  }
  if ( g_opt_k )
  {
    // we can`t use get_rank here bcs we know only type and name
    // so we can safely replace only basic types
    if ( current_element_type_ != typedef2 && 
         current_element_type_ != base_type &&
         current_element_type_ != unspec_type 
       )
    {
      elements_.back().dumped_ = true;
      return 0;
    }
  }
  // put fake type into m_replaced
  // don`t replace functions
  if ( current_element_type_ != subroutine )
  {
    dumped_type dt { current_element_type_, name, elements_.back().ate_, rep_id };
    m_replaced[elements_.back().id_] = dt;
  } else {
   // mark current function as dumped
   elements_.back().dumped_ = true;
   AddNone();
   sub_filtered = true;
  }
  return 1;
}

uint64_t TreeBuilder::get_replaced_type(uint64_t id) const
{
  const auto ci = m_replaced.find(id);
  if ( ci == m_replaced.cend() )
    return id;
  return ci->second.id;
}

bool TreeBuilder::is_go() const
{
  return cu.cu_lang == Dwarf32::dwarf_source_language::DW_LANG_Go;
}

const char *get_addr_class(int c)
{
  // from https://docs.nvidia.com/cuda/ptx-writers-guide-to-interoperability/index.html
 switch(c) {
  case 1: return "Code storage";
  case 2: return "Register storage";
  case 3: return "Special register storage";
  case 4: return "Constant storage";
  case 5: return "Global storage";
  case 6: return "Local storage";
  case 7: return "Parameter storage";
  case 8: return "Shared storage";
  case 9: return "Surface storage";
  case 10: return "Texture storage";
  case 11: return "Texture sampler storage";
  case 12: return "Generic-address storage";
  default: return nullptr;
 }
}

const char *get_cu_name(int c)
{
  switch (c)
  {
	  /* Ordered by the numeric value of these constants.  */
	case Dwarf32::dwarf_source_language::DW_LANG_C89: return "ANSI C";
	case Dwarf32::dwarf_source_language::DW_LANG_C:	 return "non-ANSI C";
	case Dwarf32::dwarf_source_language::DW_LANG_Ada83:	      return "Ada";
	case Dwarf32::dwarf_source_language::DW_LANG_C_plus_plus:	return "C++";
	case Dwarf32::dwarf_source_language::DW_LANG_Cobol74:		  return "Cobol 74";
	case Dwarf32::dwarf_source_language::DW_LANG_Cobol85:		  return "Cobol 85";
	case Dwarf32::dwarf_source_language::DW_LANG_Fortran77:		return "FORTRAN 77";
	case Dwarf32::dwarf_source_language::DW_LANG_Fortran90:		return "Fortran 90";
	case Dwarf32::dwarf_source_language::DW_LANG_Pascal83:		return "ANSI Pascal";
	case Dwarf32::dwarf_source_language::DW_LANG_Modula2:		  return "Modula 2";
	  /* DWARF 2.1 values.	*/
	case Dwarf32::dwarf_source_language::DW_LANG_Java:		    return "Java";
	case Dwarf32::dwarf_source_language::DW_LANG_C99:		      return "ANSI C99";
	case Dwarf32::dwarf_source_language::DW_LANG_Ada95:		    return "ADA 95";
	case Dwarf32::dwarf_source_language::DW_LANG_Fortran95:		return "Fortran 95";
	  /* DWARF 3 values.  */
	case Dwarf32::dwarf_source_language::DW_LANG_PLI:		      return "PLI";
	case Dwarf32::dwarf_source_language::DW_LANG_ObjC:		    return "Objective C";
	case Dwarf32::dwarf_source_language::DW_LANG_ObjC_plus_plus:	return "Objective C++";
	case Dwarf32::dwarf_source_language::DW_LANG_UPC:		      return "Unified Parallel C";
	case Dwarf32::dwarf_source_language::DW_LANG_D:			      return "D";
	  /* DWARF 4 values.  */
	case Dwarf32::dwarf_source_language::DW_LANG_Python:		  return "Python";
	  /* DWARF 5 values.  */
	case Dwarf32::dwarf_source_language::DW_LANG_OpenCL:		  return "OpenCL";
	case Dwarf32::dwarf_source_language::DW_LANG_Go:		      return "Go";
	case Dwarf32::dwarf_source_language::DW_LANG_Modula3:		  return "Modula 3";
	case Dwarf32::dwarf_source_language::DW_LANG_Haskell:		  return "Haskell";
	case Dwarf32::dwarf_source_language::DW_LANG_C_plus_plus_03:	return "C++03";
	case Dwarf32::dwarf_source_language::DW_LANG_C_plus_plus_11:	return "C++11";
	case Dwarf32::dwarf_source_language::DW_LANG_OCaml:		    return "OCaml";
	case Dwarf32::dwarf_source_language::DW_LANG_Rust:		    return "Rust";
	case Dwarf32::dwarf_source_language::DW_LANG_C11:		      return "C11";
	case Dwarf32::dwarf_source_language::DW_LANG_Swift:		    return "Swift";
	case Dwarf32::dwarf_source_language::DW_LANG_Julia:		    return "Julia";
	case Dwarf32::dwarf_source_language::DW_LANG_Dylan:		    return "Dylan";
	case Dwarf32::dwarf_source_language::DW_LANG_C_plus_plus_14:	return "C++14";
	case Dwarf32::dwarf_source_language::DW_LANG_Fortran03:		return "Fortran 03";
	case Dwarf32::dwarf_source_language::DW_LANG_Fortran08:		return "Fortran 08";
	case Dwarf32::dwarf_source_language::DW_LANG_RenderScript:	return "RenderScript";
  case Dwarf32::dwarf_source_language::DW_LANG_BLISS:       return "BLISS";
  case Dwarf32::dwarf_source_language::DW_LANG_Kotlin:      return "Kotlin";
  case Dwarf32::dwarf_source_language::DW_LANG_Zig:         return "Zig";
  case Dwarf32::dwarf_source_language::DW_LANG_Crystal:     return "Crystal";
  case Dwarf32::dwarf_source_language::DW_LANG_C_plus_plus_17: return "C++17";
  case Dwarf32::dwarf_source_language::DW_LANG_C_plus_plus_20: return "C++20";
  case Dwarf32::dwarf_source_language::DW_LANG_C17:         return "C17";
  case Dwarf32::dwarf_source_language::DW_LANG_Fortran18:   return "Fortran 2018";
  case Dwarf32::dwarf_source_language::DW_LANG_Ada2005:     return "Ada 2005";
  case Dwarf32::dwarf_source_language::DW_LANG_Ada2012:     return "Ada 2012";
  case Dwarf32::dwarf_source_language::DW_LANG_GOOGLE_RenderScript: return "Google RenderScript";
  case Dwarf32::dwarf_source_language::DW_LANG_BORLAND_Delphi: return "Delphi";
	  /* asm  */
	case Dwarf32::dwarf_source_language::DW_LANG_Mips_Assembler:	return "assembler";
	  /* UPC extension.  */
	case Dwarf32::dwarf_source_language::DW_LANG_Upc:		return "Unified Parallel C";
  }
  return nullptr;
}

void TreeBuilder::put_file_hdr(struct cu *c)
{
  if ( m_hdr_dumped )
    return;
  if ( c->cu_name )
    fprintf(g_outf, "\n// Name: %s\n", c->cu_name);
  if ( c->cu_comp_dir )
    fprintf(g_outf, "// CompDir: %s\n", c->cu_comp_dir);
  if ( c->cu_lang )
  {
    auto lang = get_cu_name(c->cu_lang);
    if ( lang )
      fprintf(g_outf, "// Language: %s\n", lang);
    else
      fprintf(g_outf, "// Language: 0x%X\n", c->cu_lang);
  }
  if ( c->cu_package )
    fprintf(g_outf, "// Package: %s\n", c->cu_package);
  if ( c->cu_producer )
    fprintf(g_outf, "// Producer: %s\n", c->cu_producer);
  if ( c->cu_base_addr )
    fprintf(g_outf, "// base_addr: %lX\n", c->cu_base_addr);
  m_hdr_dumped = true;
}

void TreeBuilder::put_file_hdr()
{
  if ( !g_opt_v )
    return;
  if ( elements_.empty() )
    return;
  put_file_hdr(&cu);
}

// called on each processed compilation unit
void TreeBuilder::ProcessUnit(int last)
{
  if ( !m_stack.empty() )
  {
    e_->warning("ProcessUnit: stack is not empty\n");
    m_stack = {};
  }
  m_hdr_dumped = false;
  RenderUnit(last);
  if ( !g_opt_g )
  {
    merge_dumped();
    m_go_attrs.clear();
    m_lvalues.clear();
    m_rng.clear(); m_rng2.clear();
  }
  if ( is_go() && !g_opt_g )
    collect_go_types();
  elements_.clear();
  m_replaced.clear();
  cu.cu_name = cu.cu_comp_dir = cu.cu_producer = cu.cu_package = NULL;
  cu.cu_lang = 0;
  cu.cu_base_addr = cu.cu_base_addr_idx = 0;
  cu.need_base_addr_idx = false;
  ns_count = 0;
  recent_ = nullptr;
}

void TreeBuilder::set_range(uint64_t off, unsigned char addr_size)
{
  if ( current_element_type_ != ElementType::subroutine )
    return;
  if ( elements_.empty() ) {
    e_->warning("Can't set range when element list is empty\n");
    return;
  }
  auto &f = elements_.back();
  f.has_range_ = true;
  if ( has_rngx )
    m_rng2[ f.id_ ] = off;
  else
    m_rng[ f.id_ ] = { off, cu.cu_base_addr, addr_size};
}

bool TreeBuilder::lookup_range(uint64_t tag, std::list<std::pair<uint64_t, uint64_t> > &res)
{
  if ( !m_locX ) return false;
  if ( has_rngx )
  {
    auto r2 = m_rng2.find(tag);
    if ( r2 == m_rng2.end() ) return false;
    return m_locX->get_rnglistx(r2->second, 0, 0, res);
  } else {
    auto r = m_rng.find(tag);
    if ( r == m_rng.end() ) return false;
    return m_locX->get_rnglistx(r->second.off, r->second.base, r->second.addr_size, res);
  }
}

void TreeBuilder::AddNone() {
  current_element_type_ = ElementType::none;
}

int TreeBuilder::add2stack(int regged)
{
  if ( !regged ) {
    m_stack.push(nullptr);
    return 1;
  }
  if (!elements_.size()) {
    // fprintf(stderr, "Can't add a member if the element list is empty\n");
    return 0;
  }
  auto &last = elements_.back();
  if ( last.type_ == ns_start && !recent_ )
  {
    ns_count++;
    if ( g_opt_v )
      fprintf(g_outf, "// ns_start %d at %lX\n", ns_count, last.id_);
  } else if ( last.type_ == lexical_block && !recent_ ) {
    // fprintf(g_outf, "// lexical_block %d at %lX\n", ns_count, last.id_);
    ns_count++;
  }
  if ( recent_ )
    m_stack.push(recent_);
  else
    m_stack.push( &last );
#if DEBUG
printf("add2stack: m_stack size %ld top %p (%s) top->owner_ %p id %lX\n", m_stack.size(), m_stack.top(), m_stack.top()->TypeName(), 
  m_stack.top()->owner_, m_stack.top()->id_);
#endif
  return 1;
}

void TreeBuilder::pop_stack(uint64_t off)
{
  // printf("pop_stack %lX, size %ld\n", off, m_stack.size());
  if ( m_stack.empty() ) {
    // fprintf(stderr, "stack is empty\n");
    return;
  }
  auto last = m_stack.top();
  if ( !last ) {
    m_stack.pop();
    recent_ = nullptr;
    return;
  }
//  printf( "top %d %lX\n", last->type_, last->id_);
  if ( last->type_ == ns_start )
  {
    ns_count--;
    elements_.push_back(Element(ns_end, last->type_id_, last->level_, get_owner(), top_ns()));
    elements_.back().name_ = last->name_;
    if ( ns_stack.empty() )
      e_->warning("ns stack is empty, off %lX, ns_count %d\n", off, ns_count);
    else
      ns_stack.pop();
    if ( g_opt_v )
      fprintf(g_outf, "// ns_end %s %d off %lX\n", last->name_, ns_count, off);
  } else if ( last->type_ == lexical_block )
  {
    // fprintf(g_outf, "// pop lexical_block %lX, ns_count %d\n", last->id_, ns_count);
    ns_count--;
  }
  m_stack.pop();
  recent_ = nullptr;
}

int TreeBuilder::can_have_methods(int level)
{
  if ( m_stack.empty() )
    return 0;
  auto last = m_stack.top();
  if ( !last ) return 0;
  if ( last->type_ == ns_start || last->type_ == subroutine_type || last->type_ == subroutine )
    return 0;
  return (level > 1);
}

bool TreeBuilder::AddVariant()
{
  if ( m_stack.empty() ) {
    e_->warning("Can't add a variant when stack is empty\n");
    return false;
  }
  auto top = m_stack.top();
  return top->type_ == ElementType::variant_type;
}

// formal parameter - level should be +1 to parent
bool TreeBuilder::AddFormalParam(uint64_t tag_id, int level, bool ell) 
{
  level -= ns_count;
  current_element_type_ = ElementType::formal_param;
  if ( m_stack.empty() ) {
    e_->warning("Can't add a formal parameter when stack is empty\n");
    return false;
  }
  auto top = m_stack.top();
  if ( !top ) return false;
  // fprintf(g_outf, "f %lX level %d level %d ns_count %d\n", top->id_, top->level_, level, ns_count);
  if ( top->level_ != level - 1 )
    return false;
  if ( !top->m_comp )
    top->m_comp = new Compound();
  
  top->m_comp->params_.push_back({NULL, tag_id, 0, ell, false});
  return true;
}

void TreeBuilder::SetDiscr(uint64_t v)
{
  if ( current_element_type_ != ElementType::variant_type )
    return;
  if ( elements_.empty() ) {
    e_->warning("Can't add a discr when element list is empty\n");
    return;
  }
  elements_.back().type_id_ = v;
}

void TreeBuilder::SetParentAccess(int a)
{
  if (current_element_type_ == ElementType::none) {
    return;
  }

  if ( m_stack.empty() ) {
    e_->warning("Can't add a access when stack is empty\n");
    return;
  }
  auto top = m_stack.top();
  if ( current_element_type_ == ElementType::member )
  {
    auto el = get_member("access");
    if ( el ) el->access_ = a;
    return;
  } else {
    if ( recent_ )
      top = recent_;
    if ( !top->m_comp || top->m_comp->parents_.empty() ) {
      e_->warning("Can't add a parent access when parents list is empty\n");
      return;
    }
    top->m_comp->parents_.back().access = a;
  }
}

int TreeBuilder::mark_has_go(uint64_t id)
{
  if ( elements_.empty() ) {
    e_->warning("Can't mark go attr when elements is empty\n");
    return 0;
  }
  auto &el = elements_.back();
  if ( el.id_ == id ) {
    el.has_go = true;
    return 1;
  }
  // check member
  if ( current_element_type_ == ElementType::member ) {
    if ( m_stack.empty() ) {
        e_->warning("Can't mark go attr when stack is empty\n");
        return 0;
    }
    auto top = m_stack.top();
    if ( !top->m_comp ) return 0;
    auto &m = top->m_comp->members_.back();
    if ( m.id_ == id ) {
      m.has_go = true;
      return 1;
    }
    return 0;
  }
  return 0;
}

void TreeBuilder::AddElement(ElementType element_type, uint64_t tag_id, int level) {
  level -= ns_count;
  // fprintf(g_outf, "AddElement %d id %lX level %d ns_count %d last_var %p\n", element_type, tag_id, level, ns_count, last_var_);
  last_var_ = nullptr;
  auto ns = top_ns();
  switch(element_type) {
    case ElementType::variant_type:
    case ElementType::member:       // Member
      if (current_element_type_ == ElementType::none) {
        return;
      }
      if ( m_stack.empty() ) {
        e_->warning("Can't add a member when stack is empty\n");
        return;
      }
      if (elements_.empty()) {
        e_->warning("Can't add a member if the element list is empty\n");
        return;
      } else {
        auto &top = m_stack.top();
        if ( !top->m_comp )
          top->m_comp = new Compound();
        top->m_comp->members_.push_back(Element(element_type, tag_id, level, get_owner(), nullptr));
      }
      if ( element_type == ElementType::variant_type )
      {
        auto &top = m_stack.top();
        top->m_comp->members_.back().type_id_ = tag_id;
        elements_.push_back(Element(element_type, tag_id, level, get_owner(), ns));
        ns->empty = false;
        recent_ = nullptr;
      }
      break;
    case ElementType::inheritance:    // Parent
      if (current_element_type_ == ElementType::none) {
        return;
      }
      if (elements_.empty()) {
        e_->warning("Can't add a parent if the element list is empty\n");
        return;
      }
      if ( m_stack.empty() ) {
        e_->warning("Can't add a parent when stack is empty\n");
        return;
      } else {
        auto &top = m_stack.top();
        if ( !top->m_comp )
          top->m_comp = new Compound();
        top->m_comp->parents_.push_back({tag_id, 0});
      }
      break;
    // Subrange
    case ElementType::subrange_type:
      break;                          // Just update the current element type

    // part of enum - check that parent is 
    case ElementType::enumerator:
      if (current_element_type_ == ElementType::none) {
        return;
      }
      if (elements_.empty()) {
        e_->warning("Can't add a enumerator if the element list is empty\n");
        return;
      }
      if ( m_stack.empty() ) {
        e_->warning("Can't add a enumerator when stack is empty\n");
        return;
      } else {
        auto &top = m_stack.top();
        if ( !top->m_comp )
          top->m_comp = new Compound();
        top->m_comp->enums_.push_back({NULL, 0});
      }
      break;

    case ElementType::var_type:
      last_var_ = recent_ = nullptr;
      if ( level > 1 && !m_stack.empty() ) // this is local var
      {
        auto &top = m_stack.top();
        if ( !top || sub_filtered ) {
          current_element_type_ = ElementType::none;
          return;
        }
        auto owner = get_top_func();
        if ( !owner )
        {
          owner = m_stack.top();
          if ( !owner || !owner->can_have_static_vars() )
          {
            if ( !owner )
              e_->warning("Can't add a local var tag %lX when there is no top function\n", tag_id);
            else if ( owner->type_ != ElementType::lexical_block )
              e_->warning("Can't add a local var tag %lX when there is no top function, owner %s\n", tag_id, owner->TypeName());
            current_element_type_ = ElementType::var_type;
            return;
          }
        }
        elements_.push_back(Element(element_type, tag_id, level, owner, ns));
        ns->empty = false;
        last_var_ = &elements_.back();
        if ( !owner->m_comp )
          owner->m_comp = new Compound(); // valgring points here as leak 
        if ( owner->type_ == ElementType::method && g_opt_v )
          e_->warning("add var %lX to method %lX\n", tag_id, owner->id_);
        owner->m_comp->lvars_.push_back(last_var_); // valgring points here as leak
      } else {
        // some top-level var
        auto owner = get_owner();
        if ( owner && owner->type_ == ElementType::var_type )
        {
          e_->warning("BUG: var id %lX wants to stack on var id %lX\n", tag_id, owner->id_);
          current_element_type_ = ElementType::none;
          return;
        }
        elements_.push_back(Element(element_type, tag_id, level, owner, ns));
        ns->empty = false;
        last_var_ = &elements_.back();
      }
      break;

    case ElementType::subroutine:
      sub_filtered = false;
      if ( can_have_methods(level) )
      {
        auto &top = m_stack.top();
        if ( !top->m_comp )
          top->m_comp = new Compound();
        top->m_comp->methods_.push_back(Method(tag_id, level, get_owner(), nullptr));
        // fprintf(g_outf, "add method to %s parent tid %lX type %d tid %lX\n", top->name_, top->id_, top->type_, tag_id);
        current_element_type_ = ElementType::method;
        recent_ = &top->m_comp->methods_.back();
        return;
      }
      // fall to default
    default:
      elements_.push_back(Element(element_type, tag_id, level, get_owner(), ns));
      ns->empty = false;
      if ( element_type == ElementType::subroutine ||
           element_type == ElementType::subroutine_type
         )
        recent_ = &elements_.back();
      else
        recent_ = nullptr;
  }

  current_element_type_ = element_type;
}

void TreeBuilder::SetFilename(std::string &fn, const char *fname)
{
  if (current_element_type_ == ElementType::none)
    return;
  if (!elements_.size()) {
    e_->warning("Can't set an file name if the element list is empty\n");
    return;
  }
  if ( current_element_type_ == ElementType::var_type )
  {
    if ( !last_var_ )
    {
      e_->warning("Can't set an file name when no last_var\n");
      return;
    }
    last_var_->fname_ = fname;
    last_var_->fullname_ = std::move(fn);
    return;
  }
  if ( recent_ )
  {
    recent_->fname_ = fname;
    recent_->fullname_ = std::move(fn);
  } else {
    elements_.back().fname_ = fname;
    elements_.back().fullname_ = std::move(fn);
  }
}

void TreeBuilder::SetLinkageName(const char* name)
{
  if (current_element_type_ == ElementType::none)
    return;
  if (elements_.empty()) {
    e_->warning("Can't set an linkage name if the element list is empty\n");
    return;
  }
  if ( current_element_type_ == ElementType::var_type )
  {
    if ( !last_var_ )
    {
      e_->warning("Can't set an linkage name when no last_var\n");
      return;
    }
    last_var_->link_name_ = name;
    return;
  }
  if ( recent_ )
    recent_->link_name_ = name;
  else
    elements_.back().link_name_ = name;
}

void TreeBuilder::SetTlsIndex(param_loc *pl)
{
  if (current_element_type_ != ElementType::var_type)
    return;
  if (elements_.empty()) {
    e_->warning("Can't set an tls index if the element list is empty\n");
    return;
  }
  auto *top = &elements_.back();
  if ( last_var_ )
    top = last_var_;
  m_tls[top->id_] = pl->locs.front().offset;
}

TreeBuilder::FormalParam* TreeBuilder::get_param(const char *why)
{
  if (elements_.empty()) {
    e_->warning("Can't set an parameter %s if the element list is empty\n", why);
    return nullptr;
  }
  auto &top = m_stack.top();
  if ( recent_ )
    top = recent_;
  if (!top->m_comp || top->m_comp->params_.empty()) {
    e_->warning("Can't set the parameter %s if the params list is empty\n", why);
    return nullptr;
  }
  return &top->m_comp->params_.back();
}

void TreeBuilder::SetLocation(param_loc *pl)
{
  if (current_element_type_ != ElementType::formal_param)
    return;
  auto p = get_param("location");
  if ( p ) p->loc = *pl;
}

void TreeBuilder::SetParamDirection(unsigned char c)
{
  if (current_element_type_ != ElementType::formal_param)
    return;
  auto p = get_param("direction");
  if ( p ) p->pdir = c;
}

void TreeBuilder::SetOptionalParam()
{
  if (current_element_type_ != ElementType::formal_param)
    return;
  auto p = get_param("optional");
  if ( p ) p->optional_ = true;
}

void TreeBuilder::SetVarParam(bool v)
{
  if (current_element_type_ != ElementType::formal_param)
    return;
  auto p = get_param("variable");
  if ( p ) p->var_ = v;
}

void TreeBuilder::SetElementName(const char* name, uint64_t off)
{
  if (current_element_type_ == ElementType::none) {
    return;
  }
  if (elements_.empty()) {
    e_->warning("Can't set an element name if the element list is empty, offset %lX\n", off);
    return;
  }

  if ( current_element_type_ == ElementType::formal_param )
  {
    if ( m_stack.empty() ) {
      e_->warning("Can't set the formal param name when stack is empty\n");
      return;
    }
    auto top = m_stack.top();
    // check that top is subroutine or method
    if ( top->type_ != ElementType::subroutine && top->type_ != ElementType::method )
    {
      e_->warning("Can't set the formal param name if the top element is %s, offset %lX\n", top->TypeName(), off);
      return;
    }
    if ( !top->m_comp || top->m_comp->params_.empty()) {
      e_->warning("Can't set the formal param name to %lX when the params list is empty, offset %lX\n", top->id_, off);
      return;
    }

    top->m_comp->params_.back().name = name;
    return;
  }
  if ( current_element_type_ == ElementType::enumerator )
  {
    if ( m_stack.empty() ) {
      e_->warning("Can't set the enum name when stack is empty\n");
      return;
    }
    auto top = m_stack.top();
    if (!top->m_comp || top->m_comp->enums_.empty()) {
      e_->warning("Can't set the enum name if the enums list is empty\n");
      return;
    }
    top->m_comp->enums_.back().name = name;
    return;
  }

  if (current_element_type_ == ElementType::var_type) 
  {
    if ( !last_var_ )
    {
      e_->warning("Can't set the var name when there is no last_var\n");
      return;
    }
    last_var_->name_ = name;
    return;
  }

  if (current_element_type_ == ElementType::member) {
    auto el = get_member("name");
    if ( el ) el->name_ = name;
    return;
  }
  if ( recent_ )
    recent_->name_ = name;
  else
    elements_.back().name_ = name;
}

void TreeBuilder::SetElementSize(uint64_t size) {
   if (current_element_type_ == ElementType::none) {
    return;
  }
  if (elements_.empty()) {
    e_->warning("Can't set an element size if the element list is empty\n");
    return;
  }

  if (current_element_type_ == ElementType::member) {
    auto *el = get_member("size");
    if ( el ) el->size_ = size;
    return;
  }
  if ( recent_ )
    recent_->size_ = size;
  else
    elements_.back().size_ = size;
}

void TreeBuilder::SetAddressClass(int v, uint64_t off)
{
  if ( m_stack.empty() ) {
    e_->warning("Can't set the address class when stack is empty, offset %lX\n", off);
    return;
  }
  if (current_element_type_ == ElementType::member )
  {
    auto el = get_member("address class");
    if ( el ) el->addr_class_ = v;
    return;
  }
  elements_.back().addr_class_ = v;
}

void TreeBuilder::SetBitSize(int v)
{
   if (current_element_type_ != ElementType::member) {
    return;
  }
  auto *el = get_member("bit size");
  if ( el ) el->bit_size_ = v;
}

void TreeBuilder::SetVtblIndex(uint64_t v)
{
  if ( !recent_ )
    return;
  if ( ElementType::method != recent_->type_ )
    return;
  Method *m = static_cast<Method *>(recent_);
  m->vtbl_index_ = v;
}

void TreeBuilder::SetObjPtr(uint64_t v)
{
  if ( !recent_ )
    return;
  if ( ElementType::method != recent_->type_ )
    return;
  Method *m = static_cast<Method *>(recent_);
  m->this_arg_ = v;
}

void TreeBuilder::SetVirtuality(int v)
{
  // parents too can be virtual
  if ( current_element_type_ == ElementType::inheritance )
  {
    auto p = get_parent("virtuality");
    if ( p ) p->virtual_ = (v != 0);
    return;
  }
  if ( !recent_ )
    return;
  if ( ElementType::method != recent_->type_ )
    return;
  Method *m = static_cast<Method *>(recent_);
  m->virt_ = v;
}

void TreeBuilder::SetAte(unsigned char v)
{
   if (current_element_type_ == ElementType::none) {
    return;
  }
  if (elements_.empty()) {
    e_->warning("Can't set Ate if the element list is empty\n");
    return;
  }
  if ( recent_ )
    recent_->ate_ = v;
  else
    elements_.back().ate_ = v;
}

void TreeBuilder::SetNoReturn()
{
   if (current_element_type_ == ElementType::none) {
    return;
  }
  if (elements_.empty()) {
    e_->warning("Can't set an noreturn attribute if the element list is empty\n");
    return;
  }
  if ( recent_ )
    recent_->noret_ = true;
  else
    elements_.back().noret_ = true;
}

void TreeBuilder::SetArtiticial()
{
  if ( !recent_ )
    return;
  if ( current_element_type_ == ElementType::formal_param )
  {
    if ( m_stack.empty() ) {
      e_->warning("Can't set Artiticial for formal param when stack is empty\n");
      return;
    }
    auto top = m_stack.top();
    if ( top->type_ == ElementType::subroutine )
      return;
    // check that top is subroutine or method
    if ( top->type_ != ElementType::method && top->type_ != ElementType::subroutine_type )
    {
      e_->warning("Can't set Artiticial for formal param if the top element tag %lX is not method (%s)\n", top->id_,  top->TypeName());
      return;
    }
    if ( !top->m_comp || top->m_comp->params_.empty()) {
      e_->warning("Can't set Artiticial for formal param to %lX when the params list is empty\n", top->id_);
      return;
    }
    auto &fp = top->m_comp->params_.back();
    fp.art_ = true;
    if ( top->type_ == ElementType::method )
    {
      Method *m = static_cast<Method *>(top);
      if ( !m->this_arg_ )
        m->this_arg_ = fp.param_id;
    }
    return;
  }
  if ( current_element_type_ != ElementType::method )
    return;
  Method *m = static_cast<Method *>(recent_);
  m->art_ = true;
}

void TreeBuilder::SetConstExpr()
{
  if ( current_element_type_ != ElementType::var_type )
    return;
  if ( !last_var_ )
  {
    e_->warning("Can't set an const_expr attribute when there is no last_var\n");
    return;
  }
  last_var_->const_expr_ = true;
}

void TreeBuilder::SetEnumClass()
{
  if ( current_element_type_ != ElementType::enumerator_type )
    return;
  if ( elements_.empty() )
  {
    e_->warning("Can't set an enum_class attribute when element list is empty\n");
    return;
  }
  elements_.back().enum_class_ = true;
}

void TreeBuilder::SetGNUVector()
{
  if ( current_element_type_ != ElementType::array_type )
    return;
  if ( elements_.empty() )
  {
    e_->warning("Can't set an GNU_vector attribute when element list is empty\n");
    return;
  }
  elements_.back().gnu_vector_ = true;
}

void TreeBuilder::SetTensor()
{
  if ( current_element_type_ != ElementType::array_type )
    return;
  if ( elements_.empty() )
  {
    e_->warning("Can't set an tensor attribute when element list is empty\n");
    return;
  }
  elements_.back().tensor_ = true;
}

void TreeBuilder::SetExplicit()
{
  if ( !recent_ || current_element_type_ != ElementType::method )
    return;
  Method *m = static_cast<Method *>(recent_);
  m->expl_ = true;
}

void TreeBuilder::SetRef_()
{
  if ( !recent_ || current_element_type_ != ElementType::method )
    return;
  Method *m = static_cast<Method *>(recent_);
  m->ref_ = true;
}

void TreeBuilder::SetRValRef_()
{
  if ( !recent_ || current_element_type_ != ElementType::method )
    return;
  Method *m = static_cast<Method *>(recent_);
  m->rval_ref_ = true;
}

void TreeBuilder::SetDefaulted()
{
  if ( !recent_ || current_element_type_ != ElementType::method )
    return;
  Method *m = static_cast<Method *>(recent_);
  m->def_ = true;
}

void TreeBuilder::SetDeclaration()
{
   if (current_element_type_ == ElementType::none) {
    return;
  }
  if (elements_.empty()) {
    e_->warning("Can't set an declaration attribute if the element list is empty\n");
    return;
  }
  auto &last = elements_.back();
  if ( last.can_have_decl() )
    last.decl_ = true;
}

void TreeBuilder::SetBitOffset(int v)
{
   if (current_element_type_ != ElementType::member) {
    return;
  }
  auto *el = get_member("bit offset");
  if ( el ) el->bit_offset_ = v;
}

TreeBuilder::Element *TreeBuilder::get_member(const char *why)
{
  if ( m_stack.empty() ) {
    e_->warning("Can't set the member %s when stack is empty\n", why);
    return nullptr;
  }
  auto top = m_stack.top();
  if (!top->m_comp || top->m_comp->members_.empty()) {
    e_->warning("Can't set the member %s if the members list is empty\n", why);
    return nullptr;
  }
  return &top->m_comp->members_.back();
}

TreeBuilder::Parent *TreeBuilder::get_parent(const char *why)
{
  if ( m_stack.empty() ) {
    e_->warning("Can't set the parent %s when stack is empty\n", why);
    return nullptr;
  }
  auto top = m_stack.top();
  if (!top->m_comp || top->m_comp->parents_.empty()) {
    e_->warning("Can't set the parent %s if the parents list is empty\n", why);
    return nullptr;
  }
  return &top->m_comp->parents_.back();
}

void TreeBuilder::SetElementOffset(uint64_t offset) {
   if (current_element_type_ == ElementType::none) {
    return;
  }
  if (elements_.empty()) {
    e_->warning("Can't set an element offset if the element list is empty\n");
    return;
  }
  Parent *p;
  Element *el;
  switch (current_element_type_) {
    case ElementType::member:
      el = get_member("offset");
      if ( el ) el->offset_ = offset;
      break;
    case ElementType::inheritance:
      p = get_parent("offset");
      if ( p ) p->offset = offset;
      break;
    default:
      break;
  }
}

void TreeBuilder::SetElementType(uint64_t type_id) {
  if (current_element_type_ == ElementType::none) {
    return;
  }
  if (elements_.empty()) {
    e_->warning("Can't set an element type if the element list is empty\n");
    return;
  }
  
  switch (current_element_type_) {
    case ElementType::member:
      if ( m_stack.empty() ) {
        e_->warning("Can't set the member type when stack is empty\n");
        return;
      } else {
        auto top = m_stack.top();
        if (!top->m_comp || top->m_comp->members_.empty()) {
          e_->warning("Can't set the member type if the members list is empty\n");
          break;
        }
        top->m_comp->members_.back().type_id_ = type_id;
      }
      break;
    case ElementType::inheritance:
      if ( m_stack.empty() ) {
        e_->warning("Can't set the parent type when stack is empty\n");
        return;
      } else {
        auto top = m_stack.top();
        if (!top->m_comp || top->m_comp->parents_.empty()) {
          e_->warning("Can't set the parent type if the parents list is empty\n");
          break;
        }
        top->m_comp->parents_.back().id = type_id;
      }
      break;
    case ElementType::formal_param:
      if ( m_stack.empty() ) {
        e_->warning("Can't set the formal param type when stack is empty\n");
        return;
      } else {
        auto top = m_stack.top();
        if (!top->m_comp || top->m_comp->params_.empty() )
        {
          e_->warning("Can't set the formal param type if the params list is empty\n");
          break;
        }
        top->m_comp->params_.back().id = type_id;
      }
      break;
    case ElementType::subrange_type:
      break; // do nothing
    case ElementType::var_type:
       if ( !last_var_ )
       {
        e_->warning("Can't set the var type %lx when no last_var\n", type_id);
        return;
       }
       last_var_->type_id_ = type_id;
      break;
    default:
      if ( recent_)
        recent_->type_id_ = type_id;
      else
        elements_.back().type_id_ = type_id;
      break;
  }
}

void TreeBuilder::SetAlignment(uint64_t v)
{
  if ( current_element_type_ == ElementType::class_type ||
       current_element_type_ == ElementType::interface_type ||
       current_element_type_ == ElementType::structure_type ||
       current_element_type_ == ElementType::union_type
  )
  {
    if (elements_.empty()) {
      e_->warning("Can't set alignment when element list is empty\n");
      return;
    }
    elements_.back().align_ = v;
  }
}

void TreeBuilder::SetContainingType(uint64_t ct)
{
  if (elements_.empty()) {
    e_->warning("Can't set ContainingType when element list is empty\n");
    return;
  }
  elements_.back().cont_type_ = ct;
}

void TreeBuilder::SetLocX(uint64_t ct)
{
  if (elements_.empty())
    return;
  if ( current_element_type_ == ElementType::var_type )
  {
    if ( !last_var_ )
    {
      e_->warning("Can't set loclistx when there is no last_var\n");
      return;
    }
    last_var_->locx_ = ct;
    return;
  }
}

void TreeBuilder::SetAbs(uint64_t ct)
{
  // formal paraameters also can have abstract_origin
  if (current_element_type_ != ElementType::subroutine &&
      current_element_type_ != ElementType::method &&
      current_element_type_ != ElementType::var_type
     )
    return;
  if ( current_element_type_ == ElementType::var_type )
  {
    if ( !last_var_ )
    {
      e_->warning("Can't set abstract_origin when there is no last_var\n");
      return;
    }
    last_var_->abs_ = ct;
    return;
  }
  if (elements_.empty()) {
    e_->warning("Can't set abstract_origin when element list is empty\n");
    return;
  }
  // fprintf(g_outf, "SetAbs %lX to %lX\n", ct, elements_.back().id_);
  if ( recent_ )
    recent_->abs_ = ct;
  else
    elements_.back().abs_ = ct;
}

void TreeBuilder::SetInlined(int ct)
{
  if (current_element_type_ != ElementType::subroutine &&
      current_element_type_ != ElementType::method
     )
    return;
  if (elements_.empty()) {
    e_->warning("Can't set inlined when element list is empty\n");
    return;
  }
  if ( recent_)
    recent_->inlined_ = ct;
  else
    elements_.back().inlined_ = ct;
}

void TreeBuilder::SetSpec(uint64_t ct)
{
  if (elements_.empty()) {
    e_->warning("Can't set specification when element list is empty\n");
    return;
  }
  if ( current_element_type_ == ElementType::var_type )
  {
    if ( !last_var_ )
    {
      e_->warning("Can't set specification when there is no last_var\n");
      return;
    }
    last_var_->spec_ = ct;
    return;
  }  
  // fprintf(g_outf, "spec %lX for %lX\n", ct, elements_.back().id_);
  elements_.back().spec_ = ct;
}

void TreeBuilder::SetAddr(uint64_t count)
{
  if ( current_element_type_ == ElementType::var_type )
  {
    if ( !last_var_ )
    {
      e_->warning("Can't set address when there is no last_var\n");
      return;
    }
    last_var_->addr_ = count;
    store_addr(last_var_, count);
    return;
  }
  if (current_element_type_ != ElementType::subroutine &&
      current_element_type_ != ElementType::method
     )
     return;
  if (elements_.empty()) {
    e_->warning("Can't set address when element list is empty\n");
    return;
  }
  if ( recent_ ) {
    recent_->addr_ = count;
    store_addr(recent_, count);
  } else {
    auto &e = elements_.back();
    e.addr_ = count;
    store_addr(&e, count);
  }
}

void TreeBuilder::SetVarConstValue(uint64_t v)
{
  if (current_element_type_ != ElementType::var_type) {
    return;
  }
  if ( !last_var_ )
  {
    e_->warning("Can't set var ConstValue when there is no last_var\n");
    return;
  }
  m_lvalues[last_var_] = v;
}

void TreeBuilder::SetConstValue(uint64_t count) {
  if (current_element_type_ != ElementType::enumerator) {
    return;
  }
  if ( m_stack.empty() ) {
    e_->warning("Can't set ConstValue when stack is empty\n");
    return;
  }
  auto top = m_stack.top();
  if ( !top->m_comp || top->m_comp->enums_.empty()) {
    e_->warning("Can't set ConstValue if the enums list is empty\n");
    return;
  }
  top->m_comp->enums_.back().value = count;
}

void TreeBuilder::SetElementCount(uint64_t count) {
  if (current_element_type_ != ElementType::subrange_type) {
    return;
  }
  if ( elements_.empty()) {
    e_->warning("Can't set an element count if the element list is empty\n");
    return;
  }
  if ( recent_ )
    recent_->count_ = count;
  else
    elements_.back().count_ = count;
}

void TreeBuilder::SetLocVarLocation(param_loc *pl)
{
  if ( current_element_type_ != var_type )
    return;
  if ( !last_var_ )
  {
    e_->warning("Can't set local var location when there is no last_var\n");
    return;
  }
  auto t = get_top_func();
  if ( !t )
  {
    e_->warning("Can't set local var location when there is no top_func\n");
    return;
  }
  if ( !t->m_comp )
  {
    e_->warning("Can't set local var location when there is no comp in top_func\n");
    return;
  }
  t->m_comp->lvar_locs_[last_var_] = *pl;
}

TreeBuilder::Element *TreeBuilder::get_owner()
{
  if ( m_stack.empty() )
    return nullptr;
  return m_stack.top();
}

TreeBuilder::Element *TreeBuilder::get_top_func() const
{
  if ( m_stack.empty() )
    return nullptr;
  auto t = m_stack.top();
  if ( !t ) return nullptr;
#if DEBUG
printf("top %p size %ld type %s owner %p\n", t, m_stack.size(), t->TypeName(), t->owner_); fflush(stdout);
#endif
  do
  {
    if ( t->type_ == ElementType::method || t->type_ == ElementType::subroutine )
      return t;
    if ( t == t->owner_ )
    {
      e_->error("BUG: t %s id %lx m_stack %ld has ref to itself\n", t->TypeName(), t->id_, m_stack.size());
      break;
    }
    t = t->owner_;
  } while(t);
  return nullptr;
}

bool TreeBuilder::is_local_var() const
{
  if ( current_element_type_ != var_type )
    return false;
  return nullptr != get_top_func();
}

int TreeBuilder::is_signed_ate(unsigned char ate)
{
  switch(ate)
  {
    case Dwarf32::dwarf_ate::DW_ATE_address:
    case Dwarf32::dwarf_ate::DW_ATE_boolean:
    case Dwarf32::dwarf_ate::DW_ATE_unsigned:
    case Dwarf32::dwarf_ate::DW_ATE_unsigned_char:
    case Dwarf32::dwarf_ate::DW_ATE_unsigned_fixed:
      return false;
  }
  return true;
}

const char* TreeBuilder::Element::TypeName() const {
  switch (type_) {
    case ElementType::none: return "none";
    case ElementType::array_type: return "array";
    case ElementType::class_type: return "class";
    case ElementType::interface_type: return "interface";
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
    case ElementType::dynamic_type: return "dynamic";
    case ElementType::atomic_type: return "atomic";
    case ElementType::immutable_type: return "immutable";
    case ElementType::reference_type: return "reference";
    case ElementType::rvalue_ref_type: return "rvalue_reference";
    case ElementType::subroutine_type: return "function_type";
    case ElementType::subroutine: return "function";
    case ElementType::method: return "method";
    case ElementType::ptr2member: return "ptr2member";
    case ElementType::unspec_type: return "unspec_type";
    case ElementType::ns_start: return "namespace";
    case ElementType::lexical_block: return "lexical_block";
    case ElementType::var_type: return "var";
    case ElementType::variant_type: return "variant_type";
    default: return "unk";
  }
}
