#include "ElfFile.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

int g_opt_d = 0,
    g_opt_f = 0,
    g_opt_k = 0,
    g_opt_l = 0,
    g_opt_v = 0;
FILE *g_outf = NULL;

void dump2file(std::string &name, const void *data, size_t size)
{
  // printf("dump2file %s\n", name.c_str());
  FILE *fp = fopen(name.c_str(), "w");
  if ( fp == NULL )
  {
    fprintf(stderr, "cannot open %s, errno %d", name.c_str(), errno);
    return;
  }
  fwrite(data, 1, size, fp);
  fclose(fp);
}

const size_t czSize = 4 + sizeof(uint64_t);

bool ElfFile::unzip_section(ELFIO::section *s, const unsigned char * &data, size_t &size)
{
  if ( s->get_size() < czSize )
  {
    fprintf(stderr, "section %s is too short, size %lX\n", s->get_name().c_str(), s->get_size());
    return false;
  }
  // check signature - see https://blogs.oracle.com/solaris/post/elf-section-compression
  const char *sdata = s->get_data();
  if ( sdata[0] != 0x5a ||
       sdata[1] != 0x4c ||
       sdata[2] != 0x49 ||
       sdata[3] != 0x42
     )
  {
    fprintf(stderr, "section %s has unknown signature %2.2X %2.2X %2.2X %2.2X\n", s->get_name().c_str(), 
      sdata[0], sdata[1], sdata[2], sdata[3]);
    return false;
  }
  size = __builtin_bswap64(*(uint64_t *)(sdata + 4));
  unsigned char *buf = (unsigned char *)malloc(size);
  if ( !buf )
  {
    fprintf(stderr, "cannot alooc unompressed size %lX, section size %lX\n", size, s->get_size() - czSize);
    return false;
  }
  if ( g_opt_d )
  {
    std::string tmp = "section.";
    tmp += s->get_name();
    tmp += ".comp";
    dump2file(tmp, sdata, s->get_size());
  }
  memset(buf, 0, size);
  int err = uncompress(buf, &size, (Bytef *)(sdata + czSize), s->get_size() - czSize);
  if ( err != Z_OK )
  {
    fprintf(stderr, "uncompress failed, err %d\n", err);
    free(buf);
    return false;
  }
  data = buf;
  if ( g_opt_d )
  {
    std::string tmp = "section.";
    tmp += s->get_name();
    tmp += ".ucomp";
    dump2file(tmp, data, size);
  }
  return true;
}

ElfFile::ElfFile(std::string filepath, bool& success, TreeBuilder *tb) :
  tree_builder(tb),
  debug_info_(nullptr), debug_info_size_(0),
  debug_abbrev_(nullptr), debug_abbrev_size_(0),
  free_info(false), free_abbrev(false)
{
  // read elf file
  if ( !reader.load(filepath.c_str()) )
  {
    fprintf(stderr, "ERR: Failed to open '%s'\n", filepath.c_str());
    success = false;
    return;
  }
  success = true;

  // Search the .debug_info and .debug_abbrev
  section *zinfo = nullptr;
  section *zabbrev = nullptr;
  tree_builder->debug_str_ = nullptr;
  tree_builder->debug_str_size_ = 0;
  Elf_Half n = reader.sections.size();
  for ( Elf_Half i = 0; i < n; i++) {
    section *s = reader.sections[i];
    const char* name = s->get_name().c_str();
    if (!strcmp(name, ".debug_info")) {
      debug_info_ = reinterpret_cast<const unsigned char*>(s->get_data());
      debug_info_size_ = s->get_size();
    } else if (!strcmp(name, ".debug_abbrev")) {
      debug_abbrev_ = reinterpret_cast<const unsigned char*>(s->get_data());
      debug_abbrev_size_ = s->get_size();
    } else if (!strcmp(name, ".debug_str")) {
      tree_builder->debug_str_ = reinterpret_cast<const char*>(s->get_data());
      tree_builder->debug_str_size_ = s->get_size();
    } // check compressed versions
    else if ( !strcmp(name, ".zdebug_info") )
      zinfo = s;
    else if ( !strcmp(name, ".zdebug_abbrev") )
      zabbrev = s;
  }
  // check if we need to decompress some sections
  if ( zinfo )
  {
    if ( !unzip_section(zinfo, debug_info_, debug_info_size_) )
    {
      fprintf(stderr, "cannot unpack section %s\n", zinfo->get_name().c_str());
      success = false;
      return;
    }
    free_info = true;
  }
  if ( zabbrev )
  {
    if ( !unzip_section(zabbrev, debug_abbrev_, debug_abbrev_size_) )
    {
      fprintf(stderr, "cannot unpack section %s\n", zabbrev->get_name().c_str());
      success = false;
      return;
    }
    free_abbrev = true;
  }
  success = (debug_info_ && debug_abbrev_);
}

ElfFile::~ElfFile() 
{
  if ( free_info && debug_info_ != nullptr )
    free((void *)debug_info_);
  if ( free_abbrev && debug_abbrev_ != nullptr )
    free((void *)debug_abbrev_);
}

// static
uint32_t ElfFile::ULEB128(const unsigned char* &data, size_t& bytes_available) {
  uint32_t result = 0;

  unsigned int shift = 0;
  while (bytes_available > 0) {
    unsigned char byte = *data;
    data++;
    bytes_available--;

    if (byte < 0x80) {
      result |= byte << shift;
      return result;
    } else {
      byte &= 0x7f;
      result |= byte << shift;
    }

    shift+=7;
  }

  return result;
}

void ElfFile::PassData(Dwarf32::Form form, const unsigned char* &data, 
                                                      size_t& bytes_available) {
  uint32_t length = 0;

  switch(form) {
    // Address
    case Dwarf32::Form::DW_FORM_addr:
      data += address_size_;
      bytes_available -= address_size_;
    break;

    // Block
    case Dwarf32::Form::DW_FORM_block:
      length = ElfFile::ULEB128(data, bytes_available);
      data += length;
      bytes_available -= length;
      break;
    case Dwarf32::Form::DW_FORM_block1:
      length = *reinterpret_cast<const uint8_t*>(data);
      data += sizeof(uint8_t) + length;
      bytes_available -= sizeof(uint8_t) + length;
      break;
    case Dwarf32::Form::DW_FORM_block2:
      length = *reinterpret_cast<const uint16_t*>(data);
      data += sizeof(uint16_t) + length;
      bytes_available -= sizeof(uint16_t) + length;
      break;
    case Dwarf32::Form::DW_FORM_block4:
      length = *reinterpret_cast<const uint32_t*>(data);
      data += sizeof(uint32_t) + length;
      bytes_available -= sizeof(uint32_t) + length;
      break;

    // Constant
    case Dwarf32::Form::DW_FORM_data1:
    case Dwarf32::Form::DW_FORM_strx1:
      data++;
      bytes_available--;
      break;
    case Dwarf32::Form::DW_FORM_data2:
    case Dwarf32::Form::DW_FORM_strx2:
      data += 2;
      bytes_available -= 2;
      break;
    case Dwarf32::Form::DW_FORM_strx3:
      data += 3;
      bytes_available -= 3;
      break;
    case Dwarf32::Form::DW_FORM_strx4:  
    case Dwarf32::Form::DW_FORM_data4:
      data += 4;
      bytes_available -= 4;
      break;
    case Dwarf32::Form::DW_FORM_data8:
      data += 8;
      bytes_available -= 8;
      break;
    case Dwarf32::Form::DW_FORM_data16:
      data += 16;
      bytes_available -= 16;
      break;
    case Dwarf32::Form::DW_FORM_sdata:
    case Dwarf32::Form::DW_FORM_addrx:
    case Dwarf32::Form::DW_FORM_strx:
      ElfFile::ULEB128(data, bytes_available);
      break;
    case Dwarf32::Form::DW_FORM_udata:
      ElfFile::ULEB128(data, bytes_available);
      break;

    // Expression or location
    case Dwarf32::Form::DW_FORM_exprloc:
      length = ElfFile::ULEB128(data, bytes_available);
      data += length;
      bytes_available -= length;
      break;

    // Line offset
    case Dwarf32::Form::DW_FORM_sec_offset:
      data += 4;
      bytes_available -= 4;
      break;

    // Flag
    case Dwarf32::Form::DW_FORM_flag:
      data++;
      bytes_available--;
      break;
    case Dwarf32::Form::DW_FORM_flag_present:
    case Dwarf32::Form::DW_FORM_implicit_const:
      break;

    // Reference
    case Dwarf32::Form::DW_FORM_ref1:
      data++;
      bytes_available--;
      break;
    case Dwarf32::Form::DW_FORM_ref2:
      data += 2;
      bytes_available -= 2;
      break;
    case Dwarf32::Form::DW_FORM_ref4:
      data += 4;
      bytes_available -= 4;
      break;
    case Dwarf32::Form::DW_FORM_ref8:
      data += 8;
      bytes_available -= 8;
      break;
    case Dwarf32::Form::DW_FORM_ref_udata:
      ElfFile::ULEB128(data, bytes_available);
      break;
    case Dwarf32::Form::DW_FORM_ref_addr:
      data += 4;
      bytes_available -= 4;
      break;
    case Dwarf32::Form::DW_FORM_ref_sig8:
      data += 8;
      bytes_available -= 8;
      break;

    // String
    case Dwarf32::Form::DW_FORM_strp:
      data += 4;
      bytes_available -= 4;
      break;
    case Dwarf32::Form::DW_FORM_string:
      while (*data) {
          data++;
          bytes_available--;
      }
      data++;
      bytes_available--;
      break;

    default:
      fprintf(stderr, "ERR: Unpexpected form type 0x%x\n", form);
      break;
  }
}

// seems that vtable_elem_location usually encoded not as simple DW_FORM_xxx (so we can`t use FormDataValue here)
// but as DW_OP_constu inside block
// so I ripped necessary part of code from binutils/dwarf.c function decode_location_expression in this method
uint64_t ElfFile::DecodeLocation(Dwarf32::Form form, const unsigned char* info, size_t bytes_available) 
{
  if ( form != Dwarf32::Form::DW_FORM_exprloc )
    return FormDataValue(form, info, bytes_available);
  uint64_t len = ElfFile::ULEB128(info, bytes_available);
  const unsigned char *end = info + len;
  while( info < end && bytes_available )
  {
    unsigned op = *info;
    info++;
    bytes_available--;
    uint64_t value = 0;
    switch(op)
    {
      case Dwarf32::dwarf_ops::DW_OP_constu:
      case Dwarf32::dwarf_ops::DW_OP_consts: // signed variant
        return ElfFile::ULEB128(info, bytes_available);
      // 1 byte
      case Dwarf32::dwarf_ops::DW_OP_const1u:
      case Dwarf32::dwarf_ops::DW_OP_const1s:
      {
        if ( bytes_available >= 1 )
        {
          value = *reinterpret_cast<const uint8_t*>(info);
          return value; 
        }
        fprintf(stderr, "DecodeLocation: wrong len %lX for op %X\n", bytes_available, op);
        return 0;
      }
      // 2 bytes
      case Dwarf32::dwarf_ops::DW_OP_const2u:
      case Dwarf32::dwarf_ops::DW_OP_const2s:
      {
        if ( bytes_available >= 2 )
        {
          value = *reinterpret_cast<const uint16_t*>(info);
          return value; 
        }
        fprintf(stderr, "DecodeLocation: wrong len2 %lX for op %X\n", bytes_available, op);
        return 0;
      }
      // 4 bytes
      case Dwarf32::dwarf_ops::DW_OP_const4u:
      case Dwarf32::dwarf_ops::DW_OP_const4s:
      {
        if ( bytes_available >= 4 )
        {
          value = *reinterpret_cast<const uint32_t*>(info);
          return value; 
        }
        fprintf(stderr, "DecodeLocation: wrong len4 %lX for op %X\n", bytes_available, op);
        return 0;
      }
      // 8 bytes
      case Dwarf32::dwarf_ops::DW_OP_const8u:
      case Dwarf32::dwarf_ops::DW_OP_const8s:
      {
        if ( bytes_available >= 8 )
        {
          value = *reinterpret_cast<const uint64_t*>(info);
          return value; 
        }
        fprintf(stderr, "DecodeLocation: wrong len8 %lX for op %X\n", bytes_available, op);
        return 0;
      }

      default:
        fprintf(stderr, "DecodeLocation: unknown op %X\n", op);
        return 0;
    }
  }
  return 0;
}

uint64_t ElfFile::FormDataValue(Dwarf32::Form form, const unsigned char* &info, 
                                                      size_t& bytes_available) {
  uint64_t value = 0;

  switch(form) {
    case Dwarf32::Form::DW_FORM_flag_present:
      fprintf(stderr, "ERR: DW_FORM_flag_present at %lX\n", info - debug_info_);
      value = 1;
     break;
    case Dwarf32::Form::DW_FORM_flag:
    case Dwarf32::Form::DW_FORM_data1:
    case Dwarf32::Form::DW_FORM_ref1:
      value = *reinterpret_cast<const uint8_t*>(info);
      info++;
      bytes_available--;
      break;
    case Dwarf32::Form::DW_FORM_data2:
    case Dwarf32::Form::DW_FORM_ref2:
      value = *reinterpret_cast<const uint16_t*>(info);
      info += 2;
      bytes_available -= 2;
      break;
    case Dwarf32::Form::DW_FORM_data4:
    case Dwarf32::Form::DW_FORM_ref4:
    case Dwarf32::Form::DW_FORM_ref_addr:
    case Dwarf32::Form::DW_FORM_sec_offset:
      value = *reinterpret_cast<const uint32_t*>(info);
      info += 4;
      bytes_available -= 4;
      break;
    case Dwarf32::Form::DW_FORM_data8:
    case Dwarf32::Form::DW_FORM_ref8:
    case Dwarf32::Form::DW_FORM_ref_sig8:
      value = *reinterpret_cast<const uint64_t*>(info);
      info += 8;
      bytes_available -= 8;
      break;
    case Dwarf32::Form::DW_FORM_addr:
      if ( address_size_ == 8 )
        value = *reinterpret_cast<const uint64_t*>(info);
      else
        value = *reinterpret_cast<const uint32_t*>(info);
      info += address_size_;
      bytes_available -= address_size_;
      break;
    case Dwarf32::Form::DW_FORM_sdata:
    case Dwarf32::Form::DW_FORM_udata:
    case Dwarf32::Form::DW_FORM_ref_udata:
    case Dwarf32::Form::DW_FORM_indirect:
      value = ElfFile::ULEB128(info, bytes_available);
      break;
    case Dwarf32::Form::DW_FORM_exprloc:
      value = ElfFile::ULEB128(info, bytes_available);
      info += value;
      bytes_available -= value;
      break;
    default:
      fprintf(stderr, "ERR: Unexpected form data 0x%x at %lX\n", form, info - debug_info_);
      exit(1);
  }

  return value;
};

const char* ElfFile::FormStringValue(Dwarf32::Form form, const unsigned char* &info, 
                                                      size_t& bytes_available) {
  const char* str = nullptr;
  uint32_t str_pos = 0;

  switch(form) {
    case Dwarf32::Form::DW_FORM_strp:
      str_pos = *reinterpret_cast<const uint32_t*>(info);
      info += sizeof(str_pos);
      bytes_available -= sizeof(str_pos);
      str = &tree_builder->debug_str_[str_pos];
      break;
    case Dwarf32::Form::DW_FORM_string:
      str = reinterpret_cast<const char*>(info);
      while (*info) {
          info++;
          bytes_available--;
      }
      info++;
      bytes_available--;
      break;
    default:
      fprintf(stderr, "ERR: Unexpected form string 0x%x\n", form);
      break;
  }

  return str;
};

// load tags from .debug_abbrev section
bool ElfFile::LoadAbbrevTags(uint32_t abbrev_offset) {
  if (!debug_info_ || !debug_abbrev_) {
      return false;
  }
  compilation_unit_.clear();

  const unsigned char* abbrev = reinterpret_cast<const unsigned char*>(debug_abbrev_ 
                                                          + abbrev_offset);
  size_t abbrev_bytes = debug_abbrev_size_ - abbrev_offset;

  // For all compilation tags
  while (abbrev_bytes > 0 && abbrev[0]) {
    struct TagSection section;
    section.number = ElfFile::ULEB128(abbrev, abbrev_bytes);
    DBG_PRINTF(".abbrev+%lx\t Tag Number %d\n",
        abbrev - debug_abbrev_, section.number);
    section.type =
        static_cast<Dwarf32::Tag>(ElfFile::ULEB128(abbrev, abbrev_bytes));
    section.has_children = *abbrev;
    abbrev++;
    abbrev_bytes--;
    section.ptr = abbrev;

    if (compilation_unit_.find(section.number) != compilation_unit_.end()) {
        fprintf(stderr, "ERR: Section number %d already exists\n", section.number);
        compilation_unit_.clear();
        return false;
    }
    compilation_unit_[section.number] = section;

    while (abbrev_bytes > 0 && abbrev[0]) { // For all attributes
        abbrev++;
    }
    abbrev += 2;
    abbrev_bytes -= 2;
  }

  return true;
}

#define CASE_REGISTER_NEW_TAG(tag_type, element_type)                         \
  case Dwarf32::Tag::tag_type:                                                \
    tree_builder->AddElement(TreeBuilder::ElementType::element_type, tag_id, m_level); \
    return true; \
    break;

bool ElfFile::RegisterNewTag(Dwarf32::Tag tag, uint64_t tag_id) {
   bool ell = false;
  switch (tag) {
    CASE_REGISTER_NEW_TAG(DW_TAG_array_type, array_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_class_type, class_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_enumeration_type, enumerator_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_enumerator, enumerator)
    CASE_REGISTER_NEW_TAG(DW_TAG_member, member)
    CASE_REGISTER_NEW_TAG(DW_TAG_pointer_type, pointer_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_structure_type, structure_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_typedef, typedef2)
    CASE_REGISTER_NEW_TAG(DW_TAG_union_type, union_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_inheritance, inheritance)
    CASE_REGISTER_NEW_TAG(DW_TAG_subrange_type, subrange_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_base_type, base_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_const_type, const_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_volatile_type, volatile_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_restrict_type, restrict_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_reference_type, reference_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_rvalue_reference_type, rvalue_ref_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_subroutine_type, subroutine_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_ptr_to_member_type, ptr2member)
    CASE_REGISTER_NEW_TAG(DW_TAG_unspecified_type, unspec_type)
    case Dwarf32::Tag::DW_TAG_namespace:
      if ( m_section->has_children )
      {
        tree_builder->AddElement(TreeBuilder::ElementType::ns_start, tag_id, m_level);
        return true;
      }
      tree_builder->AddNone();
      break;      
    case Dwarf32::Tag::DW_TAG_subprogram:
      if ( g_opt_f )
      {
        tree_builder->AddElement(TreeBuilder::ElementType::subroutine, tag_id, m_level);
        return true;
      }
      tree_builder->AddNone();
      break;
    case Dwarf32::Tag::DW_TAG_unspecified_parameters:
      ell = true;
    case Dwarf32::Tag::DW_TAG_formal_parameter:
      if ( g_opt_d )
        fprintf(g_outf, "param %lX regged %d\n", tag_id, m_regged);
      if ( m_regged )
      {
        return tree_builder->AddFormalParam(tag_id, m_level, ell);
      }
      tree_builder->AddNone();
      break;
    default:
      tree_builder->AddNone();
  }
  return false;
}

bool ElfFile::LogDwarfInfo(Dwarf32::Attribute attribute, 
                uint64_t tag_id, Dwarf32::Form form, const unsigned char* &info, 
                size_t& info_bytes, const void* unit_base) {           
  switch(attribute) {
    case Dwarf32::Attribute::DW_AT_sibling:
      m_next = FormDataValue(form, info, info_bytes);
     return true;
    case Dwarf32::Attribute::DW_AT_object_pointer: {
      uint64_t v = FormDataValue(form, info, info_bytes);
      if ( m_regged )
      {
          if (form != Dwarf32::Form::DW_FORM_ref_addr) {
            // The offset is relative to the current compilation unit, we make it
            // absolute
            v += reinterpret_cast<const unsigned char*>(unit_base) - debug_info_;
          }
        tree_builder->SetObjPtr((int)v);
      }
      return true;
    }   
    case Dwarf32::Attribute::DW_AT_virtuality: {
      uint64_t v = FormDataValue(form, info, info_bytes);
      if ( m_regged )
        tree_builder->SetVirtuality((int)v);
      return true;
    }
    case Dwarf32::Attribute::DW_AT_accessibility:
      if ( m_section->type == Dwarf32::Tag::DW_TAG_inheritance || m_section->type == Dwarf32::Tag::DW_TAG_member )
      {
        int a = (int)FormDataValue(form, info, info_bytes);
        tree_builder->SetParentAccess(a);
        return true;
      }
    // Name
    case Dwarf32::Attribute::DW_AT_producer:
      if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit )
      {
        tree_builder->cu_producer = FormStringValue(form, info, info_bytes);
        return true;  
      }
      break;
    case Dwarf32::Attribute::DW_AT_comp_dir:
      if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit )
      {
        tree_builder->cu_comp_dir = FormStringValue(form, info, info_bytes);
        return true;  
      }
      break;
    case Dwarf32::Attribute::DW_AT_language:
      if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit )
      {
        tree_builder->cu_lang = (int)FormDataValue(form, info, info_bytes);
        return true;  
      }
      break;  
    case Dwarf32::Attribute::DW_AT_name:
      if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit )
      {
        tree_builder->cu_name = FormStringValue(form, info, info_bytes);
        return true;  
      }
    case Dwarf32::Attribute::DW_AT_linkage_name: {
      const char* name = FormStringValue(form, info, info_bytes);
      if ( tree_builder->check_dumped_type(name) )
      {
        m_regged = false;
        return true;
      }
      if ( Dwarf32::Attribute::DW_AT_linkage_name == attribute )
        tree_builder->SetLinkageName(name);
      else
        tree_builder->SetElementName(name);
      return true;
    }
    case Dwarf32::Attribute::DW_AT_explicit: {
      if ( m_regged )
        tree_builder->SetExplicit();
      return true;
    }
    case Dwarf32::Attribute::DW_AT_variable_parameter: {
      uint64_t v = FormDataValue(form, info, info_bytes);
      if ( m_regged && v )
        tree_builder->SetVarParam(v ? true : false);
      return true;
      }
      break;
    case Dwarf32::Attribute::DW_AT_defaulted: {
      uint64_t v = FormDataValue(form, info, info_bytes);
      if ( m_regged && v )
        tree_builder->SetDefaulted();
      return true;
    }
    case Dwarf32::Attribute::DW_AT_inline: {
      uint64_t v = FormDataValue(form, info, info_bytes);
      if ( m_regged && v )
        tree_builder->SetInlined((int)v);
      return true;
    }
    case Dwarf32::Attribute::DW_AT_abstract_origin: {
      uint64_t addr = FormDataValue(form, info, info_bytes);
      if ( m_regged )
      {
          if (form != Dwarf32::Form::DW_FORM_ref_addr) {
            // The offset is relative to the current compilation unit, we make it
            // absolute
            addr += reinterpret_cast<const unsigned char*>(unit_base) - debug_info_;
          }
        tree_builder->SetAbs(addr);
      }
      return true;
    }
    case Dwarf32::Attribute::DW_AT_specification: {
      uint64_t addr = FormDataValue(form, info, info_bytes);
      if ( m_regged )
      {
        if (form != Dwarf32::Form::DW_FORM_ref_addr) {
          // The offset is relative to the current compilation unit, we make it
          // absolute
          addr += reinterpret_cast<const unsigned char*>(unit_base) - debug_info_;
        }
        tree_builder->SetSpec(addr);
      }
      return true;
    }
    // address
    case Dwarf32::Attribute::DW_AT_low_pc: {
      uint64_t addr = FormDataValue(form, info, info_bytes);
      if ( m_regged )
        tree_builder->SetAddr(addr);
      return true;
    }
    // Size
    case Dwarf32::Attribute::DW_AT_bit_size: {
      uint64_t byte_size = FormDataValue(form, info, info_bytes);
      if ( m_regged )
        tree_builder->SetBitSize(byte_size);
      return true;
    }
    case Dwarf32::Attribute::DW_AT_bit_offset: {
      uint64_t byte_size = FormDataValue(form, info, info_bytes);
      if ( m_regged )
        tree_builder->SetBitOffset(byte_size);
      return true;
    }

    case Dwarf32::Attribute::DW_AT_byte_size: {
      uint64_t byte_size = FormDataValue(form, info, info_bytes);
      if ( m_regged )
        tree_builder->SetElementSize(byte_size);
      return true;
    }

    case Dwarf32::Attribute::DW_AT_containing_type: {
      uint64_t ctype = FormDataValue(form, info, info_bytes);
      if ( m_regged )
      {
        if (form != Dwarf32::Form::DW_FORM_ref_addr) {
          // The offset is relative to the current compilation unit, we make it
          // absolute
          ctype += reinterpret_cast<const unsigned char*>(unit_base) - debug_info_;
        }
        tree_builder->SetContainingType(ctype);
      }
      return true;
    }

    // Offset
    case Dwarf32::Attribute::DW_AT_data_member_location: {
      uint64_t offset = FormDataValue(form, info, info_bytes);
      tree_builder->SetElementOffset(offset);
      return true;
    }

    // Type
    case Dwarf32::Attribute::DW_AT_type: {
      uint64_t id = FormDataValue(form, info, info_bytes);
      if (form != Dwarf32::Form::DW_FORM_ref_addr) {
        // The offset is relative to the current compilation unit, we make it
        // absolute
        id += reinterpret_cast<const unsigned char*>(unit_base) - debug_info_;
      }
      if ( m_regged )
        tree_builder->SetElementType(id);
      return true;
    }

    // Count
    case Dwarf32::Attribute::DW_AT_count: {
      uint64_t count = FormDataValue(form, info, info_bytes);
      if ( m_regged )
        tree_builder->SetElementCount(count);
      return true;
    }
    // upper bound
    case Dwarf32::Attribute::DW_AT_upper_bound: {
      uint64_t count = FormDataValue(form, info, info_bytes);
      if ( m_regged )
        tree_builder->SetElementCount(count);
      return true;
    }
    // const value - for enums
    case Dwarf32::Attribute::DW_AT_const_value: {
      if ( !m_regged || !tree_builder->is_enum() )
        return false;
      uint64_t count = FormDataValue(form, info, info_bytes);
      tree_builder->SetConstValue(count);
      return true;
    }
    break;
    // vtable elem location
    case Dwarf32::Attribute::DW_AT_vtable_elem_location:
      if ( m_regged )
      {
        uint64_t idx = DecodeLocation(form, info, info_bytes);
        if ( idx )
          tree_builder->SetVtblIndex(idx);
      }
      return false;
     break;
    // aligment
    case Dwarf32::Attribute::DW_AT_alignment:
     if ( !m_regged )
       return false;
     else {
      uint64_t count = FormDataValue(form, info, info_bytes);
      tree_builder->SetAlignment(count);
      return true;
     }
     break;
     // noreturn attribute
    case Dwarf32::Attribute::DW_AT_noreturn:
     if ( !m_regged )
       return false;
     else {
      tree_builder->SetNoReturn();
      return true;
     }
     break;
     // declaration
     case Dwarf32::Attribute::DW_AT_declaration:
     if ( !m_regged )
       return false;
     else {
      tree_builder->SetDeclaration();
      return true;
     }
     break;
     case Dwarf32::Attribute::DW_AT_artificial:
       if ( m_regged )
         tree_builder->SetArtiticial();
       return true;
    default:
      return false;
  }

  return false;
}

bool ElfFile::GetAllClasses() {
  const unsigned char* info = reinterpret_cast<const unsigned char*>(debug_info_);
  size_t info_bytes = debug_info_size_;

  while (info_bytes > 0) {
    // process previous compilation unit
    tree_builder->ProcessUnit();
    // Load the compilation unit information
    const unsigned char* cu_start = info;
    cu_base = cu_start - debug_info_;
    const Dwarf32::CompilationUnitHdr* unit_hdr =
        reinterpret_cast<const Dwarf32::CompilationUnitHdr*>(info);
    address_size_ = unit_hdr->address_size;
    DBG_PRINTF("unit_length         = 0x%x\n", unit_hdr->unit_length);
    DBG_PRINTF("version             = %d\n", unit_hdr->version);
    DBG_PRINTF("debug_abbrev_offset = 0x%x\n", unit_hdr->debug_abbrev_offset);
    DBG_PRINTF("address_size        = %d\n", unit_hdr->address_size);
    const unsigned char* info_end = info + unit_hdr->unit_length + sizeof(uint32_t);
    info += sizeof(Dwarf32::CompilationUnitHdr);
    info_bytes -= sizeof(Dwarf32::CompilationUnitHdr);

    if (!LoadAbbrevTags(unit_hdr->debug_abbrev_offset)) {
      fprintf(stderr, "ERR: Can't load the compilation\n");
      return false;
    }
    if ( g_opt_d )
      fprintf(g_outf, "reset level\n");
    m_level = 0;

    // For all compilation tags
    while (info < info_end) {
      uint64_t tag_id = info - debug_info_; 
      uint32_t info_number = ElfFile::ULEB128(info, info_bytes);
      DBG_PRINTF(".info+%lx\t Info Number %X\n", info-debug_info_, info_number);
      if (!info_number) { // reserved
        if ( m_level )
        {
          m_level--;
          tree_builder->pop_stack(info-debug_info_);
        }
        continue;
      }

      std::map<unsigned int, struct TagSection>::iterator it_section =
          compilation_unit_.find(info_number);
      if (it_section == compilation_unit_.end()) {
        fprintf(stderr, "ERR: Can't find tag number %X\n", info_number);
        return false;
      }
      m_section = &it_section->second;
      const unsigned char* abbrev = m_section->ptr;
      size_t abbrev_bytes = debug_abbrev_size_ - (abbrev - debug_abbrev_);
      m_regged = RegisterNewTag(m_section->type, tag_id);
      m_next = 0;

      if ( g_opt_d )
        fprintf(g_outf, "%d GetAllClasses %lx size %lx regged %d\n", m_level, tag_id, abbrev_bytes,m_regged);

      // For all attributes
      while (*abbrev) 
      {
        Dwarf32::Attribute abbrev_attribute = static_cast<Dwarf32::Attribute>(
            ElfFile::ULEB128(abbrev, abbrev_bytes));
        Dwarf32::Form abbrev_form = 
            static_cast<Dwarf32::Form>(ElfFile::ULEB128(abbrev, abbrev_bytes));

        if ( g_opt_d )
          fprintf(g_outf,".info+%lx\t %02x %02x\n", info-debug_info_, 
                                                abbrev_attribute, abbrev_form);
        bool logged = LogDwarfInfo(abbrev_attribute, 
          tag_id, abbrev_form, info, info_bytes, unit_hdr);
        if (!logged) {
          DBG_PRINTF("abbrev_form %X\n", abbrev_form);
          ElfFile::PassData(abbrev_form, info, info_bytes);
        }
      }
      if ( !m_regged /* && m_level */ && m_next )
      {
        const unsigned char* info2 = cu_start + m_next;
        if ( g_opt_d)
          fprintf(g_outf, "%lX m_next %lX - %lX\n", info - debug_info_, m_next, info2 - debug_info_);
        if ( info2 > info )
        {
          info_bytes -= info2 - info;
          info = info2;
          if ( !info_bytes )
            break;
          else
            goto skip_level;
        }
      }
      if ( m_section->has_children )
      {
        m_level++;
        tree_builder->add2stack();
      }
skip_level:
       ;
    }
  }
  // process last compilation unit
  tree_builder->ProcessUnit(1);

  return true;
}


