#!perl -w
# lame script to calc stats about dwarf tags
# 22 mar 2023 (c) redplait
use strict;
use warnings;
use Data::Dumper;

# db is hash where key is tag name and value is array of
# index 0 - count of tags 
# index 1 - hash for form names -> form count
sub parse
{
  my($fname, $db) = @_;
  my($fh, $str, $tag, $form);
  open($fh, '<', $fname) or die("cannot open $fname, error $!\n");
  my $state = 0;
  while( $str = <$fh> )
  {
    chomp $str;
    if ( !$state )
    {
      $state = 1 if ( $str =~ /Compilation Unit @ offset/ );
      next;
    }
    if ( 1 == $state )
    {
      if ( $str =~ / <\d+>.*: Abbrev Number: .* \((.*)\)/ )
      {
        $tag = $1;
        if ( exists $db->{$tag} )
        {
          $db->{$tag}->[0]++;
        } else {
          my %tmp;
          $db->{$tag} = [ 1, \%tmp ];
        }
        next;
      }
      if ( $str =~ /^\s+<[0-9a-f]+>\s+DW_AT_(\S+)\s*:/ )
      {
        $form = $1;
        my $f = $db->{$tag}->[1];
        $f->{$form}++;
        next;
      }
      # unknown AT value
      if ( $str =~ /^\s+<[0-9a-f]+>\s+Unknown AT value: (\d+):/ )
      {
        $form = "unk_" . $1;
        my $f = $db->{$tag}->[1];
        $f->{$form}++;
        next;
      }
    }
  }
  close $fh;
}

sub dump_tags
{
  my $tags = shift;
  foreach my $tag ( sort { $tags->{$b}->[0] <=> $tags->{$a}->[0] } keys %$tags )
  {
    printf("%s: %d\n", $tag, $tags->{$tag}->[0]);
  }
}

sub dump_forms
{
  my $tags = shift;
  my %fdb;
  foreach my $tag ( keys %$tags )
  {
    my $f = $tags->{$tag}->[1];
    # print Data::Dumper->Dump([$f]);
    foreach my $form ( keys %$f )
    {
      # printf("form %s\n", $form);
      if ( exists $fdb{$form} )
      {
        $fdb{$form}->[0] += $f->{$form};
        $fdb{$form}->[1]->{$tag} += $f->{$form};
      } else {
         $fdb{$form} = [ $f->{$form}, { $tag => $f->{$form} } ];
      }
    }
  }
  foreach my $f ( sort { $fdb{$b}->[0] <=> $fdb{$a}->[0] } keys %fdb )
  {
    printf("%s: %d\n", $f, $fdb{$f}->[0]);
    my $tdb = $fdb{$f}->[1];
    foreach my $t ( sort { $tdb->{$b} <=> $tdb->{$a} } keys %$tdb )
    {
      printf("  %s: %d\n", $t, $tdb->{$t})
    } 
  }
}

# main
my %db;
foreach my $a ( @ARGV )
{
  parse($a, \%db);
}
printf("--- tags:\n");
dump_tags(\%db);
printf("--- forms:\n");
dump_forms(\%db);
