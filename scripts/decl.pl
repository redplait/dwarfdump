#!perl -w
# lame script to find dwarf tags with attribute declaration
# 2 mar 2023 (c) redplait
use strict;
use warnings;

my %g_tags;
my %g_arts;
my %g_specs;
my %g_cont;
my %g_defs;
my %g_abs;
my %g_inl;
my %g_defa;

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
      if ( $str =~ /DW_AT_containing_type:/ )
      {
        $g_cont{$tag}++;
        next;
      }  
      if ( $str =~ / DW_AT_defaulted / )
      {
        $g_defs{$tag}++;
        next;
      }  
      if ( $str =~ /DW_AT_abstract_origin:/ )
      {
        $g_abs{$tag}++;
        next;
      }  
      if ( $str =~ / DW_AT_inline\s+:/ )
      {
        $g_inl{$tag}++;
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
printf("--- containing types:\n");
dump_tags(\%g_cont);
printf("--- defaulted:\n");
dump_tags(\%g_defs);
printf("--- abstract origins:\n");
dump_tags(\%g_abs);
printf("--- inlined:\n");
dump_tags(\%g_inl);
