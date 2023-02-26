#include "TreeBuilder.h"
#include "dwarf32.h"

TreeBuilder::TreeBuilder() = default;
TreeBuilder::~TreeBuilder() = default;

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
    if ( e.type_ == ns_start )
      continue;
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
	  /* MIPS extension.  */
	case Dwarf32::dwarf_source_language::DW_LANG_Mips_Assembler:	return "assembler";
	  /* UPC extension.  */
	case Dwarf32::dwarf_source_language::DW_LANG_Upc:		return "Unified Parallel C";
  }
  return nullptr;
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
    if ( cu_lang )
    {
      auto lang = get_cu_name(cu_lang);
      if ( lang )
        fprintf(g_outf, "// Language: %s\n", lang);
      else
        fprintf(g_outf, "// Language: 0x%X\n", cu_lang);
    }
    if ( cu_producer )
      fprintf(g_outf, "// Producer: %s\n", cu_producer);
  }
  RenderUnit(last);
  merge_dumped();
  elements_.clear();
  m_replaced.clear();
  cu_name = cu_comp_dir = cu_producer = NULL;
  cu_lang = 0;
  ns_count = 0;
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
  auto &last = elements_.back();
  if ( last.type_ == ns_start )
  {
    ns_count++;
    if ( g_opt_v )
      fprintf(g_outf, "// ns_start %d at %lX\n", ns_count, last.id_);
  }
  m_stack.push( &last );
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
//  printf( "top %d %lX\n", last->type_, last->id_);
  if ( last->type_ == ns_start )
  {
    ns_count--;
    elements_.push_back(Element(ns_end, last->type_id_, last->level_));
    elements_.back().name_ = last->name_;
    if ( g_opt_v )
      fprintf(g_outf, "// ns_end %s %d off %lX\n", last->name_, ns_count, off);
  }
  m_stack.pop();
}

// formal parameter - level should be +1 to parent
bool TreeBuilder::AddFormalParam(uint64_t tag_id, int level, bool ell) {
  current_element_type_ = ElementType::formal_param;
  if (!elements_.size()) {
    fprintf(stderr, "Can't add a formal parameter if the element list is empty\n");
    return false;
  }
  if ( m_stack.empty() ) {
    fprintf(stderr, "Can't add a formal parameter when stack is empty\n");
    return false;
  }
  auto top = m_stack.top();
//  fprintf(g_outf, "f level %d level %d\n", m_stack.top()->level_, level);
  if ( top->level_ != level - 1 )
    return false;
  if ( !top->m_comp )
    top->m_comp = new Compound();
  
  top->m_comp->params_.push_back({NULL, 0, ell});
  return true;
}

void TreeBuilder::SetParentAccess(int a)
{
  if (current_element_type_ == ElementType::none) {
    return;
  }

  if ( m_stack.empty() ) {
    fprintf(stderr, "Can't add a access when stack is empty\n");
    return;
  }
  auto top = m_stack.top();
  if ( current_element_type_ == ElementType::member )
  {
    if ( !top->m_comp || top->m_comp->members_.empty()) {
      fprintf(stderr, "Can't set the member access if the members list is empty\n");
      return;
    }
  // fprintf(g_outf, "SetParentAccess %s a %d\n", m_stack.top()->members_.back().name_, a);
    top->m_comp->members_.back().access_ = a;
    return;
  } else {
    if ( !top->m_comp || top->m_comp->parents_.empty() ) {
      fprintf(stderr, "Can't add a parent access when parents list is empty\n");
      return;
    }
    top->m_comp->parents_.back().access = a;
  }
}

void TreeBuilder::AddElement(ElementType element_type, uint64_t tag_id, int level) {
  level -= ns_count;
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
      } else {
        auto top = m_stack.top();
        if ( !top->m_comp )
          top->m_comp = new Compound();
        top->m_comp->members_.push_back(Element(element_type, tag_id, level));
      }
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
      } else {
        auto top = m_stack.top();
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
      if (!elements_.size()) {
        fprintf(stderr, "Can't add a enumerator if the element list is empty\n");
        return;
      }
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't add a enumerator when stack is empty\n");
        return;
      } else {
        auto top = m_stack.top();
        if ( !top->m_comp )
          top->m_comp = new Compound();
        top->m_comp->enums_.push_back({NULL, 0});
      }
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
    auto top = m_stack.top();
    if (!top->m_comp || top->m_comp->params_.empty()) {
      fprintf(stderr, "Can't set the formal param name if the params list is empty\n");
      return;
    }

    top->m_comp->params_.back().name = name;
    return;    
  }
  if ( current_element_type_ == ElementType::enumerator )
  {
    if ( m_stack.empty() ) {
      fprintf(stderr, "Can't set the enum name when stack is empty\n");
      return;
    }
    auto top = m_stack.top();
    if (!top->m_comp || top->m_comp->enums_.empty()) {
      fprintf(stderr, "Can't set the enum name if the enums list is empty\n");
      return;
    }

    top->m_comp->enums_.back().name = name;
    return;
  }  

  if (current_element_type_ == ElementType::member) {
    if ( m_stack.empty() ) {
      fprintf(stderr, "Can't set the member name when stack is empty\n");
      return;
    }
    auto top = m_stack.top();
    if (!top->m_comp || top->m_comp->members_.empty()) {
      fprintf(stderr, "Can't set the member name if the members list is empty\n");
      return;
    }

    top->m_comp->members_.back().name_ = name;
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
    auto top = m_stack.top();
    if (!top->m_comp || top->m_comp->members_.empty()) {
      fprintf(stderr, "Can't set the member size if the members list is "
        "empty\n");
      return;
    }

    top->m_comp->members_.back().size_ = size;
    return;
  }

  elements_.back().size_ = size; 
}

void TreeBuilder::SetElementOffset(uint64_t offset) {
   if (current_element_type_ == ElementType::none) {
    return;
  }
  if (!elements_.size()) {
    fprintf(stderr, "Can't set an element offset if the element list is empty\n");
    return;
  }
  switch (current_element_type_) {
    case ElementType::member:
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't set the member offset when stack is empty\n");
        return;
      } else {
        auto top = m_stack.top();
        if (!top->m_comp || top->m_comp->members_.empty()) {
          fprintf(stderr, "Can't set the member offset if the members list is empty\n");
          break;
        }
        top->m_comp->members_.back().offset_ = offset;
      }
      break;
    case ElementType::inheritance:
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't set the parent offset when stack is empty\n");
        return;
      } else {
        auto top = m_stack.top();
        if (!top->m_comp || top->m_comp->parents_.empty()) {
          fprintf(stderr, "Can't set the parent offset if the parents list is empty\n");
          break;
        }
        top->m_comp->parents_.back().offset = offset;
      }
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
      } else {
        auto top = m_stack.top();
        if (!top->m_comp || top->m_comp->members_.empty()) {
          fprintf(stderr, "Can't set the member type if the members list is empty\n");
          break;
        }
        top->m_comp->members_.back().type_id_ = type_id;
      }
      break;
    case ElementType::inheritance:
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't set the parent type when stack is empty\n");
        return;
      } else {
        auto top = m_stack.top();
        if (!top->m_comp || top->m_comp->parents_.empty()) {
          fprintf(stderr, "Can't set the parent type if the parents list is empty\n");
          break;
        }
        top->m_comp->parents_.back().id = type_id;
      }
      break;
    case ElementType::formal_param:
      if ( m_stack.empty() ) {
        fprintf(stderr, "Can't set the formal param type when stack is empty\n");
        return;
      } else {
        auto top = m_stack.top();
        if (!top->m_comp || top->m_comp->params_.empty()) {
          fprintf(stderr, "Can't set the formal param type if the members list is empty\n");
          break;
        }
        top->m_comp->params_.back().id = type_id;
      }
      break;
    case ElementType::subrange_type:
      break; // do nothing
    default:
      elements_.back().type_id_ = type_id;
      break;
  }
}

void TreeBuilder::SetAlignment(uint64_t v)
{
  if ( current_element_type_ == ElementType::class_type ||
       current_element_type_ == ElementType::structure_type ||
       current_element_type_ == ElementType::union_type
  )
  {
    if (!elements_.size()) {
      fprintf(stderr, "Can't set alignment when element list is empty\n");
      return;
    }
    elements_.back().align_ = v;
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
  auto top = m_stack.top();
  if ( !top->m_comp || top->m_comp->enums_.empty()) {
    fprintf(stderr, "Can't set ConstValue if the enums list is empty\n");
    return;
  }
  top->m_comp->enums_.back().value = count;
}

void TreeBuilder::SetElementCount(uint64_t count) {
  if (current_element_type_ != ElementType::subrange_type) {
    return;
  }
  if ( elements_.empty()) {
    fprintf(stderr, "Can't set an element count if the element list is empty\n");
    return;
  }
  elements_.back().count_ = count;
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
    case ElementType::rvalue_ref_type: return "rvalue_reference";
    case ElementType::subroutine_type: return "function_type";
    case ElementType::subroutine: return "function";
    case ElementType::ns_start: return "namespace";
    default: return "unk";
  }
}
