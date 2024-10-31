#!perl -w
# test to dump structs from dwarf debug info
# 31 oct 2024 (c) redp
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

my $s = $dw->named(TStruct);
printf("%d enums\n", scalar @$s);
my %dumped;
foreach my $en ( @$s )
{
  my $el = $dw->by_id($en);
  next if !defined($el);
  # filter some names
  next if ( $el->name() !~ /kmem/ );
  my $m = $el->members();
  next if !defined($m);
  printf("%s size %d:\n", $el->name(), $el->size());
  foreach my $f ( @$m )
  {
    printf(" off %X %s\n", $f->offset(), $f->name());
  }

}