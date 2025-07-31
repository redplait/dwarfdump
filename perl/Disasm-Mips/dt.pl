#!perl -w
# test for mips disasm binding
# 18 oct 2024 (c) redp
# 25 oct 2024 - add regpads, is_ls & apply
use strict;
use warnings;

use Elf::Reader;
use Disasm::Mips;
use Interval::Tree;

# key - name, value - [ address, size ]
my %g_syms;

# key - addr, value - [ name, type ]
my %g_addr;

sub is_li
{
  my $d = shift;
  return 0 if ( $d->op() != LI );
  return 0 if ( $d->op_class(1) != IMM );
  return $d->op_imm(1);
}

sub disasm_func
{
  my($d, $ar) = @_;
  # queue of addresses to process, [addr regpad]
  my @Q;
  my $irp = $d->regpad(); $irp->abi();
  my $tree = Interval::Tree->new();
  push @Q, [ $ar->[0], $irp ];
  while ( @Q )
  {
    my($adr, $rp) = @{ shift @Q };
    next if ( $tree->in_tree($adr) );
    my $next = $tree->next($adr);
    if ( $next ) {
      $d->setup2( $adr, $next - $adr );
    } else {
      $d->setup( $adr );
    }
    while ( my $len = $d->disasm() )
    {
      my $adr = $d->addr();
      printf("%X: %s", $adr, $d->text());
      my $ja = $d->is_jal();
      my $li = is_li($d);
      if ( $ja && exists($g_addr{ $ja }) ) {
        printf(" ; %s", $g_addr{ $ja }->[0] );
      } elsif ( $li && exists($g_addr{ $li }) ) {
        printf(" ; %s", $g_addr{ $li }->[0] );
      }
      # find jxx
      my $j = $d->is_jxx();
      # push addr in queue if it was not processed yet
      if ( $j ) {
        my $p = $tree->in_tree($j);
        if ( !$p ) { push(@Q, [ $j, $rp->clone() ]); }
        else { printf(" ; [-]"); }
        goto out;
      }
      my $ld = $d->is_ls($rp);
      if ( defined($ld) && $ld->[1] >= 0 )
      {
        printf(" ; %s %d %X\n", $ld->[0] == 1 ? "ld" : "st", $ld->[1], $ld->[2]);
        next;
      }
      my $r = $d->apply($rp);
      if ( defined($r) && $r ) {
        if ( exists($g_addr{ $r }) ) { printf(" ; %s", $g_addr{ $r }->[0] ); }
        else { printf(" ; %X", $r); }
      }
out:
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