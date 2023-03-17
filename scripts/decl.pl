#!perl -w
# lame script to find dwarf tags with attribute declaration
# 2 mar 2023 (c) redplait
use strict;
use warnings;

my %g_tags;
my %g_arts;
my %g_specs;

sub parse
{
  my $fname = shift;
  my($fh, $str, $tag);
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
      if ( $str =~ /DW_AT_declaration :/ )
      {
        $g_tags{$tag}++;
        next;
      }  
      if ( $str =~ /DW_AT_artificial  :/ )
      {
        $g_arts{$tag}++;
        next;
      }  
      if ( $str =~ /DW_AT_specification:/ )
      {
        $g_specs{$tag}++;
        next;
      }  
    }
  }
  close $fh;  
}

sub dump_tags
{
  my $tags = shift;
  foreach my $tag ( keys %$tags )
  {
    printf("%s: %d\n", $tag, $tags->{$tag});
  }  
}

# main
foreach my $a ( @ARGV )
{
  parse($a);
}
printf("--- declaration:\n");
dump_tags(\%g_tags);
printf("--- artificial:\n");
dump_tags(\%g_arts);
printf("--- specifications:\n");
dump_tags(\%g_specs);
