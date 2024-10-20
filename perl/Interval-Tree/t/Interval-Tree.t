# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Interval-Tree.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 7;
BEGIN { use_ok('Interval::Tree') };

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.
my $t = Interval::Tree->new();
ok( defined $t, 'new');
$t->insert(14, 4);
ok( $t->in_tree(15), 'in_tree');
ok( !$t->in_tree(18), 'not in_tree');
my $n1 = $t->next(16);
ok ( !defined $n1, 'no next');
$n1 = $t->next(12);
ok( $n1 == 14, 'next14');
$n1 = $t->next(19);
ok( $n1 == 0, 'no right');
