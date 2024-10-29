#!perl -w
# test to dump enums from dwarf debug info
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

my $enums = $dw->named(TEnum);
printf("%d enums\n", scalar @$enums);
my %dumped;
foreach my $en ( @$enums )
{
  my $el = $dw->by_id($en);
  next if !defined($el);
  my $iter = $el->enums();
  next if !defined($iter);
  my $name = $el->link_name();
  $name = $el->name() if !defined($name);
  next if exists $dumped{$name};
  printf("%s:\n", $name);
  $dumped{$name} = 1;
  foreach ( @$iter ) {
    print " ", $_->[0], " -> ", $_->[1], "\n";
  }
}