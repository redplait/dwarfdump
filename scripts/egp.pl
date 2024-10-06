#!perl -w
# script to extract errors from gptest db
# cmd options: path2sqlite.db
# before fly install
#  cpan -i DBI
#  cpan -i DBD::SQLite
# 6 oct 2024 (c) redplait
use strict;
use warnings;
use DBI;
use DBI qw(:sql_types);
use DBD::SQLite::Constants qw/:file_open/;

# check args
my $alen = scalar(@ARGV);
if ( $alen != 1 ) {
  print STDERR "Usage: $0 path2sqlite.db\n";
  exit(8);
}

# open sqlite db
my $cstr = "dbi:SQLite:dbname=" . $ARGV[0];
my $dbh = DBI->connect($cstr, undef, undef, {
  RaiseError => 1,
  sqlite_open_flags => SQLITE_OPEN_READONLY,
});

# prepare select stmts
my $sth = $dbh->prepare("select symtab.fname, symtab.name, errlog.msg from symtab, errlog where symtab.id = errlog.id");

$sth->execute();
while (my @row_array = $sth->fetchrow_array) {
  print $row_array[0], " ", $row_array[1], " ", $row_array[2], "\n";
}