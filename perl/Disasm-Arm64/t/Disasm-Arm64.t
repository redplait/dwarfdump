# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Disasm-Arm64.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 12;
use Elf::Reader;
BEGIN { use_ok('Disasm::Arm64') };

# load mips kernel
my $e = Elf::Reader->new("/home/redp/disc/src/linux-6.8.8/vmlinux");
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
my $ad = Disasm::Arm64->new($e);
ok ( defined($ad), 'new' );

# setup
ok( $ad->setup($addr), 'setup' );
ok( $ad->disasm(), 'disasm' );
my $op = $ad->op();
ok ( $op == PACIASP, 'first op' );
ok( $ad->disasm(), 'disasm 2' );
$op = $ad->op();
ok ( $op == STP, 'stp' );
ok ( $ad->op_num() == 4, 'stp args num' );

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

