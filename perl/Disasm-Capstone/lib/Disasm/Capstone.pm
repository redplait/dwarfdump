package Disasm::Capstone;

use 5.030000;
use strict;
use warnings;

require Exporter;
use AutoLoader qw(AUTOLOAD);

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Disasm::Capstone ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
CS_OP_INVALID
CS_OP_REG
CS_OP_IMM
CS_OP_FP
CS_OP_PRED
CS_OP_SPECIAL
CS_OP_BOUND
CS_OP_MEM
CS_OP_MEM_REG
CS_OP_MEM_IMM
CS_AC_READ
CS_AC_WRITE
CS_AC_READ_WRITE
);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('Disasm::Capstone', $VERSION);

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

Disasm::Capstone - Perl extension for blah blah blah

=head1 SYNOPSIS

  use Disasm::Capstone;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for Disasm::Capstone, created by h2xs. It looks like the
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
