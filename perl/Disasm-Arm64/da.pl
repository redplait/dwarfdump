#!perl -w
# test for Arm64 disasm binding
# 22 oct 2024 (c) redp
use strict;
use warnings;

use Elf::Reader;
use Disasm::Arm64;
use Interval::Tree;

# key - name, value - [ address, size ]
my %g_syms;

# key - addr, value - [ name, type ]
my %g_addr;

sub disasm_func
{
  my($d, $ar) = @_;
  # queue of addresses to process
  my @Q;
  my $tree = Interval::Tree->new();
  push @Q, $ar->[0];
  while ( @Q )
  {
    my $adr = shift @Q;
    next if ( $tree->in_tree($adr) );
    my $rp = $d->regpad();
    my $next = $tree->next($adr);
    if ( $next ) {
      $d->setup2( $adr, $next - $adr );
    } else {
      $d->setup( $adr );
    }
    while ( my $len = $d->disasm() )
    {
      my $adr = $d->addr();
      my $cc = $d->cc();
      if ( defined $cc ) {
        printf("%X: %s ; cc %d %d", $adr, $d->text(), $cc, $d->idx());
      } else {
        printf("%X: %s ; %d", $adr, $d->text(), $d->idx());
      }
      my $ja = $d->bl_jimm();
      if ( $ja && exists($g_addr{ $ja }) ) {
        printf(" -> %s", $g_addr{ $ja }->[0] );
      }
      # find jxx
      my $j = $d->is_jxx();
      # push addr in queue if it was not processed yet
      if ( $j ) {
        my $p = $tree->in_tree($j);
        if ( !$p ) { push(@Q, $j) }
        else { printf(" [-]"); }
      }
      # adrp/add pairs
      my $pa = $d->apply($rp);
      if ( defined($pa) && exists( $g_addr{$pa} ) ) {
        printf(" %s", $g_addr{ $pa }->[0] );
      }
      printf("\n");
    }
    # add to tree
    $tree->insert( $adr, $d->addr() - $adr );
    printf("--\n");
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
my $d = Disasm::Arm64->new($e);
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