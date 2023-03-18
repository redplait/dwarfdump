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
  ElfFile(std::string filepath, bool& success, TreeBuilder *);
  ~ElfFile();
  bool GetAllClasses();

private:
  static uint32_t ULEB128(const unsigned char* &data, size_t& bytes_available);
  static void PassData(Dwarf32::Form form,
      const unsigned char* &data, size_t& bytes_available);
  uint64_t DecodeLocation(Dwarf32::Form form,
      const unsigned char* info, size_t bytes_available);
  uint64_t FormDataValue(Dwarf32::Form form,
      const unsigned char* &info, size_t& bytes_available);
  const char* FormStringValue(Dwarf32::Form form,
      const unsigned char* &info, size_t& bytes_available);
  bool LoadAbbrevTags(uint32_t abbrev_offset);
  bool RegisterNewTag(Dwarf32::Tag tag, uint64_t tag_id, bool);
  bool LogDwarfInfo(Dwarf32::Attribute attribute, 
    uint64_t tag_id, Dwarf32::Form form, const unsigned char* &info, 
    size_t& info_bytes, const void* unit_base);

  elfio reader;
  TreeBuilder *tree_builder;

  const unsigned char* debug_info_;
  size_t debug_info_size_;
  const unsigned char* debug_abbrev_;
  size_t debug_abbrev_size_;

  struct TagSection {
      unsigned int number;
      Dwarf32::Tag type;
      bool has_children;
      const unsigned char* ptr;
  };
  typedef std::map<unsigned int, struct TagSection> CompilationUnit;
  CompilationUnit compilation_unit_;
  Dwarf32::Tag m_stype;
  int64_t cu_base;
  int64_t m_next; // value of DW_AT_sibling
  int m_level;
  bool m_regged;
};
