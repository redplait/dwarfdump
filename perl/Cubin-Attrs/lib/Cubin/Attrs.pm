package Cubin::Attrs;

use 5.030000;
use strict;
use warnings;

require Exporter;
use AutoLoader qw(AUTOLOAD);

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Cubin::Attrs ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

sub collect
{
  my $ca = shift;
  my %aoffs;
  my @res = ( \%aoffs, undef );
  # first check indirect branches
  my $ibs = $ca->grep(0x34);
  if ( defined $ibs ) {
    my %second;
    my $added = 0;
    # indirect branches always only 1
    my $ib_values = $ca->value($ibs->[0]->{'id'});
    foreach my $iv ( @$ib_values ) {
      while( my($addr, $op) = each %$iv ) {
         $aoffs{$addr} = 0x34;
         next unless defined($op); # empty
         if ( 'ARRAY' ne ref $op ) { # single address
           $second{$op} = $addr;
           $added++;
         } else { # list of addresses
           $second{$_} = $addr foreach ( @$op );
           $added++;
         }
      }
    }
    $res[1] = \%second if ( $added );
  }
  # then collect remainings offset attrs
  my @grepped = $ca->grep_list( [0x28, 0x1c, 0x1d, 0x25, 0x31, 0x39, 0x47] );
  if ( scalar @grepped ) {
    foreach my $id ( @grepped ) {
      my $value = $ca->value($id->{'id'});
      next unless defined($value);
      foreach my $iv ( @$value ) {
        $aoffs{$_} = $id->{'attr'} foreach ( @$iv );
      }
    }
  }
  return wantarray ? @res : \@res;
}

our @EXPORT = qw(
 collect
);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('Cubin::Attrs', $VERSION);

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

 Perl extension for reading/parsing/patching attributes of cubin files

=head1 SYNOPSIS

  use Elf::Reader;
  use Cubin::Attrs;
  # read elf
  my $e = Elf::Reader->new($fname);
  # make cubin attrs object
  my $fb = Cubin::Attrs->new($e);
  # and finally read here section 6
  $fb->read(6);

=head1 DESCRIPTION

There are 4 groups of methods

=over

=item 1) selecting section and reading

=item 2) extracting attributes

=item 3) patching attributes

=item 4) patching relocs

=back

=head3 Reading

To read attributes section you should use 'read' method
Also if you know only index of section with code you can try to find correspoding attributes section with method $fb->try(code_index)

=head3 Extracting attributes

First of all, it should be noted that the attribute format is not officially documented. So I made some reverse engeneering of them.
Each attribute can be in 4 forms:

=over

=item 1) just boolean TRUE

=item 2) 1-byte value

=item 3) 2-byte value, presumable 16bit WORD

=item 4) attributes with arbitrary length

=back

So after calling 'read' method you have list of attributes - size can be obtained with method 'count' and Cubin::Attrs is tiead array
of them - so you can extract attribute by index with just $fb->[index], result is ref to hash

=over

=item id) index of this attribute

=item attr) tag of attribute (exported as EIATTR_XXX)

=item form) - form of this attribute

=item off) - offset of this attribute

=item len) - length of this attribute

=back

To extract value of specific attribute use methos $fb->value(index).

To filter specific tags you can use method $fb->grep(tag) - results is ref to array with the same hashes as obtainded with indexing attributes

To filter many tags use method $fb->grep_list([ tag1, tag2, ...])

Some attributes (XXX_INSTR_OFFSETS) are lists of offsets - in this case result is ref to array with offsets

Also there is special processing of CB params - you can extract size of params with method 'params_cnt" and param itself with
method $fn->param(param_index), result is ref to hash

=over

=item off) offset of param

=item size) size of param

=item ord) ordinal of param

=back

Indirect branches returned as hash where key is offset of branch and value can be

=over

=item undef - no addresses

=item scalar - single address

=item ref to array if those branch has several targets

=back


=head3 Patching attributes

Unfortunately you can patch attributes only in-place - this means that for example size of patched lists of offsets must be the same as original.

Also you can patch only limited set of attribures with methods:

=over

=item patch(index, value) to patch byte/word/dword values

=item patch_addr(index, offset_index, offset_value) to replace offset with index offset_index to new offset_value

=item patch_alist(index, \@offset_array) to fully replace array of offsets

=item patch_ibt($address, $sv) to patch indirect target from offset $address, new list passed as ref to array in $sv.
Sizes of new and old lists must be the same, in case if there was single offset - you can pass it's value in $sv as scalar

=back


=head3 Patching relocs

patch_ft($s_idx, $r_idx, $reloc_type) - fix type of reloc at index $r_idx in section with index $s_idx to type $reloc_type

patch_ftif - almost the same as previous but has additional argument - old reloc type

to find s_idx you can use methods try_rel/try_rela - both requires section index for which get index of appropriate relocs section


=head2 EXPORT

=head3 collect

collects offsets having attributes

arg: loaded for some section Cubin::Attrs

returns couple of hashes where key is offset [
 - hash_of_offsets, where values are type of attribute
 - hash_of_indirects where values are list of reffered to this offset indirect branches (bcs the same offset can be
   target from many indirect branches)
]

supports wantarray


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

Copyright (C) 2025 by redp

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.30.0 or,
at your option, any later version of Perl 5 you may have available.


=cut
