#!perl -w
# PoC for maximal clique size estimation algo
# 21 may 2023 (c) redplait
use strict;
use warnings;
use Storable;
use Getopt::Std;
# to calc graph variance
use Statistics::Basic qw(:all);
# for debug only
use Data::Dumper;

use vars qw/$opt_c $opt_d $opt_g $opt_v $opt_s/;

sub usage()
{
  print STDERR<<EOF;
Usage: $0 [options] vertexes_num [edges_num] [additional edges for 1st vertex]
Options:
 -c -- make connected graph
 -d -- debug dump
 -g N - add clique with size N
 -s filename -- load/store graph from file
 -v -- verbose mode
EOF
  exit(8);
}

#
# graph generation functions
# graph is just hashmap where key is node number and value is another hashmap with edges
#
sub get_rand
{
  my $vc = shift;
  my $v = rand($vc);
  return POSIX::floor($v);  
}

sub get_next
{
  my($v, $vc) = @_;
  my $v2 = get_rand($vc);
  # no loops to the same vertex
  while( $v2 == $v )
  {
    $v2 = get_rand($vc);
  }
  return $v2;
}

sub connected
{
  my($g, $from, $to) = @_;
  return 0 if ( !exists $g->{$from} );
  my $e = $g->{$from};
  return 1 if ( exists $e->{$to} );
  return 0;  
}

sub add_edge
{
  my($g, $v, $v2) = @_;
  $g->{$v}->{$v2} = 1;
  $g->{$v2}->{$v} = 1;
}

# add to graph g clique with size csize starting from 0 vertex
sub add_clique
{
  my($g, $csize) = @_;
  for ( my $i = 0; $i < $csize; $i++ )
  {
    for ( my $j = $i + 1; $j < $csize; $j++ )
    {
      next if ( connected($g, $i, $j) );
      add_edge($g, $i, $j);
    }
  }
}

# probably we should try some models from https://www.researchgate.net/publication/287544803_Random_graphs_models_and_generators_of_scale-free_graphs/fulltext/5677848d08ae502c99d2fbd8/Random-graphs-models-and-generators-of-scale-free-graphs.pdf
sub gen_random_graph
{
  my($vc, $ec) = @_;
  printf("vc %d ec %d\n", $vc, $ec) if ( defined $opt_v );
  my %res;
  my($vi, $ei);
  for ( $vi = 0; $vi < $vc; $vi++ )
  {
    $res{$vi} = {};
  }
  for ( $ei = 0; $ei < $ec; $ei++ )
  {
    my $v = get_rand($vc);
    my $v2 = get_next($v, $vc);
    for ( $vi = 0; $vi < $vc; $vi++ )
    {
      next if ( exists $res{$v}->{$v2} );
      add_edge(\%res, $v, $v2);
      last;
    }
  }
  return \%res;
}

# add boost additional edges to vi node
sub add_boost1
{
  my($g, $vi, $vc, $boost) = @_;
  for ( my $i = 0; $i < $boost; $i++ )
  {
    my $v2 = get_next($vi, $vc);
    next if ( exists $g->{$vi}->{$v2} );
    add_edge($g, $vi, $v2);
  }
}

sub add_boost
{
  my($g, $vc, $boost) = @_;
  for ( my $i = 0; $i < $vc; $i++ )
  {
    add_boost1($g, $i, $vc, $boost);
    # for next node reduce boost
    $boost /= 3;
    $boost *= 2;
    last if ( $boost < 2 );
  }
}

sub connect_rem
{
  my($g, $vc) = @_;
  my $res = 0;
  while (my ($key, $value) = each %$g )
  {
    my $len = scalar( keys %$value);
    next if ( $len );
    add_boost1($g, $key, $vc, 2);
    $res++;
  }
  return $res;
}

sub calc_variance
{
  my $g = shift;
  my $n = 0;
  my $total = 0;
  my $v = vector();
  while (my ($key, $value) = each %$g )
  {
    $n++;
    my $len = scalar( keys %$value);
    $total += $len;
    $v->append($len);
  }
  $total /= 2;
  printf("total %d vertexes %d edges, avg degree %f\n", $n, $total, $total / $n);
  printf("variance %f\n", variance($v) );
  printf("median %f\n", median($v) );
  printf("stddev %f\n", stddev($v) );
}

#
# main algo
#
sub add_vertexes
{
  my($g, $curr) = @_;
  while (my ($key, $value) = each %$curr )
  {
    $g->{$key} = $value;
  }
}

# result - ref to array where each item is array ref with indexes
# 0 - vertex degree - array sorted on this field
# 1 - hashmap with vertexes
sub make_sorted_array
{
  my $g = shift; # graph  
  my(%tmp, $key, $value);
  while (($key, $value) = each %$g )
  {
    my $len = scalar( keys %$value);
    if ( exists $tmp{$len} )
    {
      $tmp{$len}->{$key} = $value;
    } else {
      $tmp{$len} = { $key => $value };  
    }
  }
  my @arr;
  while (($key, $value) = each %tmp )
  {
    push @arr, [$key, $value];
  }
  # print Dumper(\@arr);
  # sort
  my @res = sort { $b->[0] <=> $a->[0] } @arr;
  return \@res;     
}

sub dump_arr
{
  my $arr = shift;
  foreach my $i ( @$arr )
  {
    my $len = scalar( keys %{ $i->[1] } );
    printf("%d %d\n", $i->[0], $len);
  }  
}

sub compact
{
  my $g = shift;
  # make new graph with removed edges not in g
  my %pruned;
  my $pruned_cnt = 0;
  while (my ($key, $value) = each %$g )
  {
    my $ec = 0;
    my %tmp_edges;
    foreach my $e ( keys %$value )
    {
      next if ( !exists $g->{$e} );
      $tmp_edges{$e} = 1;
      $ec++;  
    }
    # remove vertex if it does not contain edges to g
    next if ( !$ec );
    $pruned{$key} = \%tmp_edges;
    $pruned_cnt++; 
  }
  # temp graph can be empty after edges removing
  return 0 if ( !$pruned_cnt );
  my $arr = make_sorted_array(\%pruned);
  if ( defined $opt_v )
  {
    printf("pruned_cnt %d:\n", $pruned_cnt);
    dump_arr($arr);
  }
  my $total = 0;
  foreach my $i ( @$arr )
  {
    $total += scalar( keys %{ $i->[1] } );
    next if ( $total < $i->[0] );
    return $i->[0];
  }
  return undef;
}

# from https://cs.stackexchange.com/questions/11360/size-of-maximum-clique-given-a-fixed-amount-of-edges
# calc first count of edges
sub calc_edges_cnt
{
  my $g = shift;
  my $total = 0;
  while (my ($key, $value) = each %$g )
  {
    my $len = scalar( keys %$value);
    $total += $len;
  }
  return $total / 2;    
}

sub est_formula
{
  my $g = shift;
  my $m = calc_edges_cnt($g);
  return (1 + sqrt(8 * $m + 1 )) / 2;  
}

# check if some vertex v contains only edges to mutual-connected verteces in graph g
# complexity is n ^ 2 / 2 where n is amount of edges in v
sub is_clique
{
  my($v, $g) = @_;
  my @es = keys %$v;
  my $es_count = scalar(@es);
  return 1 if ( $es_count < 2 );
  for ( my $i = 0; $i < $es_count; $i++ )
  {
    for ( my $j = $i + 1; $j < $es_count; $j++ )
    {
      return 0 if ( !connected($g, $es[$i], $es[$j]) );
    }
  }
  return 1;  
}

sub estimate_clique
{
  my $g = shift; # graph
  my $arr = make_sorted_array($g);
  dump_arr($arr);
  my %curr_g;
  my $res = undef;
  my $res_degree = 0;
  foreach my $i ( @$arr )
  {
    last if ( defined($res) && $res > $i->[0] );
    add_vertexes(\%curr_g, $i->[1]);
    my $len = scalar ( keys %curr_g );
    if ( $len < $i->[0] )
    {
      printf("skip %d bcs %d vertexes\n", $i->[0], $len) if ( defined $opt_v );
      next;
    }
    my $tmp = compact(\%curr_g);
    if ( defined($tmp) )
    {
      if ( !defined $res )
      {
        $res = $tmp;
        $res_degree = $i->[0];
      } elsif ( $tmp > $res )
      {
        $res = $tmp;
        $res_degree = $i->[0];
      }
    }
  }
  if ( defined $res )
  {
    printf("%d on degree %d, %d vertexes\n", $res, $res_degree, scalar(keys %curr_g));
    printf("predicted %f\n", est_formula($g));
  }
}

# main
my $g;
my $status = getopts("cdvg:s:");
usage() if ( !$status );
if ( @ARGV )
{
  my $v = int(shift @ARGV);
  if ( !$v )
  {
    printf("zero vertexes num\n");
    usage();
  }
  my $e = 0;
  if ( @ARGV )
  {
    $e = int(shift @ARGV);
    if ( $e < $v )
    {
      printf("edges num must be bigger than vertexes num\n");
      usage();
    }
  }
  $e = $v * 10 if ( !$e );
  $g = gen_random_graph($v, $e);
  my $boost = 0;
  if ( @ARGV )
  {
    $boost = int(shift @ARGV);
    add_boost($g, $v, $boost);
  }
  if ( defined $opt_g )
  {
    my $gsize = int($opt_g);
    if ( !$gsize )
    {
      warn("ignore -g with arg $opt_g\n");
    } elsif ( $gsize >= $v )
    {
      warn("clique size is too big\n");  
    } else {
      add_clique($g, $gsize);
    }
  }
  connect_rem($g, $v) if ( defined $opt_c ); 
  # save generated graph
  if ( defined($g) && defined($opt_s) )
  {
    store($g, $opt_s);
  }
} else {
  usage() if ( !defined($opt_s) );
  $g = retrieve($opt_s);
}
die("cannot make graph") if ( !defined $g );
calc_variance($g);
print Dumper($g) if ( defined $opt_d );
# estimate max clique size
estimate_clique($g);
