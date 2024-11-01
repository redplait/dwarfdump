#!perl -w
# test to dump virtial tables from dwarf debug info
# 1 nov 2024 (c) redp
use strict;
use warnings;
use Elf::Reader;
use Dwarf::Loader;

# key - class id, value - list off [ voffset method ] or undef
my %dumped;

sub find_vtbl
{
  my($dw, $met) = @_;
  my @res;
  foreach ( @$met ) {
    next if ( !$_->mvirt() );
    push @res, [ $_->mvtbl_idx(), $_ ];
  }
  return undef if !scalar(@res);
  return \@res;
}

my $asize = scalar @ARGV;
die("usage: filename\n") if ( $asize != 1 );
my $e = Elf::Reader->new($ARGV[0]);
die("cannot load $ARGV[0]") if !defined($e);

my $dw = Dwarf::Loader->new($e);
die("cannot load dwarf") if !defined($dw);

my $s = $dw->named(TClass);
printf("%d classes\n", scalar @$s);
foreach my $en ( @$s )
{
  my $el = $dw->by_id($en);
  next if !defined($el);
  # filter some names
  my $name = $el->name();
  $name = $el->link_name() if !defined($name);
  next if ( $name =~ /cxx/ );
  my $m = $el->methods();
  next if !defined($m);
  my $res = find_vtbl($dw, $m);
  next if !defined($res);
  # dump vtbl
  printf("%s vtbl:\n", $name);
  foreach ( sort { $a->[0] <=> $b->[0] } @$res )
  {
    my $miname = $_->[1]->name();
    $miname = $_->[1]->link_name() if !defined($miname);
    printf(" %X: %s\n", $_->[0], $miname);
  }
}