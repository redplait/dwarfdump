#!perl -w
# dummy script to extract risc-v instructions
# mostly like https://sourceforge.net/p/cyrplw/code/HEAD/tree/perl/pm/allins.pl
use strict;
use warnings;

sub parse
{
  my $fname = shift;
  my $res = 0;
  my @arr;
  my($str, $ename, $fh, $name, $v);
  open($fh, '<', $fname) or die("cannot open $fname, error $!");
  printf("use constant {\n");
  my $state = 0;
  while( $str = <$fh> )
  {
    if ( !$state ) {
      next if ( $str !~ /^\s+RISCV_INS_INVALID/ );
      $state = 1;
    }
    last if ( $state && $str =~ /^\s+RISCV_INS_ENDING/ );
    if ( $state && $str =~ /^\s+RISCV_INS_(\w+),?/ ) {
      # last if ( $1 eq 'OPERATION_END' );
      printf(" RISCV_%s => %d,\n", $1, $res++);
      push @arr, 'RISCV_' . $1;
    }
  }
  close $fh;
  # dump
  printf<<'END';
};
###
#
# E X P O R T E D   N A M E S
#
###
our @EXPORT = qw(
END
 foreach ( @arr ) {
   printf("%s\n", $_);
 };
  printf(");\n");

  return $res;
}

# main
my $alen = scalar(@ARGV);
if ( $alen != 1 ) {
  printf("Usage: mips.h\n");
  return 6;
};

parse($ARGV[0]);
