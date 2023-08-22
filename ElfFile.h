#pragma once
#include <string>
#include <map>
#include <vector>
#include <elfio/elfio.hpp>
#include "dwarf32.h"
#include "TreeBuilder.h"

using namespace ELFIO;

struct saved_section
{
  std::string name;
  uint64_t offset;
  uint64_t len;
};

typedef struct
{
  uint64_t	 li_length;
  unsigned short li_version;
  unsigned char  li_address_size;
  unsigned char  li_segment_size;
  uint64_t	 li_prologue_length;
  unsigned char  li_min_insn_length;
  unsigned char  li_max_ops_per_insn;
  unsigned char  li_default_is_stmt;
  int            li_line_base;
  unsigned char  li_line_range;
  unsigned char  li_opcode_base;
  unsigned int   li_offset_size;
} DWARF2_Internal_LineInfo;

class ElfFile : public ISectionNames
{
public:
  ElfFile(std::string filepath, bool& success, TreeBuilder *);
  ~ElfFile();
  bool GetAllClasses();
  bool SaveSections(std::string &fname);
  virtual const char *find_sname(uint64_t);
private:
  bool unzip_section(ELFIO::section *, const unsigned char * &data, size_t &);
  bool check_compressed_section(ELFIO::section *, const unsigned char * &data, size_t &);
  template <typename T>
  bool uncompressed_section(ELFIO::section *, const unsigned char * &data, size_t &);
  static uint32_t ULEB128(const unsigned char* &data, size_t& bytes_available);
  static int64_t SLEB128(const unsigned char* &data, size_t& bytes_available);
  void PassData(Dwarf32::Form form, const unsigned char* &data, size_t& bytes_available);
  uint64_t DecodeAddrLocation(Dwarf32::Form form, const unsigned char* info, size_t bytes_available, param_loc *);
  uint64_t DecodeLocation(Dwarf32::Form form, const unsigned char* info, size_t bytes_available);
  uint64_t FormDataValue(Dwarf32::Form form,
      const unsigned char* &info, size_t& bytes_available);
  const char* FormStringValue(Dwarf32::Form form,
      const unsigned char* &info, size_t& bytes_available);
  bool LoadAbbrevTags(uint32_t abbrev_offset);
  bool RegisterNewTag(Dwarf32::Tag tag);
  template <typename T>
  bool ProcessFlags(Dwarf32::Form form, const unsigned char* &info, size_t& info_bytes, T ptr);
  bool LogDwarfInfo(Dwarf32::Attribute attribute, 
    Dwarf32::Form form, const unsigned char* &info, 
    size_t& info_bytes, const void* unit_base);
  void free_section(const unsigned char *&s, bool);
  bool read_debug_lines();
  const char *get_indexed_str(uint32_t);
  uint64_t get_indexed_addr(uint64_t, int size);

  elfio reader;
  TreeBuilder *tree_builder;

  const unsigned char* debug_info_;
  size_t debug_info_size_;
  const unsigned char* debug_abbrev_;
  size_t debug_abbrev_size_;
  const unsigned char *debug_loc_;
  size_t debug_loc_size_;
  const unsigned char *debug_str_offsets_;
  size_t debug_str_offsets_size_;
  const unsigned char *debug_addr_;
  size_t debug_addr_size_;
  // for file names we need section .debug_line
  const unsigned char *debug_line_;
  size_t debug_line_size_;

  struct TagSection {
      unsigned int number;
      Dwarf32::Tag type;
      bool has_children;
      const unsigned char* ptr;
  };
  TagSection *m_section;
  int64_t m_implicit_const;
  uint64_t m_tag_id;
  typedef std::map<unsigned int, struct TagSection> CompilationUnit;
  CompilationUnit compilation_unit_;
  uint8_t address_size_;
  int64_t cu_base;
  int64_t m_next; // value of DW_AT_sibling
  int m_level;
  // DW_AT_str_offsets_base
  int64_t offsets_base; // dwarf5 from DW_AT_str_offsets_base
  int64_t addr_base;    // dwarf5 from DW_AT_addr_base
  // file and dir names from .debug_line
  DWARF2_Internal_LineInfo m_li;
  const unsigned char *m_curr_lines;  
  std::map<uint, const char *> m_dl_dirs;
  std::map<uint, std::pair<uint, const char *> > m_dl_files;
  inline void reset_lines()
  {
    memset(&m_li, 0, sizeof(m_li));
    m_dl_dirs.clear();
    m_dl_files.clear();
  }
  bool get_filename(unsigned int fid, std::string &);
  // sections from original elf file
  std::vector<saved_section> m_orig_sects;
  bool m_regged;
  // free sections flags
  bool free_info;
  bool free_abbrev;
  bool free_strings;
  bool free_str_offsets;
  bool free_addr;
  bool free_loc;
  bool free_line;
};
