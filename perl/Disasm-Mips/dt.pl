#!perl -w
# test for mips disasm binding
# 18 oct 2024 (c) redp
use strict;
use warnings;

use Elf::Reader;
use Disasm::Mips;

# key - name, value - [ address, size ]
my %g_syms;

# key - addr, value - [ name, type ]
my %g_addr;

sub disasm_func
{
  my($d, $ar) = @_;
  $d->setup( $ar->[0] );
  while ( my $len = $d->disasm() )
  {
    my $adr = $d->addr();
    printf("%X: %s", $adr, $d->text());
    my $ja = $d->is_jal();
    if ( $ja && exists($g_addr{ $ja }) ) {
      printf(" ; %s", $g_addr{ $ja }->[0] );
    }
    printf("\n");
    last if ( $adr >= $ar->[0] + $ar->[1] );
  }
}

# main
if ( scalar( @ARGV) < 1 ) {
  printf("Usage: file symbols ...\n");
  return 6;
}
my $e = Elf::Reader->new($ARGV[0]);
die("Cannot load $ARGV[0]") if ( !defined($e) ) ;
my $s = $e->secs();
my $sec;
foreach ( @$s ) {
 if ( $_->[2] == Elf::Reader::SHT_SYMTAB ) {
   $sec = $_->[0];
   last;
 }
}
die("cannot find symbols") if ( !defined($sec) ) ;
my $syms = $e->syms($sec);
foreach ( @$syms ) {
  last if ( !defined $_ );
  next if ( $_->[0] eq '' );
  $g_addr{ $_->[1] } = [ $_->[0], $_->[4] ] if ( $_->[1] );
  $g_syms{ $_->[0] } = [ $_->[1], $_->[2] ] if ( $_->[4] == STT_FUNC );
}

# make disasm
my $d = Disasm::Mips->new($e);
# process remaining symbols
shift @ARGV;
foreach ( @ARGV ) {
  if ( !exists $g_syms{ $_ } ) {
    printf("cannot find symbol %s\n", $_);
    next;
  }
  printf("%s:\n", $_);
  disasm_func($d, $g_syms{ $_ });
}