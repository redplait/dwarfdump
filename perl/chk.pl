#!perl -w
# check presence of files in some directory
# args: catalog.file dir
use strict;
use warnings;
use Data::Dumper;

# dictionary item, key is dir/file name
# - ref to map of next level if dir, else
# - file size
sub add_file
{
  my($root, $fname, $fsize) = @_;
  $root->{$fname} = $fsize;
}

sub add_dir
{
  my($root, $fname) = @_;
  my %next;
  $root->{$fname} = \%next;
  return \%next;
}

sub dump_dir
{
  my $dr = shift;
  foreach my $i ( @$dr ) {
    printf("%s -> ", $i->[1]);
  }
  printf("\n");
}

sub read_cat
{
  my $fname = shift;
  open FILE, $fname or die("Cannot open file " . $fname);
  my (%res, @dirlist, $size, $name, $str, $i);
  my $dsize = 0;
  my $root = \%res;
  my $prev = sub {
    return \%res unless( @dirlist );
    pop @dirlist;
    return \%res unless( @dirlist );
    $dirlist[-1]->[0];
  };
  while ( $str = <FILE> )
  {
    $str =~ s/\x0d\x0a$//;
    chomp $str;
    # check for dir pattern
    if ( $str =~ /^(\s*)DIR: (.*)$/ )
    {
      $dsize = scalar @dirlist;
      my $margin = length $1;
# printf("m %d -> %d\n", $margin, $dsize);
      if ( !$margin ) {
        $root = add_dir(\%res, $2);
        @dirlist = ( );
      } elsif ( $margin == $dsize ) # new dir at the same level
      {
        $root = add_dir($root, $2);
      } elsif ( $margin == $dsize - 1 ) { # dir at prev level
        $root = add_dir($prev->(), $2);
      } elsif ( $margin < $dsize ) {
        for ( $i = $margin; $i < $dsize; $i++ )
        {
          $root = $prev->();
        }
        $root = add_dir($root, $2);
      } else {
        die "Strange margin " . $margin . " old was " . $dsize;
      }
      push @dirlist, [ $root, $2 ];
      next;
    }
    if ( $str =~ /^(\s*)(.*) (\d+) ([a-f|0-9]{32})$/ )
    {
      $name = $2;
      $size = int $3;
      # check level
      my $margin = length $1;
      if ( !$margin ) {
        @dirlist = ( );
        $root = \%res;
        add_file($root, $name, $size);
        next;
      } elsif ( $margin < $dsize ) {
        for ( $i = $margin; $i < $dsize; $i++ ) {
          $root = $prev->();
        }
        $dsize = scalar @dirlist;
      }
      add_file($dirlist[$margin-1]->[0], $name, $size);
    }
  }
  close FILE;
  \%res;
}

# recursive
sub cmp_dir
{
  my($root, $dir) = @_;
  my %visited;
  # check missed
  while ( my($name, $v) = each %$root ) {
    if ( ref $v ) {
      # make new path
      my $newd = $dir . '/' . $name;
      if ( ! -d $newd ) {
        printf("no %s DIR\n", $newd);
      } else {
        $visited{$name} = 2;
        cmp_dir($v, $newd);
      }
      next;
    }
    # check size of this file
    my $fname = $dir . '/' . $name;
    if ( ! -s $fname ) {
      printf("no file %s\n", $fname);
    } else {
      $visited{$name} = 1;
    }
  }
  # check newly added
  opendir(DIR, $dir) or die("cannot open $dir, error $!");
  while ( my $str = readdir(DIR) ) {
    next if ( $str eq '.' || $str eq '..' );
    next if exists($visited{$str});
    # yep
    my $what = $dir . '/' . $str;
    if ( -d $what ) {
      printf("NEW DIR %s\n", $what);
    } else {
      printf("NEW FILE %s\n", $what);
    }
  }
  closedir DIR;
}

# main
my $asize = scalar @ARGV;
die("where is args?") if ( !$asize );
my $hr = read_cat($ARGV[0]);
if ( $asize < 2 ) {
 print Dumper($hr);
} else {
 cmp_dir($hr, $ARGV[1]);
}