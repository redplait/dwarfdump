#include "ElfFile.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <elf.h>

int g_opt_d = 0,
    g_opt_f = 0,
    g_opt_F = 0,
    g_opt_g = 0,
    g_opt_k = 0,
    g_opt_l = 0,
    g_opt_s = 0,
    g_opt_L = 0,
    g_opt_V = 0,
    g_opt_v = 0,
    g_opt_x = 0;
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

void dump2file(ELFIO::section *s, const char *pfx, const void *data, size_t size)
{
  std::string tmp = "section.";
  tmp += s->get_name();
  tmp += pfx;
  dump2file(tmp, data, size);
}

template <typename T>
bool ElfFile::uncompressed_section(ELFIO::section *s, const unsigned char * &data, size_t &size)
{
  data = nullptr;
  if ( s->get_size() < sizeof(T) )
  {
    fprintf(stderr, "compressed section %s is too short, size %lX\n", s->get_name().c_str(), s->get_size());
    return false;
  }
  const char *sdata = s->get_data();
  const T* hdr = (const T*)sdata;
  size = hdr->ch_size;
  if ( g_opt_d )
    fprintf(stderr, "compressed section %s type %d size %lX\n", s->get_name().c_str(), hdr->ch_type, (Elf64_Xword)hdr->ch_size);
  if ( hdr->ch_type != ELFCOMPRESS_ZLIB )
  {
    fprintf(stderr, "compressed section %s has unknown type %d\n", s->get_name().c_str(), hdr->ch_type);
    return false;
  }
  unsigned char *buf = (unsigned char *)malloc(size);
  if ( !buf )
  {
    fprintf(stderr, "cannot alooc unompressed size %lX, section size %lX\n", size, s->get_size() - sizeof(T));
    return false;
  }
  if ( g_opt_d )
    dump2file(s, ".comp", sdata, s->get_size());
  memset(buf, 0, size);
  int err = uncompress(buf, &size, (Bytef *)(sdata + sizeof(T)), s->get_size() - sizeof(T));
  if ( err != Z_OK )
  {
    fprintf(stderr, "uncompress failed, err %d\n", err);
    free(buf);
    return false;
  }
  data = buf;
  if ( g_opt_d )
    dump2file(s, ".ucomp", data, size);
  return true;
}

bool ElfFile::check_compressed_section(ELFIO::section *s, const unsigned char * &data, size_t &size)
{
  if ( ! (s->get_flags() & SHF_COMPRESSED) )
    return false;
  if ( reader.get_class() == ELFCLASS64 )
    return uncompressed_section<Elf64_Chdr>(s, data, size);
  else
    return uncompressed_section<Elf32_Chdr>(s, data, size);
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
    dump2file(s, ".comp", sdata, s->get_size());
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
    dump2file(s, ".ucomp", data, size);
  return true;
}

ElfFile::ElfFile(std::string filepath, bool& success, TreeBuilder *tb) :
  tree_builder(tb),
  debug_info_(nullptr), debug_info_size_(0),
  debug_abbrev_(nullptr), debug_abbrev_size_(0),
  debug_loc_(nullptr), debug_loc_size_(0),
  debug_str_offsets_(nullptr), debug_str_offsets_size_(0),
  debug_addr_(nullptr), debug_addr_size_(0),
  debug_loclists_(nullptr), debug_loclists_size_(0),
  debug_line_(nullptr), debug_line_size_(0),
  debug_line_str_(nullptr), debug_line_str_size_(0),
  offsets_base(0), addr_base(0), loclist_base(0),
  free_info(false), free_abbrev(false), free_strings(false), free_str_offsets(false), 
  free_addr(false), free_loc(false), free_line(false), free_line_str(false), free_loclists(false)
{
  // read elf file
  if ( !reader.load(filepath.c_str()) )
  {
    fprintf(stderr, "ERR: Failed to open '%s'\n", filepath.c_str());
    success = false;
    return;
  }
  success = true;

  // compressed sections
  section 
   *zinfo = nullptr,
   *zabbrev = nullptr,
   *zstrings = nullptr,
   *zloc = nullptr,
   *zline = nullptr,
   *zline_str = nullptr,
   *zstr_off = nullptr,
   *zaddr = nullptr,
   *zloclists = nullptr;
  // Search the .debug_info and .debug_abbrev
  tree_builder->debug_str_ = nullptr;
  tree_builder->debug_str_size_ = 0;
  Elf_Half n = reader.sections.size();
  for ( Elf_Half i = 0; i < n; i++) {
    section *s = reader.sections[i];
    const char* name = s->get_name().c_str();
    if (!strcmp(name, ".debug_info")) {
      debug_info_ = reinterpret_cast<const unsigned char*>(s->get_data());
      debug_info_size_ = s->get_size();
      if ( check_compressed_section(s, debug_info_, debug_info_size_) )
        free_info = true;
    } else if (!strcmp(name, ".debug_abbrev")) {
      debug_abbrev_ = reinterpret_cast<const unsigned char*>(s->get_data());
      debug_abbrev_size_ = s->get_size();
      if ( check_compressed_section(s, debug_abbrev_, debug_abbrev_size_) )
        free_abbrev = true;
    } else if (!strcmp(name, ".debug_str")) {
      tree_builder->debug_str_ = reinterpret_cast<const unsigned char*>(s->get_data());
      tree_builder->debug_str_size_ = s->get_size();
      if ( check_compressed_section(s, tree_builder->debug_str_, tree_builder->debug_str_size_) )
        free_strings = true;
    } else if (!strcmp(s->get_name().c_str(), ".debug_loclists")) {
      debug_loclists_ = reinterpret_cast<const unsigned char*>(s->get_data());
      debug_loclists_size_ = s->get_size();
      if ( check_compressed_section(s, debug_loclists_, debug_loclists_size_) )
        free_loclists = true;
    } else if (!strcmp(s->get_name().c_str(), ".debug_str_offsets")) {
      debug_str_offsets_ = reinterpret_cast<const unsigned char*>(s->get_data());
      debug_str_offsets_size_ = s->get_size();
      if ( check_compressed_section(s, debug_str_offsets_, debug_str_offsets_size_) )
        free_str_offsets = true;
      // printf("debug_str_offsets_size %lx\n", debug_str_offsets_size_);
    } else if (!strcmp(s->get_name().c_str(), ".debug_addr")) {
      debug_addr_ = reinterpret_cast<const unsigned char*>(s->get_data());
      debug_addr_size_ = s->get_size();
      if ( check_compressed_section(s, debug_addr_, debug_addr_size_) )
        free_addr = true;
    } else if (!strcmp(name, ".debug_loc")) {
      debug_loc_ = reinterpret_cast<const unsigned char*>(s->get_data());
      debug_loc_size_ = s->get_size();
      if ( check_compressed_section(s, debug_loc_, debug_loc_size_) )
        free_loc = true;
    } else if (g_opt_F && !strcmp(name, ".debug_line")) {
      debug_line_ = reinterpret_cast<const unsigned char*>(s->get_data());
      debug_line_size_ = s->get_size();
      if ( check_compressed_section(s, debug_line_, debug_line_size_) )
        free_line = true;
    } else if (g_opt_F && !strcmp(name, ".debug_line_str")) {
      debug_line_str_ = reinterpret_cast<const unsigned char*>(s->get_data());
      debug_line_str_size_ = s->get_size();
      if ( check_compressed_section(s, debug_line_str_, debug_line_str_size_) )
        free_line_str = true;
    } // check compressed versions
    else if ( !strcmp(name, ".zdebug_info") )
      zinfo = s;
    else if ( !strcmp(name, ".zdebug_abbrev") )
      zabbrev = s;
    else if ( !strcmp(name, ".zdebug_str") )
      zstrings = s;
    else if ( !strcmp(name, ".zdebug_loc") )
      zloc = s;
    else if ( g_opt_F && !strcmp(name, ".zdebug_line") )
      zline = s;
    else if ( g_opt_F && !strcmp(name, ".zdebug_line_str") )
      zline_str = s;
    else if ( !strcmp(name, ".zdebug_str_offsets") )
      zstr_off = s;
    else if ( !strcmp(name, ".zdebug_addr") )
      zaddr = s;
    else if ( !strcmp(name, ".zdebug_loclists") )
      zloclists = s;
  }
  // check if we need to decompress some sections
#define UNPACK_ZSECTION(zsec, zs_, zs_size, free_zs) \
  if ( zsec ) \
  { \
    if ( !unzip_section(zsec, zs_, zs_size) ) \
    { \
      fprintf(stderr, "cannot unpack section %s\n", zinfo->get_name().c_str()); \
      success = false; \
      return; \
    } \
    free_zs = true; \
  }
  UNPACK_ZSECTION(zinfo, debug_info_, debug_info_size_, free_info)
  UNPACK_ZSECTION(zabbrev, debug_abbrev_, debug_abbrev_size_, free_abbrev)
  UNPACK_ZSECTION(zstrings, tree_builder->debug_str_, tree_builder->debug_str_size_, free_strings)
  UNPACK_ZSECTION(zloc, debug_loc_, debug_loc_size_, free_loc)
  UNPACK_ZSECTION(zline, debug_line_, debug_line_size_, free_line)
  UNPACK_ZSECTION(zline_str, debug_line_str_, debug_line_str_size_, free_line_str)
  UNPACK_ZSECTION(zstr_off, debug_str_offsets_, debug_str_offsets_size_, free_str_offsets)
  UNPACK_ZSECTION(zaddr, debug_addr_, debug_addr_size_ , free_addr)
  UNPACK_ZSECTION(zloclists, debug_loclists_, debug_loclists_size_, free_loclists)

  tree_builder->m_rnames = get_regnames(reader.get_machine());
  tree_builder->m_snames = this;
  success = (debug_info_ && debug_abbrev_);
}

void ElfFile::free_section(const unsigned char *&s, bool f)
{
  if ( f && s != nullptr )
    free((void *)s);
  s = nullptr;
}

ElfFile::~ElfFile() 
{
  free_section(debug_info_, free_info);
  free_section(debug_abbrev_, free_abbrev);
  if ( tree_builder )
    free_section(tree_builder->debug_str_, free_strings);
  free_section(debug_loc_, free_loc);
  free_section(debug_line_, free_line);
  free_section(debug_line_str_, free_line_str);
  free_section(debug_str_offsets_, free_str_offsets);
  free_section(debug_addr_, free_addr);
  free_section(debug_loclists_, free_loclists);
}

bool ElfFile::read_debug_lines()
{
  if ( debug_line_ == nullptr )
    return false;
  m_li.m_ptr = nullptr;
  // check that this unit is still inside section
  if ( m_curr_lines >= debug_line_ + debug_line_size_ )
    return false;
  auto ptr = m_curr_lines;
  size_t ba = debug_line_size_ - (m_curr_lines - debug_line_);
  size_t addr_size = 0;
  // printf("read_debug_lines: %lX ba %lX\n", m_curr_lines - debug_line_, ba);
  reset_lines();
  if ( ba < 4 )
    return false;
  m_li.li_length = *(const uint32_t *)(ptr);
  ptr += 4;
  ba -= 4;
  addr_size = 4;
  if ( 0xffffffff == m_li.li_length )
  {
    if ( ba < 8 )
      return false;
    m_li.li_length = *(const uint64_t *)(ptr);
    ptr += 8;
    ba -= 8;
    addr_size += 8;
    m_li.li_offset_size = 8;    
  } else
    m_li.li_offset_size = 4;
  // version
  if ( ba < 2 )
    return false;
  m_li.li_version = *(const uint16_t *)(ptr);
  ptr += 2;
  ba -= 2;
  if ( m_li.li_version >= 5 )
  {
    if ( ba < 2 )
      return false;
    m_li.li_address_size = *static_cast<const uint8_t *>(ptr);
    ptr++;
    m_li.li_segment_size = *static_cast<const uint8_t *>(ptr);
    ptr++;
    ba -= 2;    
  }
  // prolog_length
  if ( ba < m_li.li_offset_size )
    return false;
  if ( 4 == m_li.li_offset_size )
  {
    m_li.li_prologue_length = *(const uint32_t *)(ptr);
  } else {
    m_li.li_prologue_length = *(const uint64_t *)(ptr);
  }
  ptr += m_li.li_offset_size;
  ba -= m_li.li_offset_size;
  // min_insn_length
  if ( !ba )
    return false;
  m_li.li_min_insn_length = *static_cast<const uint8_t *>(ptr);
  ptr++;
  ba--;
  if ( m_li.li_version >= 4 )
  {
    if ( !ba )
      return false;
    m_li.li_max_ops_per_insn = *static_cast<const uint8_t *>(ptr);
    ptr++;
    ba--;
    if ( !m_li.li_max_ops_per_insn )
      return false; 
  } else 
    m_li.li_max_ops_per_insn = 1;
  if ( ba < 4 )
    return false;
  m_li.li_default_is_stmt = *static_cast<const uint8_t *>(ptr);
  ptr++;
  m_li.li_line_base = *(const int8_t *)(ptr);
  ptr++;
  m_li.li_line_range = *static_cast<const uint8_t *>(ptr);
  ptr++;
  m_li.li_opcode_base = *static_cast<const uint8_t *>(ptr);
  ptr++;
  DBG_PRINTF("Length: %lX\n", m_li.li_length);
  DBG_PRINTF("version: %d\n", m_li.li_version);
  if ( m_li.li_version >= 5 )
  {
    DBG_PRINTF("address size: %d\n", m_li.li_address_size);
    DBG_PRINTF("segment selector: %d bytes\n", m_li.li_segment_size);
  }
  DBG_PRINTF("prolog length: %ld\n", m_li.li_prologue_length);
  DBG_PRINTF("min insn length: %d\n", m_li.li_min_insn_length);
  if ( m_li.li_version >= 4 )
    DBG_PRINTF("max insn length: %d\n", m_li.li_max_ops_per_insn);
  DBG_PRINTF("default_is_stmt: %d\n", m_li.li_default_is_stmt);
  DBG_PRINTF("line base: %d\n", m_li.li_line_base);
  DBG_PRINTF("line range %d\n", m_li.li_line_range);
  DBG_PRINTF("opcode base: %d\n", m_li.li_opcode_base);
  m_curr_lines += addr_size + m_li.li_length;
  // contents of the Directory table
  ptr += m_li.li_opcode_base - 1;
  if ( m_li.li_version < 5 )
  {
    // process Dir Name table
    if ( *ptr )
    {
      unsigned int last_dir_entry = 0;
      while( ptr < m_curr_lines && *ptr != 0 )
      {
        last_dir_entry++;
        m_dl_dirs[last_dir_entry] = (const char *)ptr;
        if ( g_opt_d )
          fprintf(g_outf, "dir %d %s\n", last_dir_entry, ptr);
        size_t len = strlen((const char *)ptr);
        ptr += 1 + len;
        ba -= 1 + len;
      }
      /* Skip the NULL at the end of the table.  */
	    if ( ptr < m_curr_lines )
      {
		    ptr++;
        ba--;
      }
    }
    // process File Name table
    if ( *ptr )
    {
      unsigned int last_file_entry = 0;
      while( ptr < m_curr_lines && *ptr != 0 )
      {
        last_file_entry++;
        const char *name = (const char *)ptr;
        size_t len = strlen((const char *)ptr);
        ptr += 1 + len;
        ba -= 1 + len;
        uint64_t dir, time, size;
        dir = ULEB128(ptr, ba);
        time = ULEB128(ptr, ba);
        size = ULEB128(ptr, ba);
        // put to file names map
        m_dl_files[last_file_entry] = { (unsigned int)dir, name };
        if ( g_opt_d )
          fprintf(g_outf, "file %d dir %ld size %ld time %ld %s\n", last_file_entry, dir, size, time, name);
      }     
      /* Skip the NULL at the end of the table.  */
	    if ( ptr < m_curr_lines )
      {
        ptr++;
        ba--;
      }
    }
  } else {
    m_li.m_ptr = ptr;
    return true; // safe to skip this unit bcs m_curr_lines points to next one
  }
  return true;
}

bool ElfFile::read_delayed_lines()
{
  if ( debug_line_ == nullptr )
    return false;
  if ( !offsets_base )
    return false;
  if ( !m_li.m_ptr )
    return false;
  m_li.m_ptr = read_formatted_table(true);
  if ( !m_li.m_ptr )
    return false;
  m_li.m_ptr = read_formatted_table(false);
  if ( !m_li.m_ptr )
    return false;
  return true;
}

const unsigned char *ElfFile::read_formatted_table(bool is_dir)
{
  auto ptr = m_li.m_ptr;
  size_t bytes_available = m_curr_lines - ptr;
  unsigned char format_count = *ptr;
  ptr++;
  bytes_available--;
  // first - DW_LNCT_XX, second - attr form
  std::vector<std::pair<uint64_t, uint64_t> > columns;
  columns.resize(format_count);
  for ( unsigned char formati = 0; formati < format_count; ++formati )
  {
    auto a1 = ULEB128(ptr, bytes_available);
    auto a2 = ULEB128(ptr, bytes_available);
    columns[formati] = { a1, a2 };
  }
  uint64_t data_count = ULEB128(ptr, bytes_available);
  if ( !data_count || !format_count || !bytes_available )
    return ptr;
  for ( uint64_t datai = 0; datai < data_count; ++datai )
  {
    const char *name = nullptr;
    uint64_t idx = -1;
    for ( unsigned char formati = 0; formati < format_count; ++formati )
    {
      // read_and_display_attr_value args:
      // attribute
      // form
      // implicit_const
      // start
      // data
      // end
      // cu_offset
      // pointer_size
      // offset_size
      // dwarf_version
      // ...
      if ( columns[formati].first == Dwarf32::dwarf_line_number_content_type::DW_LNCT_path )
      {
        // fprintf(stderr, "path form %lx for %s\n", columns[formati].second, is_dir ? "dirs" : "fnames");
        uint32_t str_pos;
        switch(columns[formati].second)
        {
          case Dwarf32::Form::DW_FORM_strx4:
            str_pos = *reinterpret_cast<const uint32_t*>(ptr);
            ptr += 4;
            bytes_available -= 4;
            name = check_strx4(str_pos);
           break;
          case Dwarf32::Form::DW_FORM_strx2:
            str_pos = *reinterpret_cast<const uint16_t*>(ptr);
            ptr += 2;
            bytes_available -= 2;
            name = check_strx2(str_pos);
           break;
          case Dwarf32::Form::DW_FORM_strx3:
            str_pos = read_x3(ptr, bytes_available);
            name = check_strx3(str_pos);
           break;
          case Dwarf32::Form::DW_FORM_strx1:
            str_pos = *reinterpret_cast<const uint8_t*>(ptr);
            ptr += 1;
            bytes_available -= 1;
            name = check_strx1(str_pos);
           break;
          case Dwarf32::Form::DW_FORM_line_strp:
            str_pos = *reinterpret_cast<const uint32_t*>(ptr);
          // fprintf(stderr, "srtp value %X\n", str_pos);  
            ptr += 4;
            bytes_available -= 4;
            name = check_strp(str_pos);
           break;
          default:
           fprintf(stderr, "unknown path form %lX in read_formatted_table for %s\n", columns[formati].second,
             is_dir ? "dirs" : "fnames"
           );
           return nullptr;
        }
        // store results
        if ( is_dir && name )
        {
          m_dl_dirs[datai] = (const char *)name;
          if ( g_opt_d )
            fprintf(g_outf, "dir %ld %s\n", datai, name);
          name = nullptr;
        } else if ( !is_dir && name && idx != (uint64_t)-1 )
        {
          // put to file names map
          m_dl_files[datai] = { (unsigned int)idx, name };
          if ( g_opt_d )
            fprintf(g_outf, "file %ld dir %ld %s\n", datai, idx, name);
          name = nullptr;
          idx = -1;
        }
      } else if ( !is_dir && columns[formati].first == Dwarf32::dwarf_line_number_content_type::DW_LNCT_directory_index )
      {
        // fprintf(stderr, "dir form %lx %x\n", columns[formati].second, Dwarf32::Form::DW_FORM_ref_udata);
        if ( columns[formati].second == Dwarf32::Form::DW_FORM_ref_udata ||
             columns[formati].second == Dwarf32::Form::DW_FORM_udata
           )
          idx = ElfFile::ULEB128(ptr, bytes_available);
        else if ( columns[formati].second == Dwarf32::Form::DW_FORM_data1 )
        {
          idx = *reinterpret_cast<const uint8_t*>(ptr);
          ptr++;
          bytes_available--;
        } else if ( columns[formati].second == Dwarf32::Form::DW_FORM_data2 )
        {
          idx = *reinterpret_cast<const uint8_t*>(ptr);
          ptr += 2;
          bytes_available -= 2;
        } else {
           fprintf(stderr, "unknown directory_index form %lX in read_formatted_table for %s\n", columns[formati].second,
             is_dir ? "dirs" : "fnames"
           );
           return nullptr;
        }
        if ( name && idx != (uint64_t)-1 )
        {
          // put to file names map
          m_dl_files[datai] = { (unsigned int)idx, name };
          if ( g_opt_d )
            fprintf(g_outf, "file %ld dir %ld %s\n", datai, idx, name);
          name = nullptr;
          idx = -1;
        }
      } else {
        // skip
        ElfFile::PassData((Dwarf32::Form)columns[formati].second, ptr, bytes_available);
      }
    }
  }
  return ptr;
}

bool ElfFile::get_filename(unsigned int fid, std::string &res, const char *&f)
{
  auto fiter = m_dl_files.find(fid);
  if ( fiter == m_dl_files.end() )
    return false;
  f = fiter->second.second;
  auto diter = m_dl_dirs.find(fiter->second.first);
  if ( diter != m_dl_dirs.end() )
  {
    res = diter->second;
    res += '/'; // add dirs separator
  }
  res += fiter->second.second;
  return true;
}

bool ElfFile::SaveSections(std::string &fn)
{
  elfio orig;
  if ( !orig.load(fn.c_str()) )
  {
    fprintf(stderr, "SaveSections: failed to open '%s'\n", fn.c_str());
    return false;
  }
  Elf_Half n = orig.sections.size();
  for ( Elf_Half i = 0; i < n; i++) 
  {
    section *s = orig.sections[i];
    m_orig_sects.push_back({ s->get_name(), s->get_address(), s->get_size()});
  }
  return !m_orig_sects.empty();
}

const char *ElfFile::find_sname(uint64_t addr)
{
  if ( !m_orig_sects.empty() )
  {
    for ( auto &s: m_orig_sects )
    {
      if ( (addr >= s.offset) && (addr < s.offset + s.len)
         )
        return s.name.c_str();
    }
    return nullptr;    
  }
  Elf_Half n = reader.sections.size();
  for ( Elf_Half i = 0; i < n; i++) 
  {
    section *s = reader.sections[i];
    auto s_addr = s->get_address();
    if ( (addr >= s_addr) && (addr < s_addr + s->get_size())
       )
      return s->get_name().c_str();
  }
  return nullptr;
}

// static
uint32_t ElfFile::ULEB128(const unsigned char* &data, size_t& bytes_available) {
  uint64_t result = 0;

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

int64_t ElfFile::SLEB128(const unsigned char* &data, size_t& bytes_available) {
  uint64_t result = 0;
  unsigned char byte = 0;
  unsigned int shift = 0;
  while (bytes_available > 0) {
    byte = *data;
    data++;
    bytes_available--;
    result |= (byte & 0x7f) << shift;
    shift+=7;
    if ( !(byte & 0x80) )
      break;
  }
  if ( shift < 8 * sizeof(result) && (byte & 0x40) )
    result |= -(((uint64_t) 1) << shift);
  return (int64_t)result;
}

uint32_t ElfFile::read_x3(const unsigned char* &data, size_t& bytes_available)
{
  uint32_t res = 0;
  if ( bytes_available < 3)
  {
    fprintf(stderr, "read_x3 tries to read behind available data at %lx\n", data - debug_info_);
    return 0;
  }
  memcpy(&res, data, 3);
  data += 3;
  bytes_available -= 3;
  return res;
}

void ElfFile::PassData(Dwarf32::Form form, const unsigned char* &data, size_t& bytes_available) 
{
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
    case Dwarf32::Form::DW_FORM_addrx1:
    case Dwarf32::Form::DW_FORM_data1:
    case Dwarf32::Form::DW_FORM_strx1:
      data++;
      bytes_available--;
      break;
    case Dwarf32::Form::DW_FORM_addrx2:  
    case Dwarf32::Form::DW_FORM_data2:
    case Dwarf32::Form::DW_FORM_strx2:
      data += 2;
      bytes_available -= 2;
      break;
    case Dwarf32::Form::DW_FORM_addrx3:
    case Dwarf32::Form::DW_FORM_strx3:
      data += 3;
      bytes_available -= 3;
      break;
    case Dwarf32::Form::DW_FORM_addrx4:  
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
    case Dwarf32::Form::DW_FORM_loclistx:
    case Dwarf32::Form::DW_FORM_rnglistx:
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
    case Dwarf32::Form::DW_FORM_line_strp:
    case Dwarf32::Form::DW_FORM_sec_offset:
      data += 4; // actually this is offset_size
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
      fprintf(stderr, "ERR: Unpexpected form type 0x%x at %lx\n", form,  data - debug_info_);
      break;
  }
}

uint64_t ElfFile::fetch_indexed_value(uint64_t idx, const unsigned char *s, uint64_t s_size, uint64_t base)
{
  if ( !s )
    return -1;
  if ( s_size < 4 )
    return -1;
  uint32_t pointer_size, bias;
  if ( *reinterpret_cast<const uint32_t*>(s) == 0xffffffff )
  {
    pointer_size = 8;
    bias = 20;
  } else {
    pointer_size = 4;
    bias = 12;
  }
  uint64_t offset = idx * pointer_size;
  if ( base )
    offset += base;
  else
    offset += bias;
  if ( offset + pointer_size > s_size )
  {
    fprintf(stderr, "fetch_indexed_value tries to read behind available data at %lx, section size %lx\n", offset, s_size);
    return -1;
  }
  if ( 4 == pointer_size )
    return *reinterpret_cast<const uint32_t*>(s + offset);
  else
    return *reinterpret_cast<const uint64_t*>(s + offset);
}

// ripped from functions display_offset_entry_loclists & display_loclists_list in dwarf.c
bool ElfFile::get_loclistx(uint64_t off, std::list<LocListXItem> &out_list, uint64_t func_base)
{
  if ( off > debug_loclists_size_ )
  {
    fprintf(stderr, "loclistx off %lx is not inside loclists section size %lx\n", off, debug_loclists_size_);
    return false;
  }
  uint64_t addr, base_addr = func_base;
  uint64_t begin = 0, end = 0;
  const unsigned char *start = debug_loclists_ + off;
  const unsigned char *lend = debug_loclists_ + debug_loclists_size_;
  size_t avail = debug_loclists_size_ - off;
  while( start < lend )
  {
//  fprintf(stderr, "get_loclistx: off %lx start %lx %d ", off, start - debug_loclists_, address_size_);
    unsigned char llet = *start;
    start++;
    avail--;
//  fprintf(stderr, "%d\n", llet);  
    if ( llet == Dwarf32::dwarf_location_list_entry_type::DW_LLE_end_of_list )
      break;
    switch(llet)
    {
      case Dwarf32::dwarf_location_list_entry_type::DW_LLE_base_address:
        if ( address_size_ == 8 )
        {
          base_addr = *(uint64_t *)start;
          start += 8;
          avail -= 8;
        } else {
          base_addr = *(uint32_t *)start;
          start += 4;
          avail -= 4;
        }
        break;
      case Dwarf32::dwarf_location_list_entry_type::DW_LLE_base_addressx:
         addr = ElfFile::ULEB128(start, avail);
         base_addr = get_indexed_addr(addr, address_size_);
        break;
      case Dwarf32::dwarf_location_list_entry_type::DW_LLE_startx_endx:
         addr = ElfFile::ULEB128(start, avail);
         begin = get_indexed_addr(addr, address_size_);
         addr = ElfFile::ULEB128(start, avail);
         end = get_indexed_addr(addr, address_size_);
        break;
      case Dwarf32::dwarf_location_list_entry_type::DW_LLE_start_end:
         if ( address_size_ == 8 )
         {
          begin = *(uint64_t *)start;
          start += 8;
          avail -= 8;
          end = *(uint64_t *)start;
          start += 8;
          avail -= 8;
         } else {
          begin = *(uint32_t *)start;
          start += 4;
          avail -= 4;
          end = *(uint32_t *)start;
          start += 4;
          avail -= 4;
         }
        break; 
      case Dwarf32::dwarf_location_list_entry_type::DW_LLE_offset_pair:
         begin = ElfFile::ULEB128(start, avail);
         begin += base_addr;
         end = ElfFile::ULEB128(start, avail);
         end += base_addr;
        break;
      case Dwarf32::dwarf_location_list_entry_type::DW_LLE_start_length:
        if ( address_size_ == 8 )
        {
          begin = *(uint64_t *)start;
          start += 8;
          avail -= 8;
        } else {
          begin = *(uint32_t *)start;
          start += 4;
          avail -= 4;
        }
        addr = ElfFile::ULEB128(start, avail);
        end = addr + begin;
        break;
      case Dwarf32::dwarf_location_list_entry_type::DW_LLE_startx_length:
         addr = ElfFile::ULEB128(start, avail);
         begin = get_indexed_addr(addr, address_size_);
         addr = ElfFile::ULEB128(start, avail);
         end = begin + addr;
        break;
      case Dwarf32::dwarf_location_list_entry_type::DW_LLE_default_location:
         begin = end = 0;
        break;
      default:
        fprintf(stderr, "unknown LLE tag %d at %lx\n", llet, start - debug_loclists_);
        return false;
    }
    if ( llet == Dwarf32::dwarf_location_list_entry_type::DW_LLE_base_address ||
         llet == Dwarf32::dwarf_location_list_entry_type::DW_LLE_base_addressx
       )
      continue;
    out_list.push_back( { begin, end } );
    auto &top = out_list.back();
    // length will be readed inside DecodeAddrLocation
    size_t tmp_avail = avail;
    auto tmp_start = start;
    uint64_t len = ElfFile::ULEB128(tmp_start, tmp_avail);
    if ( len > avail )
    {
      fprintf(stderr, "bad LLE len %ld at %lx\n", len, start - debug_loclists_);
      break;
    }
//  fprintf(stderr, "len %lX at %lx %lX - %lX\n", len, start - debug_loclists_, begin, end);
    DecodeAddrLocation(Dwarf32::Form::DW_FORM_block, start, avail, &top.loc, debug_loclists_);
    start = tmp_start + len;
    avail = tmp_avail - len;
  }
  return !out_list.empty();
}

// var addresses decoded as block + OP_addr
uint64_t ElfFile::DecodeAddrLocation(Dwarf32::Form form, const unsigned char* data, size_t bytes_available, param_loc *pl, const unsigned char *sect) 
{
  ptrdiff_t doff = data - sect; // debug_info_;
  // fprintf(stderr, "DecodeAddrLocation form %d off %lX\n", form, doff);
  if ( form == Dwarf32::Form::DW_FORM_sec_offset )
    return 0;
  uint32_t length = 0;
  uint64_t lindex;
  uint64_t laddr = 0;
  switch(form)
  {
    case Dwarf32::Form::DW_FORM_loclistx:
      lindex = ElfFile::ULEB128(data, bytes_available);
      if ( !loclist_base )
      {
        fprintf(stderr, "no loclist_base for DW_FORM_loclistx at %lx\n", data - debug_info_);
        return 0;
      }
      laddr = fetch_indexed_value(lindex, debug_loclists_, debug_loclists_size_, loclist_base);
      if ( (uint64_t)-1 == laddr )
        return 0;
      laddr += loclist_base;
      if ( laddr > debug_loclists_size_ )
      {
        fprintf(stderr, "laddr %lx is not inside loclists section size %lx\n", laddr, debug_loclists_size_);
        return 0;
      }
      data = (debug_loclists_ + laddr);
      // store locations block with DW_LLE here - it will be parsed later within get_loclistx virtual method
      tree_builder->SetLocX(laddr);
      return 0;
     break;
    // case Dwarf32::Form::DW_FORM_addrx:
    case Dwarf32::Form::DW_FORM_exprloc:
    case Dwarf32::Form::DW_FORM_block:
      length = ElfFile::ULEB128(data, bytes_available);
      break;
    case Dwarf32::Form::DW_FORM_block1:
      length = *reinterpret_cast<const uint8_t*>(data);
      data += sizeof(uint8_t);
      bytes_available -= sizeof(uint8_t);
      break;
    case Dwarf32::Form::DW_FORM_block2:
      length = *reinterpret_cast<const uint16_t*>(data);
      data += sizeof(uint16_t);
      bytes_available -= sizeof(uint16_t);
      break;
    case Dwarf32::Form::DW_FORM_block4:
      length = *reinterpret_cast<const uint32_t*>(data);
      data += sizeof(uint32_t);
      bytes_available -= sizeof(uint32_t);
      break;
    default:
      fprintf(stderr, "DecodeAddrLocation: unknown form %X at %lX\n", form, doff);
      return 0;
  }
  const unsigned char *end = data + length;
  int value = 0;
  uint64_t v64 = 0;
  int64_t s64;
  while( data < end && bytes_available )
  {
    unsigned op = *data;
    data++;
    bytes_available--;
    // ignore DW_OP_litX
    if ( op >= Dwarf32::dwarf_ops::DW_OP_lit0 && op <= Dwarf32::dwarf_ops::DW_OP_lit31 )
    {
      if ( pl )
        pl->push_value(op - Dwarf32::dwarf_ops::DW_OP_lit0);
      continue;
    }
    switch(op)
    {
      case Dwarf32::dwarf_ops::DW_OP_addrx:
         v64 = ElfFile::ULEB128(data, bytes_available);
         return get_indexed_addr(v64, address_size_);
        break;
      case Dwarf32::dwarf_ops::DW_OP_addr:
        if ( address_size_ == 8 )
          return *reinterpret_cast<const uint64_t*>(data);
        else
          return *reinterpret_cast<const uint32_t*>(data);
       break;
      // from function decode_locdesc in read.c
        case Dwarf32::dwarf_ops::DW_OP_call_frame_cfa:
           pl->locs.push_back({ call_frame_cfa, 0, 0});
          break;
        case Dwarf32::dwarf_ops::DW_OP_deref:
           pl->locs.push_back({ deref, 0, 0});
          break;
        case Dwarf32::dwarf_ops::DW_OP_convert:
        case Dwarf32::dwarf_ops::DW_OP_GNU_convert:
        case Dwarf32::dwarf_ops::DW_OP_reinterpret:
        case Dwarf32::dwarf_ops::DW_OP_GNU_reinterpret:
          // op for convert is relative from cu base
          v64 = ElfFile::ULEB128(data, bytes_available);
          pl->push_conv(v64 + cu_base);
          break;
        case Dwarf32::dwarf_ops::DW_OP_consts:
           pl->push_svalue(ElfFile::SLEB128(data, bytes_available));
          break;  
        case Dwarf32::dwarf_ops::DW_OP_constu:
           pl->push_uvalue(ElfFile::ULEB128(data, bytes_available));
          break;
        case Dwarf32::dwarf_ops::DW_OP_plus_uconst:
           pl->locs.push_back({ plus_uconst, 0, (int)ElfFile::ULEB128(data, bytes_available)});
          break;
        case Dwarf32::dwarf_ops::DW_OP_regval_type:
        case Dwarf32::dwarf_ops::DW_OP_GNU_regval_type:
           value = (int)ElfFile::ULEB128(data, bytes_available);
           // op for convert is relative from cu base
           v64 = ElfFile::ULEB128(data, bytes_available);
           pl->push_regval_type(v64 + cu_base, value);
          break;
        case Dwarf32::dwarf_ops::DW_OP_deref_type:
        case Dwarf32::dwarf_ops::DW_OP_GNU_deref_type:
           value = (int)*reinterpret_cast<const uint8_t*>(data);
           bytes_available -= 1;
           data += 1;
           // op for convert is relative from cu base
           v64 = ElfFile::ULEB128(data, bytes_available);
           pl->push_deref_type(v64 + cu_base, value);
          break;
        case Dwarf32::dwarf_ops::DW_OP_deref_size:
           value = (int)*reinterpret_cast<const uint8_t*>(data);
           bytes_available -= 1;
           data += 1;
           pl->locs.push_back({ deref_size, (unsigned int)value, 0 } );
          break;
        case Dwarf32::dwarf_ops::DW_OP_const1u:
           value = (int)*reinterpret_cast<const uint8_t*>(data);
           pl->push_value(value);
           bytes_available -= 1;
           data += 1;
          break;
        case Dwarf32::dwarf_ops::DW_OP_const1s:
           value = (int)*reinterpret_cast<const int8_t*>(data);
           pl->push_svalue(value);
           bytes_available -= 1;
           data += 1;
          break;
        case Dwarf32::dwarf_ops::DW_OP_const2u:
           value = (int)*reinterpret_cast<const uint16_t*>(data);
           pl->push_value(value);
           bytes_available -= 2;
           data += 2;
          break;
        case Dwarf32::dwarf_ops::DW_OP_const2s:
           value = (int)*reinterpret_cast<const int16_t*>(data);
           pl->push_svalue(value);
           bytes_available -= 2;
           data += 2;
          break;
        case Dwarf32::dwarf_ops::DW_OP_const4u:
           value = (int)*reinterpret_cast<const uint32_t*>(data);
           pl->push_value(value);
           bytes_available -= 4;
           data += 4;
          break;
        case Dwarf32::dwarf_ops::DW_OP_const4s:
           value = (int)*reinterpret_cast<const int32_t*>(data);
           pl->push_svalue(value);
           bytes_available -= 4;
           data += 4;
          break;
        case Dwarf32::dwarf_ops::DW_OP_const8u:
           pl->push_uvalue(*reinterpret_cast<const uint64_t*>(data));
           bytes_available -= 8;
           data += 8;
          break;
        case Dwarf32::dwarf_ops::DW_OP_const8s:
           s64 = *reinterpret_cast<const int64_t*>(data);
           pl->push_svalue(s64);
           bytes_available -= 8;
           data += 8;
          break;
        case Dwarf32::dwarf_ops::DW_OP_GNU_push_tls_address:
           if ( !pl->push_tls() )
             pl->locs.push_back({ tls_index, 0, value});
          break;
        case Dwarf32::dwarf_ops::DW_OP_neg:
           if ( pl )
             pl->push_exp(fneg);
          break;
        case Dwarf32::dwarf_ops::DW_OP_not:
           if ( pl )
             pl->push_exp(fnot);
          break;
        case Dwarf32::dwarf_ops::DW_OP_abs:
           if ( pl )
             pl->push_exp(fabs);
          break;
        case Dwarf32::dwarf_ops::DW_OP_and:
           if ( pl )
             pl->push_exp(fand);
          break;
        case Dwarf32::dwarf_ops::DW_OP_minus:
           if ( pl )
             pl->push_exp(fminus);
          break;
        case Dwarf32::dwarf_ops::DW_OP_or:
           if ( pl )
             pl->push_exp(f_or);
          break;
        case Dwarf32::dwarf_ops::DW_OP_plus:
           if ( pl )
             pl->push_exp(fplus);
          break;
        case Dwarf32::dwarf_ops::DW_OP_shl:
           if ( pl )
             pl->push_exp(fshl);
          break;
        case Dwarf32::dwarf_ops::DW_OP_shr:
           if ( pl )
             pl->push_exp(fshr);
          break;
        case Dwarf32::dwarf_ops::DW_OP_shra:
           if ( pl )
             pl->push_exp(fshra);
          break;
        case Dwarf32::dwarf_ops::DW_OP_xor:
           if ( pl )
             pl->push_exp(fxor);
          break;
        case Dwarf32::dwarf_ops::DW_OP_mul:
           if ( pl )
             pl->push_exp(fmul);
          break;
        case Dwarf32::dwarf_ops::DW_OP_div:
           if ( pl )
             pl->push_exp(fdiv);
          break;
        case Dwarf32::dwarf_ops::DW_OP_mod:
           if ( pl )
             pl->push_exp(fmod);
          break;
        case Dwarf32::dwarf_ops::DW_OP_piece:
          v64 = ElfFile::ULEB128(data, bytes_available);
          pl->locs.push_back({ fpiece, (unsigned int)v64, 0 } );
         break;
        case Dwarf32::dwarf_ops::DW_OP_reg0:
        case Dwarf32::dwarf_ops::DW_OP_reg1:
        case Dwarf32::dwarf_ops::DW_OP_reg2:
        case Dwarf32::dwarf_ops::DW_OP_reg3:
        case Dwarf32::dwarf_ops::DW_OP_reg4:
        case Dwarf32::dwarf_ops::DW_OP_reg5:
        case Dwarf32::dwarf_ops::DW_OP_reg6:
        case Dwarf32::dwarf_ops::DW_OP_reg7:
        case Dwarf32::dwarf_ops::DW_OP_reg8:
        case Dwarf32::dwarf_ops::DW_OP_reg9:
        case Dwarf32::dwarf_ops::DW_OP_reg10:
        case Dwarf32::dwarf_ops::DW_OP_reg11:
        case Dwarf32::dwarf_ops::DW_OP_reg12:
        case Dwarf32::dwarf_ops::DW_OP_reg13:
        case Dwarf32::dwarf_ops::DW_OP_reg14:
        case Dwarf32::dwarf_ops::DW_OP_reg15:
        case Dwarf32::dwarf_ops::DW_OP_reg16:
        case Dwarf32::dwarf_ops::DW_OP_reg17:
        case Dwarf32::dwarf_ops::DW_OP_reg18:
        case Dwarf32::dwarf_ops::DW_OP_reg19:
        case Dwarf32::dwarf_ops::DW_OP_reg20:
        case Dwarf32::dwarf_ops::DW_OP_reg21:
        case Dwarf32::dwarf_ops::DW_OP_reg22:
        case Dwarf32::dwarf_ops::DW_OP_reg23:
        case Dwarf32::dwarf_ops::DW_OP_reg24:
        case Dwarf32::dwarf_ops::DW_OP_reg25:
        case Dwarf32::dwarf_ops::DW_OP_reg26:
        case Dwarf32::dwarf_ops::DW_OP_reg27:
        case Dwarf32::dwarf_ops::DW_OP_reg28:
        case Dwarf32::dwarf_ops::DW_OP_reg29:
        case Dwarf32::dwarf_ops::DW_OP_reg30:
        case Dwarf32::dwarf_ops::DW_OP_reg31:
           pl->locs.push_back({ reg, op - Dwarf32::dwarf_ops::DW_OP_reg0, 0});
         break;
        case Dwarf32::dwarf_ops::DW_OP_regx:
           pl->locs.push_back({ reg, (unsigned int)ElfFile::ULEB128(data, bytes_available), 0});
         break;
        case Dwarf32::dwarf_ops::DW_OP_breg0:
        case Dwarf32::dwarf_ops::DW_OP_breg1:
        case Dwarf32::dwarf_ops::DW_OP_breg2:
        case Dwarf32::dwarf_ops::DW_OP_breg3:
        case Dwarf32::dwarf_ops::DW_OP_breg4:
        case Dwarf32::dwarf_ops::DW_OP_breg5:
        case Dwarf32::dwarf_ops::DW_OP_breg6:
        case Dwarf32::dwarf_ops::DW_OP_breg7:
        case Dwarf32::dwarf_ops::DW_OP_breg8:
        case Dwarf32::dwarf_ops::DW_OP_breg9:
        case Dwarf32::dwarf_ops::DW_OP_breg10:
        case Dwarf32::dwarf_ops::DW_OP_breg11:
        case Dwarf32::dwarf_ops::DW_OP_breg12:
        case Dwarf32::dwarf_ops::DW_OP_breg13:
        case Dwarf32::dwarf_ops::DW_OP_breg14:
        case Dwarf32::dwarf_ops::DW_OP_breg15:
        case Dwarf32::dwarf_ops::DW_OP_breg16:
        case Dwarf32::dwarf_ops::DW_OP_breg17:
        case Dwarf32::dwarf_ops::DW_OP_breg18:
        case Dwarf32::dwarf_ops::DW_OP_breg19:
        case Dwarf32::dwarf_ops::DW_OP_breg20:
        case Dwarf32::dwarf_ops::DW_OP_breg21:
        case Dwarf32::dwarf_ops::DW_OP_breg22:
        case Dwarf32::dwarf_ops::DW_OP_breg23:
        case Dwarf32::dwarf_ops::DW_OP_breg24:
        case Dwarf32::dwarf_ops::DW_OP_breg25:
        case Dwarf32::dwarf_ops::DW_OP_breg26:
        case Dwarf32::dwarf_ops::DW_OP_breg27:
        case Dwarf32::dwarf_ops::DW_OP_breg28:
        case Dwarf32::dwarf_ops::DW_OP_breg29:
        case Dwarf32::dwarf_ops::DW_OP_breg30:
        case Dwarf32::dwarf_ops::DW_OP_breg31:
           pl->locs.push_back({ breg, op - Dwarf32::dwarf_ops::DW_OP_breg0, (int)ElfFile::SLEB128(data, bytes_available)});
         break;
        case Dwarf32::dwarf_ops::DW_OP_bregx: {
          auto reg = ElfFile::ULEB128(data, bytes_available);
          int off = ElfFile::SLEB128(data, bytes_available);
          pl->locs.push_back({ breg, (unsigned int)reg, off });
          break;
        }
        case Dwarf32::dwarf_ops::DW_OP_fbreg:
           pl->locs.push_back({ fbreg, 0, (int)ElfFile::SLEB128(data, bytes_available)});
         break;
        case Dwarf32::dwarf_ops::DW_OP_stack_value:
           if ( pl )
             pl->push_exp(fstack);
          break;
        case Dwarf32::dwarf_ops::DW_OP_implicit_value:
           // uleb encoded length
           v64 = ElfFile::ULEB128(data, bytes_available);
           switch(v64)
           {
             case 1:
                pl->locs.push_back({ imp_value, *reinterpret_cast<const uint8_t*>(data), 0});
               break;
             case 2:
                pl->locs.push_back({ imp_value, *reinterpret_cast<const uint16_t*>(data), 0});
               break;
             case 4:
                pl->locs.push_back({ imp_value, *reinterpret_cast<const uint32_t*>(data), 0});
               break;
             case 8:
                pl->locs.push_back({ imp_value, (unsigned int)*reinterpret_cast<const uint64_t*>(data), 0});
               break;
             default:
               fprintf(stderr, "DecodeAddrLocation: unknown implicit_value size %lX at %lX\n", v64, doff); 
           }
           bytes_available -= v64;
           data += v64; 
          break;
      default:
        fprintf(stderr, "DecodeAddrLocation: unknown op %X at %lX\n", op, doff);
        return 0;
    }
  }
  return 0;
}

// seems that vtable_elem_location usually encoded not as simple DW_FORM_xxx (so we can`t use FormDataValue here)
// but as DW_OP_constu inside block
// so I ripped necessary part of code from binutils/dwarf.c function decode_location_expression in this method
uint64_t ElfFile::DecodeLocation(Dwarf32::Form form, const unsigned char* info, size_t bytes_available) 
{
  uint64_t len = 0;
  switch(form)
  {
    case Dwarf32::Form::DW_FORM_block1:
      len = *reinterpret_cast<const uint8_t*>(info);
      info++;
      bytes_available--;
      break;
    case Dwarf32::Form::DW_FORM_block2:
      len = *reinterpret_cast<const uint16_t*>(info);
      info += 2;
      bytes_available -= 2;
      break;
    case Dwarf32::Form::DW_FORM_block4:
      len = *reinterpret_cast<const uint32_t*>(info);
      info += 4;
      bytes_available -= 4;
      break;
    case Dwarf32::Form::DW_FORM_exprloc:
      len = ElfFile::ULEB128(info, bytes_available);
      break;
    default:
      return FormDataValue(form, info, bytes_available);
  }
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
        fprintf(stderr, "DecodeLocation: unknown op %X at %lX\n", op, info - debug_info_);
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
     /* if ( 1 == value && form == Dwarf32::Form::DW_FORM_flag )
        fprintf(stderr, "flag1 at %lX\n", info - debug_info_); */
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
    // addrx
    case Dwarf32::Form::DW_FORM_addrx1:
      value = *reinterpret_cast<const uint8_t*>(info);
      info++;
      bytes_available--;
      return get_indexed_addr(value, address_size_);  
    case Dwarf32::Form::DW_FORM_addrx2:
      value = *reinterpret_cast<const uint16_t*>(info);
      info += 2;
      bytes_available -= 2;
      return get_indexed_addr(value, address_size_);
    case Dwarf32::Form::DW_FORM_addrx3:
      value = read_x3(info, bytes_available);
      return get_indexed_addr(value, address_size_);  
    case Dwarf32::Form::DW_FORM_addrx4:
      value = *reinterpret_cast<const uint32_t*>(info);
      info += 4;
      bytes_available -= 4;
      return get_indexed_addr(value, address_size_);  
    case Dwarf32::Form::DW_FORM_addrx:
      value = ElfFile::ULEB128(info, bytes_available);
      return get_indexed_addr(value, address_size_);
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
    case Dwarf32::Form::DW_FORM_implicit_const:
       value = m_implicit_const;
      break;
    default:
      fprintf(stderr, "ERR: Unexpected form data 0x%x at %lX\n", form, info - debug_info_);
      exit(1);
  }

  return value;
};

uint64_t ElfFile::get_indexed_addr(uint64_t pos, int size)
{
  if ( !debug_addr_size_ || !addr_base )
    return 0;
  pos *= address_size_;
  if ( pos + size + addr_base > debug_addr_size_ )
    return 0;
  switch(size)
  {
    case 1: { const uint8_t *b = (const uint8_t *)(debug_addr_ + addr_base + pos);
      return *b;
    }
    case 2: { const uint16_t *b = (const uint16_t *)(debug_addr_ + addr_base + pos);
      return *b;
    }
    case 4: { const uint32_t *b = (const uint32_t *)(debug_addr_ + addr_base + pos);
      return *b;
    }
    case 8: { const uint64_t *b = (const uint64_t *)(debug_addr_ + addr_base + pos);
      return *b;
    }
  }
  return 0;
}

const char* ElfFile::get_indexed_str(uint32_t str_pos)
{
  if ( !debug_str_offsets_size_ || !offsets_base )
    return nullptr;
  uint64_t index_offset = str_pos * 4;
  if ( index_offset + offsets_base > debug_str_offsets_size_ )
    return nullptr;
  uint32_t str_offset = *(uint32_t *)(debug_str_offsets_ + index_offset + offsets_base);
  return (const char*)&tree_builder->debug_str_[str_offset];
}

const char* ElfFile::check_strp(uint32_t str_pos)
{
  if ( !debug_line_str_ )
    return nullptr;
  if ( (size_t)str_pos > debug_line_str_size_ )
  {
    fprintf(stderr, "strp %X is not in debug_line_str section size %lx\n", str_pos, debug_line_str_size_);
    fflush(stderr);
    return nullptr;
  } else
    return (const char*)debug_line_str_ + str_pos;
}

const char* ElfFile::check_strx4(uint32_t str_pos)
{
  if ( (size_t)str_pos > debug_str_offsets_size_ )
  {
    fprintf(stderr, "stringx4 %X is not in str_offsets section size %lx\n", str_pos, debug_str_offsets_size_);
    fflush(stderr);
    return nullptr;
  } else
    return get_indexed_str(str_pos);
}

const char* ElfFile::check_strx2(uint32_t str_pos)
{
  if ( (size_t)str_pos > debug_str_offsets_size_ )
  {
    fprintf(stderr, "stringx2 %X is not in str_offsets section size %lx\n", str_pos, debug_str_offsets_size_);
    fflush(stderr);
    return nullptr;
  } else
    return get_indexed_str(str_pos);
}

const char* ElfFile::check_strx3(uint32_t str_pos)
{
  if ( (size_t)str_pos > debug_str_offsets_size_ )
  {
    fprintf(stderr, "stringx3 %X is not in str_offsets section size %lx\n", str_pos, debug_str_offsets_size_);
    fflush(stderr);
    return nullptr;
  } else
    return get_indexed_str(str_pos);
}

const char* ElfFile::check_strx1(uint32_t str_pos)
{
  if ( (size_t)str_pos > debug_str_offsets_size_ )
  {
    fprintf(stderr, "stringx1 %X is not in str_offsets section size %lx\n", str_pos, debug_str_offsets_size_);
    fflush(stderr);
    return nullptr;
  } else
    return get_indexed_str(str_pos);
}

const char* ElfFile::FormStringValue(Dwarf32::Form form, const unsigned char* &info, 
                                                      size_t& bytes_available) {
  const char* str = nullptr;
  const unsigned char *s = info;
  uint32_t str_pos = 0;

  switch(form) {
    case Dwarf32::Form::DW_FORM_strx4:
      str_pos = *reinterpret_cast<const uint32_t*>(info);
      info += 4;
      bytes_available -= 4;
      if ( curr_asgn )
      {
        push2dlist(&ElfFile::check_strx4, str_pos);
        return nullptr;
      }
      return check_strx4(str_pos);
    case Dwarf32::Form::DW_FORM_strx3:
      str_pos = read_x3(info, bytes_available);
      if ( curr_asgn )
      {
        push2dlist(&ElfFile::check_strx3, str_pos);
        return nullptr;
      }
      return check_strx3(str_pos);
    case Dwarf32::Form::DW_FORM_strx2:
      str_pos = *reinterpret_cast<const uint16_t*>(info);
      info += 2;
      bytes_available -= 2;
      if ( curr_asgn )
      {
        push2dlist(&ElfFile::check_strx2, str_pos);
        return nullptr;
      }
      return check_strx2(str_pos);
    case Dwarf32::Form::DW_FORM_strx1:
      str_pos = *reinterpret_cast<const uint8_t*>(info);
      info += 1;
      bytes_available -= 1;
      if ( curr_asgn )
      {
        push2dlist(&ElfFile::check_strx1, str_pos);
        return nullptr;
      }
      return check_strx1(str_pos);
    case Dwarf32::Form::DW_FORM_strp:
      str_pos = *reinterpret_cast<const uint32_t*>(info);
      info += sizeof(str_pos);
      bytes_available -= sizeof(str_pos);
      if ( str_pos > tree_builder->debug_str_size_ )
      {
        fprintf(stderr, "string %X is not in string section at %lX\n", str_pos, s - debug_info_);
        fflush(stderr);
      } else
        str = (const char*)&tree_builder->debug_str_[str_pos];
      break;
    case Dwarf32::Form::DW_FORM_string:
      str = reinterpret_cast<const char*>(info);
      // fprintf(stderr, "name %p at %lX %s\n", str, info - debug_info_, str);
      while (*info) {
          info++;
          bytes_available--;
      }
      info++;
      bytes_available--;
      break;
    default:
      fprintf(stderr, "ERR: Unexpected form string 0x%x at %lX\n", form, info - debug_info_);
      break;
  }

  return str;
};

// load tags from .debug_abbrev section
bool ElfFile::LoadAbbrevTags(uint32_t abbrev_offset) {
  if (!debug_info_ || !debug_abbrev_)
    return false;
  if ( abbrev_offset >= debug_abbrev_size_ )
  {
    fprintf(stderr, "abbrev_offset %X is out of section\n", abbrev_offset);
    return false;
  }
  compilation_unit_.clear();

  const unsigned char* abbrev = reinterpret_cast<const unsigned char*>(debug_abbrev_ + abbrev_offset);
  size_t abbrev_bytes = debug_abbrev_size_ - abbrev_offset;

  // For all compilation tags
  while (abbrev_bytes > 0 && abbrev[0]) {
    struct TagSection section;
    section.number = ElfFile::ULEB128(abbrev, abbrev_bytes);
    DBG_PRINTF(".abbrev+%lx\t Tag Number %d\n", abbrev - debug_abbrev_, section.number);
    section.type = static_cast<Dwarf32::Tag>(ElfFile::ULEB128(abbrev, abbrev_bytes));
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
      ElfFile::ULEB128(abbrev, abbrev_bytes);
      unsigned long form = ElfFile::ULEB128(abbrev, abbrev_bytes);
      if (form == Dwarf32::Form::DW_FORM_implicit_const)
        ElfFile::SLEB128(abbrev, abbrev_bytes);
    }
    abbrev += 2;
    abbrev_bytes -= 2;
  }

  return true;
}

#define CASE_REGISTER_NEW_TAG(tag_type, element_type)                         \
  case Dwarf32::Tag::tag_type:                                                \
    tree_builder->AddElement(TreeBuilder::ElementType::element_type, m_tag_id, m_level); \
    return true; \
    break;

bool ElfFile::RegisterNewTag(Dwarf32::Tag tag) {
   bool ell = false;
  switch (tag) {
    CASE_REGISTER_NEW_TAG(DW_TAG_array_type, array_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_class_type, class_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_interface_type, interface_type)
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
    CASE_REGISTER_NEW_TAG(DW_TAG_atomic_type, atomic_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_immutable_type, immutable_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_reference_type, reference_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_rvalue_reference_type, rvalue_ref_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_subroutine_type, subroutine_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_ptr_to_member_type, ptr2member)
    CASE_REGISTER_NEW_TAG(DW_TAG_unspecified_type, unspec_type)
    CASE_REGISTER_NEW_TAG(DW_TAG_variant_part, variant_type)
    case Dwarf32::Tag::DW_TAG_variant:
      return tree_builder->AddVariant();
    case Dwarf32::Tag::DW_TAG_lexical_block:
      if ( g_opt_L && m_section->has_children )
      {
        tree_builder->AddElement(TreeBuilder::ElementType::lexical_block, m_tag_id, m_level);
        return true;
      }
      break;
    case Dwarf32::Tag::DW_TAG_variable:
      if ( g_opt_V )
      {
        tree_builder->AddElement(TreeBuilder::ElementType::var_type, m_tag_id, m_level);
        return true;
      }
      break;
    case Dwarf32::Tag::DW_TAG_namespace:
      if ( m_section->has_children )
      {
        tree_builder->AddElement(TreeBuilder::ElementType::ns_start, m_tag_id, m_level);
        return true;
      }
      break;      
    case Dwarf32::Tag::DW_TAG_subprogram:
      if ( g_opt_f )
      {
        tree_builder->AddElement(TreeBuilder::ElementType::subroutine, m_tag_id, m_level);
        return true;
      }
      break;
    case Dwarf32::Tag::DW_TAG_unspecified_parameters:
      ell = true;
    case Dwarf32::Tag::DW_TAG_formal_parameter:
      if ( g_opt_d )
        fprintf(g_outf, "param %lX regged %d\n", m_tag_id, m_regged);
      if ( m_regged )
        return tree_builder->AddFormalParam(m_tag_id, m_level, ell);
      break;
    default: ;
  }
  tree_builder->AddNone();
  return false;
}

template <typename T>
bool ElfFile::ProcessFlags(Dwarf32::Form form, const unsigned char* &info, size_t& info_bytes, T ptr)
{
  if ( !m_regged )
    return false;
  if ( form == Dwarf32::Form::DW_FORM_flag_present )
    (tree_builder->*ptr)();
  else {
    auto v = FormDataValue(form, info, info_bytes);
    if ( v )
      (tree_builder->*ptr)();
  }
  return true;
}

bool ElfFile::LogDwarfInfo(Dwarf32::Attribute attribute, 
        Dwarf32::Form form, const unsigned char* &info, 
        size_t& info_bytes, const void* unit_base) 
{           
  switch((unsigned int)attribute) {
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
      return false;
    // param direction - see http://redplait.blogspot.com/2023/04/custom-attributes-in-gcc-and-dwarf.html
    case 0x28ff:
      if ( m_regged )
      {
         auto c = FormDataValue(form, info, info_bytes);
         if ( c )
           tree_builder->SetParamDirection((unsigned char)c);
         return true;
      }
      return false;
     break;
    // go extended attributes
    // see https://github.com/golang/go/blob/master/src/cmd/internal/dwarf/dwarf.go#L321
    // DW_AT_go_kind - DW_FORM_data1
    case 0x2900:
      if ( m_regged && tree_builder->is_go() )
      {
        int kind = FormDataValue(form, info, info_bytes);
        if ( kind )
          tree_builder->SetGoKind(m_tag_id, kind);
        return true;
      }
      return false;
      // DW_AT_go_package_name
    case 0x2905:
      if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit && tree_builder->is_go() )
      {
        if ( !offsets_base && debug_str_offsets_ )
          curr_asgn = &ElfFile::asgn_package;
        asgn_package(FormStringValue(form, info, info_bytes));
        return true;
      }
      return false;
      // DW_AT_go_key - DW_FORM_ref_addr
    case 0x2901:
      if ( m_regged && tree_builder->is_go() )
      {
        uint64_t addr = FormDataValue(form, info, info_bytes);
        if (form != Dwarf32::Form::DW_FORM_ref_addr) {
          // The offset is relative to the current compilation unit, we make it
          // absolute
          addr += reinterpret_cast<const unsigned char*>(unit_base) - debug_info_;
        }
        tree_builder->SetGoKey(m_tag_id, addr);  
        return true;
      }
      return false;
      // DW_AT_go_elem - DW_FORM_ref_addr
    case 0x2902:
      if ( m_regged && tree_builder->is_go() )
      {
        uint64_t addr = FormDataValue(form, info, info_bytes);
        if (form != Dwarf32::Form::DW_FORM_ref_addr) {
          // The offset is relative to the current compilation unit, we make it
          // absolute
          addr += reinterpret_cast<const unsigned char*>(unit_base) - debug_info_;
        }
        tree_builder->SetGoElem(m_tag_id, addr);  
        return true;
      }
      return false;
      // DW_AT_go_runtime_type - DW_FORM_addr
    case 0x2904:
      if ( m_regged && tree_builder->is_go() )
      {
        uint64_t addr = FormDataValue(form, info, info_bytes);
        if ( addr )
          tree_builder->SetGoRType(m_tag_id, (const void *)addr);  
        return true;
      }
      return false;
      // DW_AT_go_dict_index - DW_FORM_udata
    case 0x2906:
      if ( m_regged && tree_builder->is_go() )
      {
        uint64_t addr = FormDataValue(form, info, info_bytes);
        if ( addr )
          tree_builder->SetGoDictIndex(m_tag_id, addr);  
        return true;
      }
      return false;
    case Dwarf32::Attribute::DW_AT_loclists_base:
     if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit )
     {
       loclist_base = FormDataValue(form, info, info_bytes);
        // check that it located somewhere inside .debug_loclists section
        if ( (size_t)loclist_base > debug_loclists_size_ )
        {
          fprintf(stderr, "bad DW_AT_loclist_base %lx, size of .debug_loclists %lx\n", loclist_base, debug_loclists_size_);
          loclist_base = 0;
        }
        return true;
     }
     break;
    case Dwarf32::Attribute::DW_AT_addr_base:
      if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit )
      {
        addr_base = FormDataValue(form, info, info_bytes);
        // check that it located somewhere inside .debug_addr section
        if ( (size_t)addr_base > debug_addr_size_ )
        {
          fprintf(stderr, "bad DW_AT_addr_base %lx, size of .debug_addr %lx\n", addr_base, debug_addr_size_);
          addr_base = 0;
        } else
          if ( tree_builder->cu.need_base_addr_idx )
            tree_builder->cu.cu_base_addr = get_indexed_addr(tree_builder->cu.cu_base_addr_idx, address_size_);
        return true;
      }
      return false;
    case Dwarf32::Attribute::DW_AT_str_offsets_base:
      if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit )
      {
        offsets_base = FormDataValue(form, info, info_bytes);
        // check that it located somewhere inside .debug_str_offsets section
        if ( (size_t)offsets_base > debug_str_offsets_size_ )
        {
          fprintf(stderr, "bad DW_AT_str_offsets_base %lx, size of .debug_str_offsets %lx\n", offsets_base, debug_str_offsets_size_);
          offsets_base = 0;
        }
        apply_dlist();
        if ( g_opt_F && debug_line_ && m_li.m_ptr )
          read_delayed_lines();
        return true;
      }
      return false;
    // Name
    case Dwarf32::Attribute::DW_AT_producer:
      if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit )
      {
        if ( !offsets_base && debug_str_offsets_ )
          curr_asgn = &ElfFile::asgn_producer;
        asgn_producer(FormStringValue(form, info, info_bytes));
        return true;  
      }
      break;
    case Dwarf32::Attribute::DW_AT_comp_dir:
      if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit )
      {
        if ( !offsets_base && debug_str_offsets_ )
          curr_asgn = &ElfFile::asgn_comp_dir;
        asgn_comp_dir(FormStringValue(form, info, info_bytes));
        return true;  
      }
      break;
    case Dwarf32::Attribute::DW_AT_language:
      if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit )
      {
        tree_builder->cu.cu_lang = (int)FormDataValue(form, info, info_bytes);
        return true;  
      }
      break;  
    case Dwarf32::Attribute::DW_AT_name:
      if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit )
      {
        if ( !offsets_base && debug_str_offsets_ )
          curr_asgn = &ElfFile::asgn_cu_name;
        asgn_cu_name(FormStringValue(form, info, info_bytes));
        return true;  
      }
    case Dwarf32::Attribute::DW_AT_MIPS_linkage_name:
    case Dwarf32::Attribute::DW_AT_linkage_name: {
      const char* name = FormStringValue(form, info, info_bytes);
      if ( tree_builder->check_dumped_type(name) )
      {
        m_regged = false;
        return true;
      }
      if ( Dwarf32::Attribute::DW_AT_name != attribute )
        tree_builder->SetLinkageName(name);
      else
        tree_builder->SetElementName(name, info - debug_info_);
      return true;
    }
    case Dwarf32::Attribute::DW_AT_decl_file:
      if ( g_opt_F && m_regged && tree_builder->need_filename() )
      {
        auto fid = FormDataValue(form, info, info_bytes);
        if ( fid )
        {
          std::string fname;
          const char *f;
          if ( get_filename(fid, fname, f) )
            tree_builder->SetFilename(fname, f);
        }
        return true;
      }
      return false;
    case Dwarf32::Attribute::DW_AT_const_expr:
      return ProcessFlags(form, info, info_bytes, &TreeBuilder::SetConstExpr);
    case Dwarf32::Attribute::DW_AT_explicit:
      return ProcessFlags(form, info, info_bytes, &TreeBuilder::SetExplicit);
    case Dwarf32::Attribute::DW_AT_is_optional:
      return ProcessFlags(form, info, info_bytes, &TreeBuilder::SetOptionalParam);
    case Dwarf32::Attribute::DW_AT_variable_parameter: {
      uint64_t v = FormDataValue(form, info, info_bytes);
      if ( m_regged && v )
        tree_builder->SetVarParam(v ? true : false);
      return true;
      }
      break;
    case Dwarf32::Attribute::DW_AT_defaulted:
      return ProcessFlags(form, info, info_bytes, &TreeBuilder::SetDefaulted);
    case Dwarf32::Attribute::DW_AT_inline: {
      uint64_t v = FormDataValue(form, info, info_bytes);
      if ( m_regged && v )
        tree_builder->SetInlined((int)v);
      return true;
    }
    case Dwarf32::Attribute::DW_AT_discr: {
      uint64_t addr = FormDataValue(form, info, info_bytes);
      if ( m_regged )
      {
        if (form != Dwarf32::Form::DW_FORM_ref_addr) {
          // The offset is relative to the current compilation unit, we make it
          // absolute
          addr += reinterpret_cast<const unsigned char*>(unit_base) - debug_info_;
        }
        // fprintf(stderr, "discr %lX form %d at %lX\n", addr, form, info - debug_info_);  
        tree_builder->SetDiscr(addr);
      }
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
    case Dwarf32::Attribute::DW_AT_low_pc:
      // printf("low_pc form %X\n", form);
      if ( m_section->type == Dwarf32::Tag::DW_TAG_compile_unit )
      {
        if ( form == Dwarf32::Form::DW_FORM_addrx )
        {
          if ( addr_base )
          {
            tree_builder->cu.cu_base_addr = FormDataValue(form, info, info_bytes);
          } else {
            tree_builder->cu.cu_base_addr_idx = ElfFile::ULEB128(info, info_bytes);
            tree_builder->cu.need_base_addr_idx = true;
          }
        }
        else
          tree_builder->cu.cu_base_addr = FormDataValue(form, info, info_bytes);
      // fprintf(stderr, "low_pc %lX on compile_unit form %X\n", tree_builder->cu.cu_base_addr, form);
        return true;
      }
      if ( m_regged )
      {
        uint64_t addr = FormDataValue(form, info, info_bytes);
        if ( addr )
          tree_builder->SetAddr(addr);
        return true;
      }
      return false;
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

    case Dwarf32::Attribute::DW_AT_encoding:
     if ( m_regged )
     {
       unsigned char ate = (unsigned char)FormDataValue(form, info, info_bytes);
       tree_builder->SetAte(ate);
       return true;
     }
     return false;
    case Dwarf32::Attribute::DW_AT_address_class: {
      int ac = (int)FormDataValue(form, info, info_bytes);
      tree_builder->SetAddressClass(ac);
      return true;
    }
    // Offset
    case Dwarf32::Attribute::DW_AT_data_member_location: {
      uint64_t offset = FormDataValue(form, info, info_bytes);
      tree_builder->SetElementOffset(offset);
      return true;
    }

    case Dwarf32::Attribute::DW_AT_location:
      if ( !m_regged )
        return false;
      else {
        param_loc loc;
        uint64_t offset = DecodeAddrLocation(form, info, info_bytes, &loc, debug_info_);
        if ( tree_builder->is_formal_param() )
        {
          if ( !loc.empty() )
            tree_builder->SetLocation(&loc);
        } else if ( loc.is_tls() )
          tree_builder->SetTlsIndex(&loc);
        else if ( g_opt_x && !loc.empty() && tree_builder->is_local_var() )
          tree_builder->SetLocVarLocation(&loc);
        else if ( offset )
          tree_builder->SetAddr(offset);
        return false;
      }

    // Type
    case Dwarf32::Attribute::DW_AT_type: {
      uint64_t id = FormDataValue(form, info, info_bytes);
      if (form != Dwarf32::Form::DW_FORM_ref_addr) {
        // The offset is relative to the current compilation unit, we make it
        // absolute
        id += reinterpret_cast<const unsigned char*>(unit_base) - debug_info_;
      }
      // fprintf(stderr, "type %lX form %d at %lX\n", id, form, info - debug_info_);
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
    case Dwarf32::Attribute::DW_AT_const_value:
      if ( !m_regged )
        return false;
      if ( tree_builder->is_enum() )
      {
        uint64_t val = FormDataValue(form, info, info_bytes);
        tree_builder->SetConstValue(val);
        return true;
      }
      if ( m_section->type == Dwarf32::Tag::DW_TAG_variable )
      {
        if ( form == Dwarf32::Form::DW_FORM_block ||
             form == Dwarf32::Form::DW_FORM_block1 ||
             form == Dwarf32::Form::DW_FORM_block2 ||
             form == Dwarf32::Form::DW_FORM_block4
           )
          return false;
        uint64_t val = FormDataValue(form, info, info_bytes);
        tree_builder->SetVarConstValue(val);
        return true;
      }
      return false;
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
      return ProcessFlags(form, info, info_bytes, &TreeBuilder::SetNoReturn);
     // declaration
    case Dwarf32::Attribute::DW_AT_declaration:
      return ProcessFlags(form, info, info_bytes, &TreeBuilder::SetDeclaration);  
    case Dwarf32::Attribute::DW_AT_artificial:
      return ProcessFlags(form, info, info_bytes, &TreeBuilder::SetArtiticial);
    default:
      return false;
  }

  return false;
}

bool ElfFile::GetAllClasses() 
{
  const unsigned char* info = reinterpret_cast<const unsigned char*>(debug_info_);
  size_t info_bytes = debug_info_size_;
  m_curr_lines = debug_line_;

  while (info_bytes > 0) {
    // process previous compilation unit
    tree_builder->ProcessUnit();
    // Load the compilation unit information
    const unsigned char* cu_start = info;
    cu_base = cu_start - debug_info_;
    const Dwarf32::CompilationUnitHdr* unit_hdr =
        reinterpret_cast<const Dwarf32::CompilationUnitHdr*>(info);
    const unsigned char* info_end;
    uint32_t abbrev_offset = unit_hdr->debug_abbrev_offset;
    if ( unit_hdr->version < 5 )
    {
      address_size_ = unit_hdr->address_size;
      DBG_PRINTF("unit_length         = 0x%x\n", unit_hdr->unit_length);
      DBG_PRINTF("version             = %d\n", unit_hdr->version);
      DBG_PRINTF("debug_abbrev_offset = 0x%x\n", unit_hdr->debug_abbrev_offset);
      DBG_PRINTF("address_size        = %d\n", unit_hdr->address_size);
      info_end = info + unit_hdr->unit_length + sizeof(uint32_t);
      info += sizeof(Dwarf32::CompilationUnitHdr);
      info_bytes -= sizeof(Dwarf32::CompilationUnitHdr);
    } else {
      const Dwarf32::CompilationUnitHdr5* unit_hdr5 =
        reinterpret_cast<const Dwarf32::CompilationUnitHdr5*>(info);
      address_size_ = unit_hdr5->address_size;
      DBG_PRINTF("unit_length         = 0x%x\n", unit_hdr5->unit_length);
      DBG_PRINTF("version             = %d\n", unit_hdr5->version);
      DBG_PRINTF("unit_type           = %d\n", unit_hdr5->unit_type);
      DBG_PRINTF("address_size        = %d\n", unit_hdr5->address_size);
      abbrev_offset = unit_hdr5->debug_abbrev_offset;
      DBG_PRINTF("debug_abbrev_offset = 0x%x\n", abbrev_offset);
      info_end = info + unit_hdr5->unit_length + sizeof(uint32_t);
      info += sizeof(Dwarf32::CompilationUnitHdr5);
      info_bytes -= sizeof(Dwarf32::CompilationUnitHdr5);
      if ( unit_hdr5->unit_type == Dwarf32::unit_type::DW_UT_type )
      {
        DBG_PRINTF("signature        = %lX\n", *(const uint64_t *)info);
        info += 8 + address_size_;
        info_bytes -= 8 + address_size_;
      }
      if ( unit_hdr5->unit_type == Dwarf32::unit_type::DW_UT_split_compile ||
           unit_hdr5->unit_type == Dwarf32::unit_type::DW_UT_skeleton
         )
      {
        info += 8;
        info_bytes -= 8;
      }
      DBG_PRINTF("hdr5: %lx\n", info-debug_info_);
    }
    // read debug lines
    if ( !read_debug_lines() )
      free_section(debug_line_, free_line);

    if (!LoadAbbrevTags(abbrev_offset)) {
      fprintf(stderr, "ERR: Can't load the compilation, abbrev_offset %X\n", abbrev_offset);
      return false;
    }
    if ( g_opt_d )
      fprintf(g_outf, "reset level\n");
    m_level = 0;

    // reset bases for new compilation unit
    offsets_base = 0;
    addr_base = 0;
    loclist_base = 0;
    // For all compilation tags
    while (info < info_end) {
      m_tag_id = info - debug_info_; 
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
      m_regged = RegisterNewTag(m_section->type);
      m_next = 0;

      if ( g_opt_d )
        fprintf(g_outf, "%d GetAllClasses %lx size %lx regged %d\n", m_level, m_tag_id, abbrev_bytes, m_regged);

      // For all attributes
      while (*abbrev) 
      {
        curr_asgn = nullptr;
        Dwarf32::Attribute abbrev_attribute = static_cast<Dwarf32::Attribute>(
            ElfFile::ULEB128(abbrev, abbrev_bytes));
        Dwarf32::Form abbrev_form = 
            static_cast<Dwarf32::Form>(ElfFile::ULEB128(abbrev, abbrev_bytes));
        if ( abbrev_form == Dwarf32::Form::DW_FORM_implicit_const )
          m_implicit_const = ElfFile::SLEB128(abbrev, abbrev_bytes);

        if ( g_opt_d )
          fprintf(g_outf,".info+%lx\t %02x %02x\n", info-debug_info_, 
                                                abbrev_attribute, abbrev_form);
        bool logged = LogDwarfInfo(abbrev_attribute, abbrev_form, info, info_bytes, cu_start);
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


