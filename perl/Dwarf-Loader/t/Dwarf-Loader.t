# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Dwarf-Loader.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 7;
use Elf::Reader;
BEGIN { use_ok('Dwarf::Loader') };

my $e = Elf::Reader->new("/home/redp/disc/src/common/vmlinux");
ok( defined($e), 'elf load');

my $dw = Dwarf::Loader->new($e);
ok( defined($dw), 'new Dwarf::Loader' );

my $adr = $dw->by_addr(0x12C76C0);
ok ( defined($adr), 'by_addr');
ok ( $adr->type() == TSub, 'by_addr is sub');
my $atag = $adr->abs();
$atag = $adr->tag() if !defined($atag);

my $fn = $dw->by_name('slab_unmergeable');
ok ( defined($fn), 'by_name');
ok ( $fn->tag() == $atag, 'same tags');

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

