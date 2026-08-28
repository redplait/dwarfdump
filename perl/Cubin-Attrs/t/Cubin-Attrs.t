# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Cubin-Attrs.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;
use Elf::Reader;
use Data::Dumper;

use Test::More tests => 23;
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
ok( 0x1f == $a206->[0], 'regcount for sym 206');

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
my @cres = $fb->collect();
ok( defined($cres[0]), 'first collect');
ok( !defined($cres[1]), 'second collect');
my $link = $fb->link();
ok( 224 == $link, 'link test');
my %rels;
# this cubin don't have relocs so both read_rel & read_rela should return 0
ok( !$fb->read_rel($e, $link, \%rels), 'read_rel');
ok( !$fb->read_rela($e, $link, \%rels), 'read_rela');
ok( !defined($fb->mbars()), 'mbars');

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

