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
  # queue of addresses to process, [ addr regpad ]
  my @Q;
  my $tree = Interval::Tree->new();
  push @Q, [ $ar->[0], $d->regpad() ];
  while ( @Q )
  {
    my($adr, $rp) = @{ shift @Q };
    next if ( $tree->in_tree($adr) );
    my $next = $tree->next($adr);
    if ( $next ) {
      $d->setup2( $adr, $next - $adr);
    } else {
      $d->setup( $adr );
    }
    while ( my $len = $d->disasm() )
    {
      my $adr = $d->addr();
      my $cc = $d->cc(); my $idx = $d->idx();
      printf("%X: %s ; ", $adr, $d->text());
      printf("%d ", $idx) if $idx;
      printf("cc %d", $cc) if defined($cc);
      my $ja = $d->bl_jimm();
      if ( $ja && exists($g_addr{ $ja }) ) {
          printf(" -> %s", $g_addr{ $ja }->[0] );
      }
      # find jxx
      my $j = $d->is_jxx();
      # push addr in queue if it was not processed yet
      if ( $j ) {
        if ( !$tree->in_tree($j) ) { push(@Q, [$j, $rp->clone()]) }
        else { printf(" [-]"); }
      }
      # adrp/add pairs
      my $pa = $d->apply($rp);
      if ( defined($pa) && $pa ) {
        if ( exists( $g_addr{$pa} ) ) {
          printf(" %s", $g_addr{ $pa }->[0] );
        } else {
          printf(" ? %X", $pa );
        }
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
my $scount = simple_symbols($e, \%g_syms, \%g_addr);
die("cannot find symbols") if ( !$scount ) ;
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