#!perl -w
# test for riscv disasm binding
# 26 nov 2024 (c) redp
use strict;
use warnings;

use Elf::Reader;
use Disasm::Capstone;
use Disasm::Capstone::RiscV;
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
      printf("%d %d", $m->[0], $m->[1]);
    }
    printf("\n");
  }
}

sub is_call
{
  my $d = shift;
  my $op = $d->op();
  return 0 if ( $op != RISCV_JAL );
  # addr + imm
  return $d->addr() + $d->op_imm(0);
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
    if ( $next ) { $d->setup2( $adr, $next - $adr ); } 
    else { $d->setup( $adr ); }
#    $pad = $d->abi() if !defined($pad);
    while ( $d->disasm() ) {
      my $oc = $d->op_cnt();
      printf("%X: %s %s ; %d %d", $d->addr(), $d->mnem(), $d->text(), $d->op(), $d->ea());
      my $caddr = is_call($d); # check call
      if ( $caddr ) {
         my $sym = chsym($caddr);
        if ( defined $sym ) { printf(" %s\n", $sym ); }
        else { printf(" call %X\n", $caddr ); }
        next;
      }
      $caddr = $d->is_jxx(); # check jmps
      if ( defined($caddr) ) {
        my $p = $tree->in_tree($caddr);
        if ( !$p ) { push(@Q, [ $caddr, undef ] ); printf(" add_branch %X\n", $caddr); }
        else { printf(" [-]\n"); }
        next;
      }
      # dump details
      printf(" %d ops", $oc) if $oc;
      printf("\n");
      dump_ops($d, $oc) if $oc;
    }
    # add to tree
    $tree->insert( $adr, $d->addr() - $adr );
    printf("--\n");
  }
}

# main
if ( scalar( @ARGV) < 1 ) {
  printf("Usage: file system.map symbols ...\n");
  return 6;
}
my $e = Elf::Reader->new($ARGV[0]);
die("Cannot load $ARGV[0]") if ( !defined($e) ) ;
my $scount = simple_symbols($e, \%g_syms, \%g_addr);
die("cannot find symbols from $ARGV[1]") if ( !$scount ) ;
# make disasm
my $d = Disasm::Capstone::RiscV->new($e);
# process remaining symbols
shift @ARGV;
foreach ( @ARGV ) {
  my $addr;
  if ( !exists $g_syms{ $_ } )
  {
    # try to add _noprof suffix
    my $name = $_ . '_noprof';
    if ( !exists $g_syms{ $name } ) {
     printf("cannot find symbol %s\n", $_);
     next;
    } else {
      $addr = $g_syms{ $name };
    }
  } else {
    $addr = $g_syms{ $_ };
  }
  printf("%s:\n", $_);
  disasm_func($d, $addr);
}