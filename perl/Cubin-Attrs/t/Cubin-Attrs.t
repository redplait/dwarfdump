# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Cubin-Attrs.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;
use Elf::Reader;
use Data::Dumper;

use Test::More tests => 26;
BEGIN { use_ok('Cubin::Attrs') };

my $fname = '/home/redp/disc/src/cuda-ptx/src/denvdis/test/cv/libcvcuda.so.0.15.13.sm_70.cubin';
my $e = Elf::Reader->new($fname);
ok( defined($e), 'elf load');

my $fb = Cubin::Attrs->new($e);
ok( defined($fb), 'Cubin::Attrs');
# sym attrs
my $nv = Cubin::Attrs::nv_info($e);
ok( defined($nv), 'nv_info');
my $ah = $fb->get_sym_attrs($nv);
ok( defined($ah), 'get_sym_attrs' );
ok( 'HASH' eq ref $ah, 'get_sym_attrs returned hash');
ok( exists($ah->{0x206}), 'get_sym_attrs has sym 206');
my $a206 = $ah->{0x206};
ok( 'ARRAY' eq ref $a206, 'get_sym_attrs is array ref');
is( $a206->[0], 0x1f, 'regcount for sym 206');
# test grep_sym_pair
my @tags = ( Cubin::Attrs::MIN_STACK_SIZE, Cubin::Attrs::MAX_STACK_SIZE, Cubin::Attrs::REGCOUNT);
my $sr = $fb->grep_sym_pair(530, \@tags);
ok( defined($sr), 'grep_sym_pair');
ok( exists($sr->{0x2f}), 'grep_sym_pair regcount exists');
is( $sr->{0x2f}->[1], 4, 'grep_sym_pair regcount');

ok( $fb->read(6), 'read attrs');
is( $fb->params_cnt(), 3, 'params count');
is( $fb->count(), 10, 'count' );

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
my @cres = $fb->collect();
ok( defined($cres[0]), 'first collect');
ok( !defined($cres[1]), 'second collect');
my $link = $fb->link();
is( $link, 224, 'link test');
my %rels;
# this cubin don't have relocs so both read_rel & read_rela should return 0
ok( !$fb->read_rel($e, $link, \%rels), 'read_rel');
ok( !$fb->read_rela($e, $link, \%rels), 'read_rela');
ok( !defined($fb->mbars()), 'mbars');

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

