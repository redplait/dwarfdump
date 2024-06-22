#include "ElfFile.h"

bool ElfFile::try_apply_debug_relocs()
{
 // fill map with loaded debug sections
 // .debug_abbrev & .debug_str should be ignored 
 // see last bool field in dwaft.c table debug_displays
 std::map<Elf_Half, dwarf_section *> rmaps;
 if ( !debug_info_.empty() )
   rmaps[debug_info_.idx] = &debug_info_;
 if ( !debug_loclists_.empty() )
   rmaps[debug_loclists_.idx] = &debug_loclists_;
 if ( !debug_addr_.empty() )
   rmaps[debug_addr_.idx] = &debug_addr_;
 if ( !debug_frame_.empty() )
   rmaps[debug_frame_.idx] = &debug_frame_;
 if ( !debug_ranges_.empty() )
   rmaps[debug_ranges_.idx] = &debug_ranges_;
 if ( !debug_rnglists_.empty() )
   rmaps[debug_rnglists_.idx] = &debug_rnglists_;
 if ( !debug_loc_.empty() )
   rmaps[debug_loc_.idx] = &debug_loc_;
 if ( !debug_line_.empty() )
   rmaps[debug_line_.idx] = &debug_line_;
 if ( !debug_line_str_.empty() )
   rmaps[debug_line_str_.idx] = &debug_line_str_;
 if ( rmaps.empty() ) return true;
 // ok, enum reloc sections
 std::list<Elf_Half> rs;
 Elf_Half n = reader.sections.size();
 for ( Elf_Half i = 0; i < n; i++) {
   section *s = reader.sections[i];
   if ( s->get_type() == SHT_REL || s->get_type() == SHT_RELA )
   {
     auto inf = s->get_info();
     auto si = rmaps.find(inf);
     if ( si == rmaps.end() ) continue;
     if ( g_opt_v )
       printf("section(%d) %s has relocs\n", inf, reader.sections[inf]->get_name().c_str());
     rs.push_back(i);
   }
 }
 if ( rs.empty() ) return true;
 return false;
}