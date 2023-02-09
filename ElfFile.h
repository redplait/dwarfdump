#pragma once
#include <string>
#include <map>
#include <vector>
#include <elfio/elfio.hpp>
#include "dwarf32.h"
#include "TreeBuilder.h"

using namespace ELFIO;

class ElfFile {
public:
  ElfFile(std::string filepath, bool& success);
  ~ElfFile();
  bool GetAllClasses();

  std::string json() { return tree_builder_.GenerateJson(); }

private:
  static uint32_t ULEB128(const unsigned char* &data, size_t& bytes_available);
  static void PassData(Dwarf32::Form form,
      const unsigned char* &data, size_t& bytes_available);
  uint64_t FormDataValue(Dwarf32::Form form,
      const unsigned char* &info, size_t& bytes_available);
  const char* FormStringValue(Dwarf32::Form form,
      const unsigned char* &info, size_t& bytes_available);
  bool LoadAbbrevTags(uint32_t abbrev_offset);
  bool RegisterNewTag(Dwarf32::Tag tag, uint64_t tag_id);
  bool LogDwarfInfo(Dwarf32::Attribute attribute, 
    uint64_t tag_id, Dwarf32::Form form, const unsigned char* &info, 
    size_t& info_bytes, const void* unit_base);

  elfio reader;

  const unsigned char* debug_info_;
  size_t debug_info_size_;
  const unsigned char* debug_abbrev_;
  size_t debug_abbrev_size_;
  const char* debug_str_;
  size_t debug_str_size_;

  struct TagSection {
      unsigned int number;
      Dwarf32::Tag type;
      bool has_children;
      const unsigned char* ptr;
  };
  typedef std::map<unsigned int, struct TagSection> CompilationUnit;
  CompilationUnit compilation_unit_;
  Dwarf32::Tag m_stype;
  int64_t m_next; // value of DW_AT_sibling
  int m_level;

  TreeBuilder tree_builder_;
};
