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

Elf::Reader - Perl extension for blah blah blah

=head1 SYNOPSIS

  use Elf::Reader;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for Elf::Reader, created by h2xs. It looks like the
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
