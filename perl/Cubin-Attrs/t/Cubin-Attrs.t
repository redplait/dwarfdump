# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Cubin-Attrs.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;
use Elf::Reader;

use Test::More tests => 3;
BEGIN { use_ok('Cubin::Attrs') };

my $fname = '/home/redp/disc/src/cuda-ptx/src/denvdis/test/cv/libcvcuda.so.0.15.13.sm_70.cubin';
my $e = Elf::Reader->new($fname);
ok( defined($e), 'elf load');

my $fb = Cubin::Attrs->new($e, 6);
ok( defined($fb), 'Cubin::Attrs');

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

