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

Dwarf::Loader - Perl extension for blah blah blah

=head1 SYNOPSIS

  use Dwarf::Loader;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for Dwarf::Loader, created by h2xs. It looks like the
author of the extension was negligent enough to leave the stub
unedited.

Blah blah blah.

=head2 EXPORT

None by default.



=head1 SEE ALSO

Mention other useful documentation such as the documentation of
related modules or operating system documentation (such as man pages
in UNIX), or any relevant external documentation such as RFCs or
standards.

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
