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
  // for delayed loading for version 5 - bcs we don`t know str_offsets
  const unsigned char *m_ptr;
} DWARF2_Internal_LineInfo;

class ElfFile : public ISectionNames, public IGetLoclistX
{
public:
  ElfFile(std::string filepath, bool& success, TreeBuilder *);
  ~ElfFile();
  bool GetAllClasses();
  bool SaveSections(std::string &fname);
  // ISectionNames
  virtual const char *find_sname(uint64_t);
  // IGetLoclistX
  virtual bool get_loclistx(uint64_t off, std::list<LocListXItem> &, uint64_t);
private:
  bool unzip_section(ELFIO::section *, const unsigned char * &data, size_t &);
  bool check_compressed_section(ELFIO::section *, const unsigned char * &data, size_t &);
  template <typename T>
  bool uncompressed_section(ELFIO::section *, const unsigned char * &data, size_t &);
  static uint64_t ULEB128(const unsigned char* &data, size_t& bytes_available);
  static int64_t SLEB128(const unsigned char* &data, size_t& bytes_available);
  void PassData(Dwarf32::Form form, const unsigned char* &data, size_t& bytes_available);
  uint64_t DecodeAddrLocation(Dwarf32::Form form, const unsigned char* info, size_t bytes_available, param_loc *, const unsigned char *);
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
  bool read_delayed_lines();
  unsigned const char *read_formatted_table(bool);
  const char *get_indexed_str(uint32_t);
  uint64_t get_indexed_addr(uint64_t, int size);
  uint64_t fetch_indexed_addr(uint64_t, int size);
  uint64_t fetch_indexed_value(uint64_t, const unsigned char *, uint64_t s_size, uint64_t base);

  elfio reader;
  endianess_convertor endc;
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
  const unsigned char *debug_loclists_;
  size_t debug_loclists_size_;
  // for file names we need section .debug_line
  const unsigned char *debug_line_;
  size_t debug_line_size_;
  // for file names in dwarf5 we also need .debug_line_str
  const unsigned char *debug_line_str_;
  size_t debug_line_str_size_;
  // section with addresses ranges - when opt_f
  const unsigned char *debug_rnglists_;
  size_t debug_rnglists_size_;
  const unsigned char *debug_ranges_;
  size_t debug_ranges_size_;
  // section with frame info - when opt_f
  const unsigned char *debug_frame_;
  size_t debug_frame_size_;

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
  // for cases like some string in compilation unit and then later DW_AT_str_offsets_base we need push delayed handlers
  typedef void (ElfFile::*t_dassign)(const char *);
  typedef const char *(ElfFile::*t_check)(uint32_t);
  t_dassign curr_asgn;
  struct one_delayed_str {
   t_dassign asgn;
   t_check check;
   uint32_t value;
  };
  std::list<one_delayed_str> m_dlist;
  void push2dlist(t_check c, uint32_t v)
  {
    m_dlist.push_back( { curr_asgn, c, v} );
    curr_asgn = nullptr;
  }
  void apply_dlist()
  {
    for ( auto &d: m_dlist )
    {
      auto val = (this->*(d.check))(d.value);
      (this->*(d.asgn))(val);
    }
    m_dlist.clear();
  }
  void asgn_producer(const char *v)
  {
    tree_builder->cu.cu_producer = v;
  }
  void asgn_comp_dir(const char *v)
  {
    tree_builder->cu.cu_comp_dir = v;
  }
  void asgn_cu_name(const char *v)
  {
    tree_builder->cu.cu_name = v;
  }
  void asgn_package(const char *v)
  {
    tree_builder->cu.cu_package = v;
  }
  const char *check_strx4(uint32_t);
  const char *check_strx2(uint32_t);
  const char *check_strx3(uint32_t);
  const char *check_strx1(uint32_t);
  const char *check_strp(uint32_t);
  uint32_t read_x3(const unsigned char* &data, size_t& bytes_available);
  // base offsets
  int64_t offsets_base = 0, // dwarf5 from DW_AT_str_offsets_base
   addr_base = 0,     // dwarf5 from DW_AT_addr_base
   loclist_base = 0,  // dwarf5 from DW_AT_loclists_base
   rnglists_base = 0; // dwarf5 from DW_AT_rnglists_base
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
  bool get_filename(unsigned int fid, std::string &, const char *&);
  // sections from original elf file
  std::vector<saved_section> m_orig_sects;
  bool m_regged;
  // free sections flags
  bool free_info = false,
   free_abbrev = false,
   free_strings = false,
   free_str_offsets = false,
   free_addr = false,
   free_loc = false,
   free_line = false,
   free_line_str = false,
   free_loclists = false,
   free_rnglists = false,
   free_ranges = false,
   free_frame = false,
   is_eh = false;
  // data for rnglists
  struct rnglist_ctx {
    short version;
    unsigned char addr_size, offset_size;
    int64_t start, end;
  };
  std::vector<rnglist_ctx> m_rnglists;
  bool parse_rnglists();
  void read_range(Dwarf32::Form form, const unsigned char* &info, size_t& bytes_available, uint64_t &value);
  bool get_rnglistx_(int64_t off, std::list<std::pair<uint64_t, uint64_t> > &);
  bool get_old_range(int64_t off, uint64_t base_addr, unsigned char addr_size, std::list<std::pair<uint64_t, uint64_t> > &);
  virtual bool get_rnglistx(int64_t off, uint64_t base_addr, unsigned char addr_size, std::list<std::pair<uint64_t, uint64_t> > &);
};
