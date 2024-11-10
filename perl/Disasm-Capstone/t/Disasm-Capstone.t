# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Disasm-Capstone.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 5;
BEGIN { use_ok('Disasm::Capstone') };
BEGIN { use_ok('Disasm::Capstone::PPC') };

use Elf::Reader;
# load ppc kernel
my $e = Elf::Reader->new("/home/redp/disc/src/ppc/vmlinuz-5.11.12-300.fc34.ppc64le");
ok( defined($e), 'elf load');

my $d = Disasm::Capstone::PPC->new($e);
ok( defined($d) && ref($d), 'new ppc');

my $sres = $d->setup(0xC0000000003FD020);
ok( $sres, 'setup');


#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

