#!perl -w
# dummy script to extract elf relocs from binutils
# mostly like https://sourceforge.net/p/cyrplw/code/HEAD/tree/perl/pm/allins.pl
use strict;
use warnings;

# items [ name, [ list of [ reloc_name, value ] ]
my @gar;

sub dump_rels
{
  my( $i1, $i2, @tmp );
  printf("use constant {\n");
  foreach $i1 ( @gar ) {
    printf("# %s\n", $i1->[0]);
    foreach $i2 ( @{ $i1->[1] } ) {
      printf("%s => %s,\n", $i2->[0], $i2->[1]);
      push @tmp, $i2->[0];
    }
  }
  printf<<'END';
};
###
#
# E X P O R T E D   N A M E S
#
###
our @EXPORT = qw(
END
 foreach ( @tmp ) {
   printf("%s\n", $_);
 };
  printf(");\n");
}

sub parse
{
  my $fname = shift;
  my $res = 0;
  my $state = 0;
  my @arr;
  my($str, $ename, $fh, $name, $v);
  open($fh, '<', $fname) or die("cannot open $fname, error $!");
  while( $str = <$fh> )
  {
    if ( !$state ) {
      if ( $str =~ /START_RELOC_NUMBERS\s*\(([^\)]+)\)/ ) {
        $ename = $1;
        # extract proc name
        $ename =~ s/^elf_//;
        $ename =~ s/_reloc_type\s*$//;
        $state = 1;
      }
      next;
    }
    last if ( $str =~ /END_RELOC_NUMBERS/ );
    if ( $str =~ /RELOC_NUMBER\s*\((\S+)\s*,\s*([^\)]+)\)/ ) {
      $res++;
      push @arr, [ $1, $2 ];
    }
  }
  close $fh;
  push(@gar, [ $ename, \@arr ]) if ( $res );
  return $res;
}

# main
my $alen = scalar(@ARGV);
if ( $alen != 1 ) {
  printf("Usage: dir_name\n");
  return 6;
};

# parse filed
my $fname = $ARGV[0];
my $res = 0;
$fname =~ s/\/$//;
my $dh;
opendir($dh, $fname) or die("cannot open dir $fname, error $!");
while( my $f = readdir $dh ) {
  $res++ if ( parse($fname . '/' . $f) );
}
closedir $dh;
dump_rels() if ( $res );