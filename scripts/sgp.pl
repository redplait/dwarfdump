#!perl -w
# test script to select functions reffered to some specific fields or vmethods
# cmd options: path2sqlite.db pattern1 pattern2 ...
# 23 sep 2024 (c) redplait
use strict;
use warnings;
use DBI;
use DBI qw(:sql_types);
use DBD::SQLite::Constants qw/:file_open/;

# check args
my $alen = scalar(@ARGV);
if ( $alen < 3 ) {
  print STDERR<<EOF;
Usage: $0 path2sqlite.db type pattern1 ...
 where tyoe is
  c for call
  v for vcall
  f for field
  x for xref
EOF
  exit(8);
}

# open sqlite db
my $cstr = "dbi:SQLite:dbname=" . $ARGV[0];
my $dbh = DBI->connect($cstr, undef, undef, {
  RaiseError => 1,
  sqlite_open_flags => SQLITE_OPEN_READONLY,
});

my $letter = $ARGV[1];

# prepare select stmts
my $sth = $dbh->prepare("select id, bcount, name from symtab where symtab.id in (" .
 "select id from xrefs where kind = '$letter' and xrefs.what in (" .
 "select id from symtab where name like (?) ) ) order by bcount");

my $det = $dbh->prepare("select symtab.name, xrefs.arg from symtab, xrefs where " .
 "xrefs.id = (?) and symtab.id = xrefs.what and xrefs.kind = '$letter' and xrefs.what in (" .
 "select id from symtab where name like (?) )");

for ( my $i = 2; $i < $alen; $i++ )
{
  my $what = $ARGV[$i] . '%';
  $sth->bind_param(1, $what, SQL_VARCHAR);
  printf("%s:\n", $ARGV[$i]);
  $sth->execute();
  while (my @row_array = $sth->fetchrow_array) {
    print $row_array[1], " is ", $row_array[2], "\n";
    # 1 - function id from sth, 2 - what
    $det->bind_param(1, $row_array[0], SQL_INTEGER);
    $det->bind_param(2, $what, SQL_VARCHAR);
    $det->execute();
    while(my @det_array = $det->fetchrow_array) {
      print " [", $det_array[1], "] ", $det_array[0], "\n";
    }
  }
}