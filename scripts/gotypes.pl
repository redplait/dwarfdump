#!perl -w
# lame script to go custom atts stat
# 10 apr 2023 (c) redplait
use strict;
use warnings;
use Data::Dumper;

# key is go kind, and value is array of
# index 0 - count of tags 
# index 1 - hash for form names -> form count
my %g_kinds;

sub parse
{
  my($fname, $db) = @_;
  my($fh, $str, $tag, $form, $kind);
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
        next;
      }
      # unknown AT value
      if ( $str =~ /^\s+<[0-9a-f]+>\s+Unknown AT value: (\d+):\s+(\d+)$/ )
      {
        $kind = int($1);
        if ( 2900 == $kind )
        {
          $kind = int($2);
          if ( exists $g_kinds{$kind} )
          {
            $g_kinds{$kind}->[0]++;
            my $t = $g_kinds{$kind}->[1];
            $t->{$tag}++;
          } else {
            my %tmp = ( $tag => 1);
            $g_kinds{$kind} = [1, \%tmp ];
          }
        }
        next;
      }
    }
  }
  close $fh;  
}

sub dump_kinds
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
printf("--- kinds:\n");
dump_kinds(\%g_kinds);
