#!perl -w
# test to dump structs from dwarf debug info
# 31 oct 2024 (c) redp
use strict;
use warnings;
use Elf::Reader;
use Dwarf::Loader;

sub dump_named
{
  my($t, $name, $what) = @_;
  if ( $name ) {
    printf(" %s %s", $what, $name);
  } else {
    printf(" unnamed_%s %X", $what, $t->tag());
  }
}

sub dump_type
{
  my($dw, $t) = @_;
  return if !defined($t);
  my $what = $t->type();
  my $name = $t->name();
  $name = $t->link_name() if !defined($name);
  if ( $what == TBase ) {
    printf(" %s", $name);
    return;
  }
  if ( $what == TClass ) {
    dump_named($t, $name, "class");
    return;
  }
  if ( $what == TStruct ) {
    dump_named($t, $name, "struct");
    return;
  }
  if ( $what == TUnion ) {
    dump_named($t, $name, "union");
    return;
  }
  if ( $what == TEnum ) {
    dump_named($t, $name, "enum");
    return;
  }
  if ( $what == TSubType ) {
    # this is ptr to some function
    printf(" sub_%X", $t->tag());
    return;
  }
  goto nt if ( $what == TTypedef );
  # complex types like ptr
  if ( $what == TConst ) {
    printf(" const");
    goto nt;
  }
  if ( $what == TPtr ) {
       printf(" *");
    goto nt;
  }
  if ( $what == TArray ) {
    printf(" [%d]", $t->count());
    goto nt;

  }
  printf(" %d ???", $what);
  return;
nt:
  dump_type($dw, $dw->by_id( $t->type_id() ));
}

my $asize = scalar @ARGV;
die("usage: filename\n") if ( $asize != 1 );
my $e = Elf::Reader->new($ARGV[0]);
die("cannot load $ARGV[0]") if !defined($e);

my $dw = Dwarf::Loader->new($e);
die("cannot load dwarf") if !defined($dw);

my $s = $dw->named(TStruct);
printf("%d structs\n", scalar @$s);
foreach my $en ( @$s )
{
  my $el = $dw->by_id($en);
  next if !defined($el);
  # filter some names
  my $name = $el->name();
  $name = $el->link_name() if !defined($name);
  next if ( $name !~ /kmem/ );
  my $m = $el->members();
  next if !defined($m);
  printf("%s size %d:\n", $el->name(), $el->size());
  foreach my $f ( @$m )
  {
    printf(" off %X %s", $f->offset(), $f->name());
    my $bsize = $f->bit_size();
    if ( $bsize ) {
      printf(" :1 bit_off %d", $bsize, $f->bit_offset());
    } else {
      dump_type($dw, $dw->by_id($f->type_id()));
    }
    print "\n";
  }
}