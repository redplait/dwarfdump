#!perl -w
# script to conpare loclist offsets
# params: etalon file from objdump   cracked output from my dumper
use strict;
use warnings;

sub read_etalon
{
  my($fname, $hr) = @_;
  my($fh, $str, $name);
  my $off = 0;
  open($fh, '<', $fname) or die("cannot open $fname, error $!");
  while($str = <$fh>)
  {
    chomp $str;
    if ( $str =~ /<([^>]+)>.*\(index: 0x([0-9a-f]+)\): 0x([0-9a-f]+)/ )
    {
      my $loc = hex($3);
      $off = hex($1);
      $hr->{$off} = $loc;
      # printf("at %x %x\n", $off, $loc);
    }
  }
  close $fh;
}

sub read_susp
{
  my($fname, $hr) = @_;
  my($fh, $str, $name);
  my $off = 0;
  open($fh, '<', $fname) or die("cannot open $fname, error $!");
  while($str = <$fh>)
  {
    chomp $str;
    next if ( $str !~ /loclistx: ([0-9a-f]+), .* at ([0-9a-f]+)$/ );
    $off = hex($2);
    my $susp = hex($1);
    $hr->{$off} = $susp;
    # printf("at %x %x\n", $off, $susp);
  }
  close $fh;
}

sub cmp_locs
{
  my($a1, $a2) = @_;
  my $idx;
  while ( my ($off, $v) = each %$a2 )
  {
    if ( !exists $a1->{$off} )
    {
      printf("cannot find off %x\n", $off);
      next;
    }
    my $diff = $a1->{$off} - $v;
    next if !$diff;
    printf("%x diff %x: %x vs %x\n", $off, $diff, $a1->{$off}, $v);
  }
}

# main
if ( 2 != scalar @ARGV )
{
  printf("usage: nm.dump my.dump");
  exit(6);  
}
my(%a1, %a2);
read_etalon($ARGV[0], \%a1);
read_susp($ARGV[1], \%a2);
cmp_locs(\%a1, \%a2);
