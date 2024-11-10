package Elf::Reader;

use 5.030000;
use strict;
use warnings;

require Exporter;
use AutoLoader qw(AUTOLOAD);

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Elf::Reader ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

# File type
use constant {
 ET_NONE   => 0,
 ET_REL    => 1,
 ET_EXEC   => 2,
 ET_DYN    => 3,
 ET_CORE   => 4,
 ET_LOOS   => 0xFE00,
 ET_HIOS   => 0xFEFF,
 ET_LOPROC => 0xFF00,
 ET_HIPROC => 0xFFFF,
};

# File class
use constant {
 ELFCLASSNONE => 0,
 ELFCLASS32   => 1,
 ELFCLASS64   => 2,
};

# Encoding
use constant {
 ELFDATANONE => 0,
 ELFDATA2LSB => 1,
 ELFDATA2MSB => 2,
};

# Segment types
use constant {
 PT_NULL         => 0,
 PT_LOAD         => 1,
 PT_DYNAMIC      => 2,
 PT_INTERP       => 3,
 PT_NOTE         => 4,
 PT_SHLIB        => 5,
 PT_PHDR         => 6,
 PT_TLS          => 7,
 PT_LOOS         => 0X60000000,
 PT_GNU_EH_FRAME => 0X6474E550,
 PT_GNU_STACK    => 0X6474E551,
 PT_GNU_RELRO    => 0X6474E552,
 PT_GNU_PROPERTY => 0X6474E553,
 PT_GNU_MBIND_LO => 0X6474E555,
 PT_GNU_MBIND_HI => 0X6474F554,
 PT_PAX_FLAGS    => 0X65041580,
 PT_OPENBSD_RANDOMIZE => 0X65A3DBE6,
 PT_OPENBSD_WXNEEDED  => 0X65A3DBE7,
 PT_OPENBSD_BOOTDATA  => 0X65A41BE6,
 PT_SUNWSTACK         => 0X6FFFFFFB,
 PT_HIOS              => 0X6FFFFFFF,
 PT_LOPROC            => 0X70000000,
 PT_HIPROC            => 0X7FFFFFFF,
};

# segment flags
use constant {
 PF_X        => 1,
 PF_W        => 2,
 PF_R        => 4,
 PF_MASKOS   => 0x0ff00000,
 PF_MASKPROC => 0xf0000000,
};

# Section indexes
use constant {
 SHN_UNDEF     => 0,
 SHN_LORESERVE => 0xFF00,
 SHN_LOPROC    => 0xFF00,
 SHN_HIPROC    => 0xFF1F,
 SHN_LOOS      => 0xFF20,
 SHN_HIOS      => 0xFF3F,
 SHN_ABS       => 0xFFF1,
 SHN_COMMON    => 0xFFF2,
 SHN_XINDEX    => 0xFFFF,
 SHN_HIRESERVE => 0xFFFF,
};

# Section types
use constant {
 SHT_NULL => 0,
 SHT_PROGBITS => 1,
 SHT_SYMTAB => 2,
 SHT_STRTAB => 3,
 SHT_RELA => 4,
 SHT_HASH => 5,
 SHT_DYNAMIC => 6,
 SHT_NOTE => 7,
 SHT_NOBITS => 8,
 SHT_REL => 9,
 SHT_SHLIB => 10,
 SHT_DYNSYM => 11,
 SHT_INIT_ARRAY => 14,
 SHT_FINI_ARRAY => 15,
 SHT_PREINIT_ARRAY => 16,
 SHT_GROUP => 17,
 SHT_SYMTAB_SHNDX => 18,
 SHT_GNU_ATTRIBUTES => 0x6ffffff5,
 SHT_GNU_HASH => 0x6ffffff6,
 SHT_GNU_LIBLIST => 0x6ffffff7,
 SHT_CHECKSUM => 0x6ffffff8,
 SHT_LOSUNW => 0x6ffffffa,
 SHT_SUNW_move => 0x6ffffffa,
 SHT_SUNW_COMDAT => 0x6ffffffb,
 SHT_SUNW_syminfo => 0x6ffffffc,
 SHT_GNU_verdef => 0x6ffffffd,
 SHT_GNU_verneed => 0x6ffffffe,
 SHT_GNU_versym => 0x6fffffff,
 SHT_LOOS => 0x60000000,
 SHT_HIOS => 0x6fffffff,
 SHT_LOPROC => 0x70000000,
 SHT_HIPROC => 0x7FFFFFFF,
 SHT_LOUSER => 0x80000000,
 SHT_HIUSER => 0xFFFFFFFF,
};

# section attribs
use constant {
 SHF_WRITE => 0x1,
 SHF_ALLOC => 0x2,
 SHF_EXECINSTR => 0x4,
 SHF_MERGE => 0x10,
 SHF_STRINGS => 0x20,
 SHF_INFO_LINK => 0x40,
 SHF_LINK_ORDER => 0x80,
 SHF_OS_NONCONFORMING => 0x100,
 SHF_GROUP => 0x200,
 SHF_TLS => 0x400,
 SHF_MASKOS => 0x0ff00000,
 SHF_MASKPROC => 0xF0000000,
};

# symbol binding
use constant {
 STB_LOCAL => 0,
 STB_GLOBAL => 1,
 STB_WEAK => 2,
 STB_LOOS => 10,
 STB_HIOS => 12,
 STB_MULTIDEF => 13,
 STB_LOPROC => 13,
 STB_HIPROC => 15,
};

# symbol types
use constant {
 STT_NOTYPE => 0,
 STT_OBJECT => 1,
 STT_FUNC => 2,
 STT_SECTION => 3,
 STT_FILE => 4,
 STT_COMMON => 5,
 STT_TLS => 6,
 STT_LOOS => 10,
 STT_AMDGPU_HSA_KERNEL => 10,
 STT_HIOS => 12,
 STT_LOPROC => 13,
 STT_HIPROC => 15,
};

# symbol visibility
use constant {
 STV_DEFAULT => 0,
 STV_INTERNAL => 1,
 STV_HIDDEN => 2,
 STV_PROTECTED => 3,
};

# Dynamic Array Tags
use constant {
 DT_NULL => 0,
 DT_NEEDED => 1,
 DT_PLTRELSZ => 2,
 DT_PLTGOT => 3,
 DT_HASH => 4,
 DT_STRTAB => 5,
 DT_SYMTAB => 6,
 DT_RELA => 7,
 DT_RELASZ => 8,
 DT_RELAENT => 9,
 DT_STRSZ => 10,
 DT_SYMENT => 11,
 DT_INIT => 12,
 DT_FINI => 13,
 DT_SONAME => 14,
 DT_RPATH => 15,
 DT_SYMBOLIC => 16,
 DT_REL => 17,
 DT_RELSZ => 18,
 DT_RELENT => 19,
 DT_PLTREL => 20,
 DT_DEBUG => 21,
 DT_TEXTREL => 22,
 DT_JMPREL => 23,
 DT_BIND_NOW => 24,
 DT_INIT_ARRAY => 25,
 DT_FINI_ARRAY => 26,
 DT_INIT_ARRAYSZ => 27,
 DT_FINI_ARRAYSZ => 28,
 DT_RUNPATH => 29,
 DT_FLAGS => 30,
 DT_ENCODING => 32,
 DT_PREINIT_ARRAY => 32,
 DT_PREINIT_ARRAYSZ => 33,
 DT_MAXPOSTAGS => 34,
 DT_GNU_HASH => 0x6ffffef5,
 DT_VERSYM => 0x6ffffff0,
 DT_RELACOUNT => 0x6ffffff9,
 DT_RELCOUNT => 0x6ffffffa,
 DT_FLAGS_1 => 0x6ffffffb,
 DT_VERNEED => 0x6ffffffe,
 DT_VERNEEDNUM => 0x6fffffff,
 DT_LOOS => 0x6000000D,
 DT_HIOS => 0x6ffff000,
 DT_LOPROC => 0x70000000,
 DT_HIPROC => 0x7FFFFFFF,
};

our %dt_names = (
 DT_NULL() => "DT_NULL",
 DT_NEEDED() => "DT_NEEDED",
 DT_PLTRELSZ() => "DT_PLTRELSZ",
 DT_PLTGOT() => "DT_PLTGOT",
 DT_HASH() => "DT_HASH",
 DT_STRTAB() => "DT_STRTAB",
 DT_SYMTAB() => "DT_SYMTAB",
 DT_RELA() => "DT_RELA",
 DT_RELASZ() => "DT_RELASZ",
 DT_RELAENT() => "DT_RELAENT",
 DT_STRSZ() => "DT_STRSZ",
 DT_SYMENT() => "DT_SYMENT",
 DT_INIT() => "DT_INIT",
 DT_FINI() => "DT_FINI",
 DT_SONAME() => "DT_SONAME",
 DT_RPATH() => "DT_RPATH",
 DT_SYMBOLIC() => "DT_SYMBOLIC",
 DT_REL() => "DT_REL",
 DT_RELSZ() => "DT_RELSZ",
 DT_RELENT() => "DT_RELENT",
 DT_PLTREL() => "DT_PLTREL",
 DT_DEBUG() => "DT_DEBUG",
 DT_TEXTREL() => "DT_TEXTREL",
 DT_JMPREL() => "DT_JMPREL",
 DT_BIND_NOW() => "DT_BIND_NOW",
 DT_INIT_ARRAY() => "DT_INIT_ARRAY",
 DT_FINI_ARRAY() => "DT_FINI_ARRAY",
 DT_INIT_ARRAYSZ() => "DT_INIT_ARRAYSZ",
 DT_FINI_ARRAYSZ() => "DT_FINI_ARRAYSZ",
 DT_RUNPATH() => "DT_RUNPATH",
 DT_FLAGS() => "DT_FLAGS",
 DT_ENCODING() => "DT_ENCODING",
 DT_PREINIT_ARRAY() => "DT_PREINIT_ARRAY",
 DT_PREINIT_ARRAYSZ() => "DT_PREINIT_ARRAYSZ",
 DT_MAXPOSTAGS() => "DT_MAXPOSTAGS",
 DT_GNU_HASH() => "DT_GNU_HASH",
 DT_VERSYM() => "DT_VERSYM",
 DT_RELACOUNT() => "DT_RELACOUNT",
 DT_RELCOUNT() => "DT_RELCOUNT",
 DT_FLAGS_1() => "DT_FLAGS_1",
 DT_VERNEED() => "DT_VERNEED",
 DT_VERNEEDNUM() => "DT_VERNEEDNUM",
 DT_LOOS() => "DT_LOOS",
 DT_HIOS() => "DT_HIOS",
 DT_LOPROC() => "DT_LOPROC",
 DT_HIPROC() => "DT_HIPROC",
);

sub get_dtag_name($)
{
  my $tag = shift;
  return $dt_names{$tag} if exists($dt_names{$tag});
  undef;
}

# lean & mean symbols reading
# arguments:
#  - ref to Reader::Elf object
#  - ref to map where key - name, value - [ address, size ]
#  - ref tp map where key - addr, value - [ name, type ]
# return count of symbols
sub simple_symbols
{
  my( $e, $rsyms, $raddr ) = @_;
  my $s = $e->secs();
  my $sec;
  foreach ( @$s ) {
    if ( $_->[2] == SHT_SYMTAB ) {
      $sec = $_->[0];
      last;
    }
  }
  return 0 if ( !defined($sec) );
  my $syms = $e->syms($sec);
  return 0 if ( !defined $syms );
  my $res = 0;
  foreach ( @$syms ) {
    last if ( !defined $_ );
    next if ( $_->[0] eq '' ); # skip unnamed symbol
    $res++;
    $raddr->{ $_->[1] } = [ $_->[0], $_->[4] ] if ( $_->[1] );
    $rsyms->{ $_->[0] } = [ $_->[1], $_->[2] ] if ( $_->[4] == STT_FUNC );
  }
  return $res;
}

# the same as above but read System.map
# first map is key - name, value - [ address, 0 bcs System.map don't provide sizes]
# second map is key - addr, value - [ name, type where tT is STT_FUNC and STT_OBJECT for anything else]
sub parse_system_map
{
  no warnings "portable";
  my( $fn, $rsyms, $raddr ) = @_;
  my($fh, $str, $name, $addr, $type);
  open($fh, '<', $fn) or die("Cannot open $fn, error $!");
  my $res = 0;
  while( $str = <$fh> )
  {
    chomp $str;
    # addr letter name
    next if ( $str !~ /^([0-9a-f]+) (\S) (\S+)$/i );
    use integer;
    $addr = hex($1);
    $type = ( $2 eq 't' || $2 eq 'T' ) ? STT_FUNC : STT_OBJECT;
    $name = $3;
    $res++;
    $raddr->{ $addr } = [ $name, $type ] if ( $addr );
    $rsyms->{ $name } = [ $addr, 0 ] if ( $type == STT_FUNC );
  }
  close($fh);
  return $res;
}

our @EXPORT = qw(
 ET_NONE
 ET_REL
 ET_EXEC
 ET_DYN
 ET_CORE
 ET_LOOS
 ET_HIOS
 ET_LOPROC
 ET_HIPROC
 ELFCLASSNONE
 ELFCLASS32
 ELFCLASS64
 ELFDATANONE
 ELFDATA2LSB
 ELFDATA2MSB
 PT_NULL
 PT_LOAD
 PT_DYNAMIC
 PT_INTERP
 PT_NOTE
 PT_SHLIB
 PT_PHDR
 PT_TLS
 PT_LOOS
 PT_GNU_EH_FRAME
 PT_GNU_STACK
 PT_GNU_RELRO
 PT_GNU_PROPERTY
 PT_GNU_MBIND_LO
 PT_GNU_MBIND_HI
 PT_PAX_FLAGS
 PT_OPENBSD_RANDOMIZE
 PT_OPENBSD_WXNEEDED
 PT_OPENBSD_BOOTDATA
 PT_SUNWSTACK
 PT_HIOS
 PT_LOPROC
 PT_HIPROC
 PF_X
 PF_W
 PF_R
 PF_MASKOS
 PF_MASKPROC
 SHN_UNDEF
 SHN_LORESERVE
 SHN_LOPROC
 SHN_HIPROC
 SHN_LOOS
 SHN_HIOS
 SHN_ABS
 SHN_COMMON
 SHN_XINDEX
 SHN_HIRESERVE
SHT_NULL
SHT_PROGBITS
SHT_SYMTAB
SHT_STRTAB
SHT_RELA
SHT_HASH
SHT_DYNAMIC
SHT_NOTE
SHT_NOBITS
SHT_REL
SHT_SHLIB
SHT_DYNSYM
SHT_INIT_ARRAY
SHT_FINI_ARRAY
SHT_PREINIT_ARRAY
SHT_GROUP
SHT_SYMTAB_SHNDX
SHT_GNU_ATTRIBUTES
SHT_GNU_HASH
SHT_GNU_LIBLIST
SHT_CHECKSUM
SHT_LOSUNW
SHT_SUNW_move
SHT_SUNW_COMDAT
SHT_SUNW_syminfo
SHT_GNU_verdef
SHT_GNU_verneed
SHT_GNU_versym
SHT_LOOS
SHT_HIOS
SHT_LOPROC
SHT_HIPROC
SHT_LOUSER
SHT_HIUSER
SHF_WRITE
SHF_ALLOC
SHF_EXECINSTR
SHF_MERGE
SHF_STRINGS
SHF_INFO_LINK
SHF_LINK_ORDER
SHF_OS_NONCONFORMING
SHF_GROUP
SHF_TLS
SHF_MASKOS
SHF_MASKPROC
STB_LOCAL
STB_GLOBAL
STB_WEAK
STB_LOOS
STB_HIOS
STB_MULTIDEF
STB_LOPROC
STB_HIPROC
STT_NOTYPE
STT_OBJECT
STT_FUNC
STT_SECTION
STT_FILE
STT_COMMON
STT_TLS
STT_LOOS
STT_AMDGPU_HSA_KERNEL
STT_HIOS
STT_LOPROC
STT_HIPROC
STV_DEFAULT
STV_INTERNAL
STV_HIDDEN
STV_PROTECTED
DT_NULL
DT_NEEDED
DT_PLTRELSZ
DT_PLTGOT
DT_HASH
DT_STRTAB
DT_SYMTAB
DT_RELA
DT_RELASZ
DT_RELAENT
DT_STRSZ
DT_SYMENT
DT_INIT
DT_FINI
DT_SONAME
DT_RPATH
DT_SYMBOLIC
DT_REL
DT_RELSZ
DT_RELENT
DT_PLTREL
DT_DEBUG
DT_TEXTREL
DT_JMPREL
DT_BIND_NOW
DT_INIT_ARRAY
DT_FINI_ARRAY
DT_INIT_ARRAYSZ
DT_FINI_ARRAYSZ
DT_RUNPATH
DT_FLAGS
DT_ENCODING
DT_PREINIT_ARRAY
DT_PREINIT_ARRAYSZ
DT_MAXPOSTAGS
DT_GNU_HASH
DT_VERSYM
DT_RELACOUNT
DT_RELCOUNT
DT_FLAGS_1
DT_VERNEED
DT_VERNEEDNUM
DT_LOOS
DT_HIOS
DT_LOPROC
DT_HIPROC
get_dtag_name
simple_symbols
parse_system_map
);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('Elf::Reader', $VERSION);

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

Elf::Reader - Perl extension for Elf files parsing

=head1 SYNOPSIS

  use Elf::Reader;
  my $er = Elf::Reader->new($filename);
  # for sections
  my $secs = $er->secs();
  print "sections ", scalar( @$secs );
  # for segments
  my $segs = $er->segs();
  print "segments ", scalar( @$segs );
  # for symbols
  foreach ( @$secs ) {
    if ( $_->[2] == SHT_SYMTAB ) {
      my $syms = $er->syms($_->[0]);
      if ( defined($syms) ) {
        foreach my $s ( @$syms ) {
          print $s->[0], $s->[1], "\n";
        }
      }
      last;
    }
  }

for reloc names use Elf::Relocs;

=head1 DESCRIPTION

Perl binding for ELFIO library (https://github.com/serge1/ELFIO/commits/main/)
Support array-like iterators for sections, segments, symbols and relocs

secs items are arrays with indexes
 0 - index
 1 - name, string
 2 - type
 3 - flags
 4 - info
 5 - link
 6 - addr_align
 7 - entry_size
 8 - address, 64bit
 9 - size
 10 - offset, 64bit

segs items are arrays with indexes
 0 - index
 1 - type
 2 - flags
 3 - align
 4 - virtual address, 64bit
 5 - physical address, 64bit
 6 - file size
 7 - memory size
 8 - offset, 64bit

syms items are arrays with indexes
 0 - name
 1 - value, 64bit
 2 - size
 3 - bind
 4 - type
 5 - section
 6 - other

You can extract symbols from section with type == SHT_SYMTAB or SHT_DYNSYM, argument for syms method is section index

relocs items are arrays with indexes
 0 - offset, 64bit
 1 - symbol
 2 - type
 3 - addend

You can extract relocations from section with type == SHT_REL or SHT_RELA, argument for rels method is section index

dyns items are arrays with indexes
 0 - name
 1 - tag
 2 - value

You can extract dynamics from section with type == SHT_DYNAMIC, argument for dyns method is section index

notes itetms are array with indexes
 0 - name, string
 1 - type
 2 - desclen
 3 - desc, addr
 4 - desc, blob

You can extract notes from section with type == SHT_NOTE, argument for dyns method is section index

versyms items are arrays with indexes
 0 - version
 1 - filename, string
 2 - hash
 3 - flags
 4 - other
 5 - dep_name, string

=head3 Boyer Moore string search methods
 bm_idx(self, pattern, section_index)
 bmz_idx - the same as bm_idx but includes trailing zero
 bm_from(self, pattern, address)
 bmz_from - the same as bm_from but includes trailing zero

=head2 EXPORT

Names of ELF constants

=head1 SEE ALSO

Mention other useful documentation such as the documentation of
related modules or operating system documentation (such as man pages
in UNIX), or any relevant external documentation such as RFCs or
standards.

If you have a mailing list set up for your module, mention it here.

If you have a web site set up for your module, mention it here.

=head1 AUTHOR

redp, <lt>redp@mail.ru<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2024 by redp

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.30.0 or,
at your option, any later version of Perl 5 you may have available.


=cut
