# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Elf-FatBinary.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;
use Elf::Reader;
use Test::More tests => 8;
BEGIN { use_ok('Elf::FatBinary') };
my $fname = '/home/redp/disc/src/cuda-ptx/src/cuda_latency_benchmark/cuda_task_queue.cpython-38-x86_64-linux-gnu.so';
#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

my $e = Elf::Reader->new($fname);
ok( defined($e), 'elf load');

my $fb = Elf::FatBinary->new($e, $fname);
ok( defined($fb), 'fat binary');

ok( $fb->read(), 'load fat binary');
ok( 1 == $fb->count(), 'count');
# extract 0 entry
my $i0 = $fb->[0];
ok( defined($i0), 'entry 0');
ok($i0->{'kind'} == 2, 'cubin');
ok($i0->{'flags'} == 0x11, 'flags');