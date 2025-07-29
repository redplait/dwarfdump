# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Elf-FatBinary.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;
use Elf::Reader;
use Test::More tests => 3;
BEGIN { use_ok('Elf::FatBinary') };
my $fname = '/home/redp/disc/src/cuda-ptx/src/cuda_latency_benchmark/cuda_task_queue.cpython-38-x86_64-linux-gnu.so';
#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

my $e = Elf::Reader->new($fname);
ok( defined($e), 'elf load');

my $fb = Elf::FatBinary->new($e, $fname);
ok( defined($e), 'fat binary');
