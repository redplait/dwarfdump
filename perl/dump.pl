#!/usr/bin/perl
#
# This fine script check content on DVD and on disk
# 7 Apr 2007 (C) RedPlait

# format of entry for file
# [0] - type, 'f' for file, 'd' for dir
# For files:
# [1] - filename
# [2] - size
# [3] - md5 hash
# For dirs:
# [1] - dir name
# [2] - full dir name
# [3] - array of entries in this dir

# 14 Jun 2008, RedPlait
# It seems that Digest::MD5::File::file_md5_hex mistically slow on some files
# so I just replace this method to separate function my_md5
use strict;
use warnings;
use Digest::MD5;

sub my_md5
{
  my $name = shift;
  open DFILE, $name or die("Cannot open file " . $name);
  binmode DFILE;
  my $res = Digest::MD5->new->addfile(*DFILE)->hexdigest;
  close DFILE;
  return $res;
}

### traverse function - recursive
sub traverse_dir
{
  my $full_path = shift;
  my($str, $fullname, @array, @dar, $size, $hash);

  $full_path =~ s/[\\\/]$//;
  $full_path .= '/';
  my $i = 0;
  opendir(DIR, $full_path) or return undef;

  while( $str = readdir(DIR) )
  {
    next if ( $str eq '.' or
              $str eq '..'
            );
    $fullname = $full_path . $str;
    if ( -f $fullname )
    {
      $size = -s $fullname;
      $hash = my_md5($fullname);
      push @array, [ 'f', $str, $size, $hash ];
      $i++;
    } elsif ( -d $fullname )
    {
      push @dar, [$str, $fullname];
      $i++;
    }
  }
  closedir DIR; 
  return undef if ( ! $i );
  # lets traverse all sub-dirs
  foreach $str ( sort { $a->[0] cmp $b->[0] } @dar )
  {
    $i = traverse_dir($str->[1]);
    next if ( ! defined $i );
    push @array, [ 'd', $str->[0], $str->[1], $i ];
  }
  return undef if ( -1 == $#array );
  return \@array;
}

### dump function - recursive
sub dump_tree
{
  my ($ref, $margin) = @_;
  return if ( ! defined $ref );

  $margin = 0 if ( ! defined $margin );
  my ($mstr, $enum);
  $mstr = ' ' x $margin;
  foreach $enum ( sort { ( $a->[0] cmp $b->[0] ) or 
                         ( uc($a->[1]) cmp uc($b->[1]) )
                       } @$ref )
  {
    if ( $enum->[0] eq 'd' )
    {
      printf("%sDIR: %s\n", $mstr, $enum->[1]);
      dump_tree($enum->[3], $margin + 1);
    } else {
      printf("%s%s %d %s\n", $mstr, $enum->[1], $enum->[2], $enum->[3]);
    }
  }
}

# main
my $ref = traverse_dir $ARGV[0];
dump_tree $ref;