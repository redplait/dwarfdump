#!perl -w
# test to find addressable function with argument ptr to struct bpf_prog
# 29 oct 2024 (c) redp
use strict;
use warnings;
use Elf::Reader;
use Dwarf::Loader;

my $asize = scalar @ARGV;
die("usage: filename\n") if ( $asize != 1 );
my $e = Elf::Reader->new($ARGV[0]);
die("cannot load $ARGV[0]") if !defined($e);

my $dw = Dwarf::Loader->new($e);
die("cannot load dwarf") if !defined($dw);

# get hash of TSubs, key addr, value id
my $href = $dw->addressable(TSub);
printf("%d funcs\n", scalar keys %$href);
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
    if ( $rt->name() eq 'bpf_prog' ) {
      printf("%s arg %d: %X\n", $el->name(), $idx, $addr);
      last;
    }
  }
}