#!perl -w
# test to find addressable function with argument ptr to struct bpf_prog
# 29 oct 2024 (c) redp
use strict;
use warnings;
use Elf::Reader;
use Dwarf::Loader;
use Getopt::Std;

use vars qw/$opt_c $opt_m $opt_u/;

sub usage
{
  print STDERR<<EOF;
usage: $0 [options] filename struct_name
Options:
 -m search for methods instead of functions
 -c search class
 -u search union
EOF
  exit(8);
}

# key - name, value - [ address, size ]
my %g_syms;

# key - addr, value - [ name, type ]
my %g_addr;

my $status = getopts("cmu");
usage() if ( !$status );
my $asize = scalar @ARGV;
usage() if ( $asize != 2 );
my $e = Elf::Reader->new($ARGV[0]);
die("cannot load $ARGV[0]") if !defined($e);
my $scount = simple_symbols($e, \%g_syms, \%g_addr);
die("cannot find symbols") if ( !$scount ) ;

my $dw = Dwarf::Loader->new($e);
die("cannot load dwarf") if !defined($dw);

my $what = TStruct;
$what = TUnion if defined($opt_u);
$what = TClass if defined($opt_c);

# get hash of TSubs, key addr, value id
my $href = $dw->addressable(defined($opt_m) ? TMethod : TSub);
while ( my($addr, $id) = each %$href )
{
  my $el = $dw->by_id($id);
  next if !defined($el);
  # can be abstracted from real function (and thus not prototyped)
  my $abs = $el->abs();
  if ( defined($abs) ) {
    $el = $dw->by_id($abs);
    next if !defined($el);
  }
  # skip unnamed functions
  my $cnf = $el->name();
  next if !defined($cnf);
  # get function arguments
  my $args = $el->params();
  next if !defined $args;
  # check all params
  my $idx = 0;
  foreach my $a ( @$args ) {
    $idx++;
    # find type of arg
    my $at = $dw->by_id( $a->type_id() );
    next if !defined($at);
    # find last item in ptrs chain
    my $rt = unchain($dw, $at);
    next if !defined($rt);
    # check that it is structure
    next if $rt->type() != TStruct;
    if ( $rt->name() eq $ARGV[1] ) {
      # check that this is really exported function with the same name
      last if ( !exists $g_addr{ $addr } );
      my $na = $g_addr{ $addr };
      my $nf = $na->[0];
      # remove trailing .part, .isra and other useless prefixes
      $nf =~ s/\.part\.\d+$//;
      $nf =~ s/\.isra\.\d+$//;
      $nf =~ s/\.constprop\.\d+$//;
      if ( $nf ne $cnf ) {
        printf(STDERR "skip %s bcs at this address located %s\n", $cnf, $na->[0]);
        last;
      }
      printf("%d %s: %X\n", $idx, $cnf, $addr);
      last;
    }
  }
}