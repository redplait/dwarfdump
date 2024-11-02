package Dwarf::Loader;

use 5.030000;
use strict;
use warnings;

require Exporter;
use AutoLoader qw(AUTOLOAD);

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Dwarf::Loader ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

# access
use constant {
 Public => 1,
 Protected => 2,
 Private => 3,
};

our @EXPORT = qw(
TArray
TSubrange
TClass
TInterface
TEnum
TMember
TPtr
TStruct
TUnion
TTypedef
TBase
TConst
TVolatile
TRestrict
TDynamic
TAtomic
TImmutable
TRef
TRValue
TSubType
TArg
TSub
TMethod
TPtr2Member
TUnspec
TVar
TVariant
Public
Protected
Private
unchain
ATE_address
ATE_boolean
ATE_complex_float
ATE_float
ATE_signed
ATE_signed_char
ATE_unsigned
ATE_unsigned_char
ATE_imaginary_float
ATE_packed_decimal
ATE_numeric_string
ATE_edited
ATE_signed_fixed
ATE_unsigned_fixed
ATE_decimal_float
ATE_UTF
ATE_UCS
ATE_ASCII
ATE_lo_user
ATE_hi_user
);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('Dwarf::Loader', $VERSION);

# sub to get final type in chain of pointers/references
# args: dwarf_reader element
sub unchain
{
  my($dw, $e) = @_;
  my $t = $e->type();
  while( defined($t) && ( $t == TPtr() || $t == TConst() || $t == TVolatile() || $t == TImmutable() || $t == TRef() ) )
  {
    $e = $dw->by_id($e->type_id());
    last if !defined($e);
    $t = $e->type();
  }
  return $e;
}

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

Dwarf::Loader - Perl extension for loading/parsing DWARF debug info
C++ sources for dwarf parser itself located in ../.. dir

=head1 SYNOPSIS

  use Dwarf::Loader;
  use Elf::Reader;
  my $e = Elf::Reader->new($ARGV[0]);
  die("cannot load $ARGV[0]") if !defined($e);

  my $dw = Dwarf::Loader->new($e);
  die("cannot load dwarf") if !defined($dw);

  ...

=head1 DESCRIPTION

In DWARF all revolved around so called DIE - it representing some tag (tag_id is essentially offset in section .debug_abbrev)
 and all attributes, in my dwarf library this is TreeBuilder::Element (exposed to perl as Dwarf::Loader::Element)
There is problem when you read some tag - is it full-fledged DIE or just some limited nameless anchor for another type like
 DW_TAG_const_type
  DW_AT_type xxx
xxx DW_TAG_pointer_type
  DW_AT_type yyy
yyy DW_TAG_some_real_type


For some tags you can safely predict to which class it falls - like DW_TAG_enumerator or DW_TAG_member but in general you can't
So there are two approach how to read and parse DWARF debug info
1) do all in one pass. Obvious drawback is memory consumption - each DIE should keep all possible attributes (see Table A.1
 on page 266 for list of all possible attributes for each tag)
2) scan first pass only to build indexes of named/addressable DIEs and then read and parse each DIE on demand. In such case
we need to do lots of book-keeping for each CU - like store bases for things like loclists/offsets/addresses/rnglists etc

I decided to use first approach - despite the fact that on x64 sizeof(Element) is 224 bytes as of this writing. To save space
some tags keeping in smaller types:
 - DW_TAG_inheritance is type Parent (exposed to perl as Dwarf::Loader::Parent)
 - DW_TAG_formal_parameter is type FormalParam (exposed to perl as Dwarf::Loader::Param)
 - DW_TAG_enumerator is type EnumItem (can be accessed via Dwarf::Loader::EnumIterator)
and keep lists of nested elements in structure Compound (sizeof is 200 bytes). Can't say I'm satisfied with the result -
in typical DWARF lots of xxx_type tags occupy only several bytes vs 224 in my library

=head2 EXPORT

None by default.



=head1 SEE ALSO

 DWARF Version 5 Standard: https://dwarfstd.org/doc/DWARF5.pdf

If you have a mailing list set up for your module, mention it here.

If you have a web site set up for your module, mention it here.

=head1 AUTHOR

redp, E<lt>redp@E<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2024 by redp

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.30.0 or,
at your option, any later version of Perl 5 you may have available.


=cut
