#!perl -w
# script to estimate size overhead due types duplication for c++
# 1 apr 2023 (c) redplait
use strict;
use warnings;

# key is string file_string_column
# value is array where
# idx 0 - name
#     1 - id if name is missed
#     2 - overhead size
my %gdb;

# namespaces stack
my @g_stack;

sub make_key
{
  my($name, $s, $c) = @_;
  my $fname = $name;
  $fname = join('::', @g_stack) . $name if ( scalar @g_stack );
  return sprintf("%s_%d_%d", $fname, $s, $c);
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
  my $was_ns = 0;
  my $process_attr = 0;
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
      # Abbrev Number: 0
      if ( $str =~ / <(\d+)><([a-f0-9]+)>: Abbrev Number: 0/ )
      {
        my $level = int($1);
        my $stack_size = scalar(@g_stack);
        if ( $level == 1 + $stack_size )
        {
          my $name = pop @g_stack;
          # printf("pop ns %s\n", $name);
        }
        next;
      }
      # process only top level tags
      if ( $str =~ / <(\d+)><([a-f0-9]+)>.*: Abbrev Number: .* \((.*)\)/ )
      {
        $tag = $3;
        my $level = int($1);
        $process_attr = 0;
        next if ( $level != 1 + scalar(@g_stack) );
        $process_attr = 1;
        $id = hex($2);
        # check previous
# printf("%X: tag %s $f $s $c\n", $id, $tag);
        if ( !$was_ns && defined($name) && $s && $c )
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
        }
        undef $name;
        $sib = 0;
        if ( $tag eq 'DW_TAG_namespace' )
        {
    # printf("%X ns %d\n", $id, scalar @g_stack);
          $was_ns = 1;
          next;
        }
        $was_ns = 0;
        # skip inlined subs
        if ( $tag eq 'DW_TAG_inlined_subroutine' ||
             $tag eq 'DW_TAG_imported_module' 
           )
        {
          $s = $c = 0;
          next;
        }
        next;
      }
      if ( $process_attr && $str =~ /^\s+<[0-9a-f]+>\s+DW_AT_(\S+)\s*:\s*(.*)$/ )
      {
        my $attr = $1;
        my $rest = $2;
    # printf("attr %s $2\n", $attr);    
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
          # add namespaces only if not-empty - so it should have sibling attr
          if ( $was_ns )
          {
            my $ns_name = defined($name) ? $name : "unnamed";
            # printf("%d ns %s\n", scalar @g_stack, $ns_name);
            push @g_stack, $ns_name;
          }
          next;
        }
        # name
        if ( $attr eq 'name' )
        {
          if ( $rest =~ /\(indirect string, .*\): (.*)$/ )
          {
            $name = $1;
          } else {
            $name = $rest;
          }
          # printf("%X name %s\n", $id, $name);
          next;
        }
      }
    }
  }
  # process last type
  if ( !$was_ns && defined($name) && $s && $c )
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
  # dump top-10 types
  my $i = 0;
  foreach my $e ( sort { $b->[2] <=> $a->[2] } values %gdb )
  {
    last if ( ++$i > 10 );
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