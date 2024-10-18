# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Disasm-Mips.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 14;
use Elf::Reader;

BEGIN { use_ok('Disasm::Mips') };
ok( XORI == 423, 'xori');

# load mips kernel
my $e = Elf::Reader->new("/home/redp/disc/src/common/vmlinux");
ok( defined($e), 'elf load');

# get address of slab_unmergeable
my $s = $e->secs();
my $sec;
foreach ( @$s ) {
 if ( $_->[2] == Elf::Reader::SHT_SYMTAB ) {
   $sec = $_->[0];
   last;
 }
}
ok( defined($sec), 'found symbols');

# find symbol address
my $syms = $e->syms($sec);
ok( defined($syms), 'load symbols');
my $addr;
foreach ( @$syms ) {
  if ( $_->[0] eq 'slab_unmergeable' ) {
    $addr = $_->[1];
    last;
  }
}
ok( defined($addr), 'symbol found');

# make new disasm object
my $dis = Disasm::Mips->new($e);
ok( defined($dis), 'new Disasm::Mips' );
ok( $dis->setup($addr), 'setup' );
my $len = $dis->disasm();
ok( $len == 4, 'disasm' );
my $op = $dis->op();
ok ( $op == ADDIU, 'addiu' );
my $c0 = $dis->op_class(0);
ok ( $c0 == REG, 'class0' );
my $c1 = $dis->op_class(1);
ok ( $c1 == REG, 'class1' );
my $c2 = $dis->op_class(2);
ok ( $c2 == IMM, 'class2' );
ok ( Disasm::Mips::reg_name( $dis->op_reg(0) ) eq '$sp' );

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

