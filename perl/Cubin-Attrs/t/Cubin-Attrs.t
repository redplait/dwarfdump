# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Cubin-Attrs.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;
use Elf::Reader;
use Data::Dumper;

use Test::More tests => 11;
BEGIN { use_ok('Cubin::Attrs') };

my $fname = '/home/redp/disc/src/cuda-ptx/src/denvdis/test/cv/libcvcuda.so.0.15.13.sm_70.cubin';
my $e = Elf::Reader->new($fname);
ok( defined($e), 'elf load');

my $fb = Cubin::Attrs->new($e);
ok( defined($fb), 'Cubin::Attrs');
ok( $fb->read(6), 'read attrs');
ok( 3 == $fb->params_cnt(), 'params count');
ok( 10 == $fb->count(), 'count' );

my($wide) = $fb->grep(0x31);
ok( defined $wide, 'grep on attr' );
# print STDERR Dumper($wide);
ok( exists $wide->{'id'}, 'has id' );
my $id = $wide->{'id'};
ok( $id, 'id' );
my $wlist = $fb->[$id];
ok( defined($wlist), 'wlist');
my $wl = $fb->value($id);
ok( defined($wl), 'value');
# print STDERR Dumper($wl);

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

