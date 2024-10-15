# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Elf-Reader.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 5;
BEGIN { use_ok('Elf::Reader') };

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.
my $e= Elf::Reader->new("/bin/ls");
ok( $e->get_class() == ELFCLASS64, 'class' );
ok( $e->type() == ET_DYN, 'type' );
ok( $e->flags() == 0, 'flags' );
ok( $e->entry() == 0x67d0, 'entry');
