#!perl -w
# test for mips disasm binding
# 18 oct 2024 (c) redp
# 25 oct 2024 - add regpads, is_ls & apply
use strict;
use warnings;

use Elf::Reader;
use Disasm::Capstone;
use Disasm::Capstone::PPC;
use Interval::Tree;

# key - name, value - [ address, size ]
my %g_syms;

# key - addr, value - [ name, type ]
my %g_addr;

sub chsym
{
  my $addr = shift;
  return $g_addr{ $addr }->[0] if exists $g_addr{ $addr };
  # skip 8 bytes for TOC loading
  $addr -= 8;
  return $g_addr{ $addr }->[0] if exists $g_addr{ $addr };
  undef;
}

sub dump_ops
{
  my($d, $oc) = @_;
  for ( my $i = 0; $i < $oc; $i++ ) {
    my $t = $d->op_type($i);
    printf("; %d %d %d ", $i, $t, $d->op_access($i));
    if ( $t == CS_OP_REG ) {
      printf("%d", $d->op_reg($i));
    } elsif ( $t == CS_OP_IMM ) {
      printf("%X", $d->op_imm($i));
    } elsif ( $t == CS_OP_MEM ) {
      my $m = $d->op_mem($i);
      printf("%d %d %d", $m->[0], $m->[1], $m->[2]);
    }
    printf("\n");
  }
}

sub disasm_func
{
  my($d, $ar) = @_;
  # branches
  my @Q;
  my $tree = Interval::Tree->new();
  push @Q, [ $ar->[0], undef ];
  while ( @Q )
  {
    my($adr, $pad) = @{ shift @Q };
    next if ( $tree->in_tree($adr) );
    my $next = $tree->next($adr);
    if ( $next ) {
      $d->setup2( $adr, $next - $adr );
    } else {
      $d->setup( $adr );
    }
    $pad = $d->abi() if !defined($pad);
    while ( $d->disasm() ) {
      my $oc = $d->op_cnt();
      printf("%X: %s %s ; %d %d ops", $d->addr(), $d->mnem(), $d->text(), $d->op(), $oc);
      my $caddr = is_call($d);
      if ( $caddr ) {
        my $sym = chsym($caddr);
        if ( defined $sym ) {
          printf(" %s\n", $sym ); next;
        }
      }
      $caddr = is_bimm($d);
      if ( $caddr ) {
        my $p = $tree->in_tree($caddr);
        if ( !$p ) { push(@Q, [ $caddr, $pad->clone() ] ); printf(" add_branch\n"); }
        else { printf(" ; [-]\n"); }
        next;
      }
      $caddr = $d->apply($pad);
      if ( $caddr ) {
        my $sym = chsym($caddr);
        if ( defined $sym ) { printf(" %s\n", $sym ); }
        else { printf(" %X\n", $caddr ); }
        next;
      }
      # dump details
      printf("\n");
      dump_ops($d, $oc) if ( $oc );
    }
    # add to tree
    $tree->insert( $adr, $d->addr() - $adr );
    printf("--\n");
  }
}

# main
if ( scalar( @ARGV) < 2 ) {
  printf("Usage: file system.map symbols ...\n");
  return 6;
}
my $e = Elf::Reader->new($ARGV[0]);
die("Cannot load $ARGV[0]") if ( !defined($e) ) ;
my $scount = Elf::Reader::parse_system_map($ARGV[1], \%g_syms, \%g_addr);
die("cannot find symbols") if ( !$scount ) ;
# make disasm
my $d = Disasm::Capstone::PPC->new($e);
# process remaining symbols
shift @ARGV; shift @ARGV;
foreach ( @ARGV ) {
  if ( !exists $g_syms{ $_ } ) {
    printf("cannot find symbol %s\n", $_);
    next;
  }
  printf("%s:\n", $_);
  disasm_func($d, $g_syms{ $_ });
}