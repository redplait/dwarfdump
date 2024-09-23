#!perl -w
# script to estimate size overhead due types duplication
# 31 mar 2023 (c) redplait
use strict;
use warnings;

# key is string file_string_column
# value is array where
# idx 0 - name
#     1 - id if name is missed
#     2 - overhead size
my %gdb;

sub make_key
{
  my($name, $s, $c) = @_;
  return sprintf("%s_%d_%d", $name, $s, $c);
}

sub parse
{
  my $fname = shift;
  my($fh, $str, $tag, $form);
  open($fh, '<', $fname) or die("cannot open $fname, error $!\n");
  my $state = 0;
  my $s = 0;
  my $c = 0;
  my $curr = 0;
  my $sib = 0;
  my $name;
  my $id = 0;
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
      # process only top level tags
      if ( $str =~ / <1><([a-f0-9]+)>.*: Abbrev Number: .* \((.*)\)/ )
      {
        my $tag = $2;
        $id = hex($1);
        # check previous
# printf("%X: tag %s $f $s $c\n", $id, $tag);
        if ( defined($name) && $s && $c )
        {
          my $len = $id - $curr;
          $len = $sib - $curr if ( $sib && $sib > $id );
          my $key = make_key($name, $s, $c);
          if ( exists $gdb{$key} )
          {
            printf("id %X\n", $id) if ( $len < 0);
            $gdb{$key}->[2] += $len;
          } else {
            $gdb{$key} = [ $name, $id, 0 ];
          }
          $curr = $id;
          undef $name;
          $sib = 0;
          # skip inlined subs
          if ( $tag eq 'DW_TAG_inlined_subroutine' )
          {
            $s = $c = 0;
            next;
          }
          next;
        }
      }
      if ( $str =~ /^\s+<[0-9a-f]+>\s+DW_AT_(\S+)\s*:\s*(.*)$/ )
      {
        my $attr = $1;
        my $rest = $2;
  #  printf("attr %s $2\n", $attr);
        # gather line and column
        if ( $attr eq 'decl_line' )
        {
          my $v = $2;
          if ( $v =~ /^0x/ )
          {
            $s = hex($v);
          } else {
            $s = int($v);
          }
          next;
        }
        if ( $attr eq 'decl_column' )
        {
          $c = int($2);
          next;
        }
        # sibling
        if ( $attr eq 'sibling' )
        {
          $rest =~ s/^<//;
          $rest =~ s/>$//;
          $sib = hex($rest);
          next;
        }
        # name
        if ( $attr eq 'name' )
        {
          if ( $rest =~ /: ([^:]+)$/ )
          {
            $name = $1;
          } else {
            $name = $rest;
          }
          next;
        }
      }
    }
  }
  # process last type
  if ( defined($name) && $s && $c )
  {
    my $len = $id - $curr;
    $len = $sib - $curr if ( $sib );
    my $key = make_key($name, $s, $c);
    $gdb{$key}->[2] += $len if ( exists $gdb{$key} );
  }
  close $fh;
}

sub calc_est
{
  my $res = 0;
  while ( my ($key, $value) = each %gdb )
  {
    $res += $value->[2];
  }
  printf("%d\n", $res);
  # dump top-5 types
  my $i = 0;
  foreach my $e ( sort { $b->[2] <=> $a->[2] } values %gdb )
  {
    last if ( ++$i > 5 );
    if ( defined($e->[0]) )
    {
      printf(" %s %s\n", $e->[0], $e->[2]);
    } else {
      printf(" %d id %X\n", $e->[0], $e->[1]);
    }
  }
}

# main
foreach my $a ( @ARGV )
{
  parse($a);
}
calc_est();