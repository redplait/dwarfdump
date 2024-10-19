# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Elf-Reader.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 16;
BEGIN { use_ok('Elf::Reader') };

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.
my $e= Elf::Reader->new("/bin/ls");
ok( $e->get_class() == ELFCLASS64, 'class' );
ok( $e->type() == ET_DYN, 'type' );
ok( $e->flags() == 0, 'flags' );
ok( $e->entry() == 0x67d0, 'entry');
# sections tests
my $secs = $e->secs();
ok( $secs, 'secs' );
ok( scalar( @$secs ) == 31, 'secs count' );
# segments tests
my $segs = $e->segs();
ok( $segs, 'segs' );
ok( scalar( @$segs ) == 14, 'segs count' );
my $phdr = $segs->[0];
ok( $phdr, 'phdr' );
ok( $phdr->[1] == PT_PHDR(), 'phdr type' );
# relocs tests
use_ok('Elf::Relocs');
ok( $Elf::Relocs::aarch64_rnames{20} eq 'R_AARCH64_P32_JUMP26' );
# versyms
my $vs = $e->versyms();
ok ( defined($vs), 'versyms');
ok ( scalar( @$vs ) == 2, 'versyms count');
ok ( $vs->[0]->[1] eq 'libc.so.6', 'veryms name' );
