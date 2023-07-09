#!perl -w
# awful script to parse required R version from cran packages
use strict;
use warnings;
use LWP::UserAgent;

my $ua = LWP::UserAgent->new;
$ua->agent("r-crawler/0.1 ");
my $g_root = 'https://cran.r-project.org/web/packages/';
# Create a request
my $req = HTTP::Request->new(GET => $g_root . 'available_packages_by_name.html');
my $res = $ua->request($req);

# main db - key is R version and value is list of packages
my %g_db;
my $g_total = 0;

sub dump_versions
{
  foreach my $v ( sort { scalar @{ $g_db{$b} } <=> scalar @{ $g_db{$a} } } keys %g_db )
  {
    my $c = scalar @{ $g_db{$v} };
    printf("%s: %d %f\n", $v, $c, $c / $g_total);
  }   
}

sub parse_details
{
 my($name, $c) = @_;
 my $state = 0;
 foreach my $str ( split /\n/, $c )
 {
   if ( !$state )
   {
     $state = 1 if ( $str =~ m#<td>Depends:</td># );
     next;
   }
   if ( $str =~ /<td>R \(([^\)]+)\)/ )
   {
     printf("%s %s\n", $name, $1);
     if ( exists $g_db{$1} )
     {
       push @{ $g_db{$1} }, $name;
     } else {
       $g_db{$1} = [ $name ];
     }
   }
   last;
 }   
}

sub gather_packs
{
  my($ua, $c) = @_;
  foreach my $str ( split /\n/, $c )
  {
    next if ( $str !~ m#<a href=".*/([^/]+)/index\.html"><span class="CRAN"># );
    # printf("%s \n", $1);
    my $url = $g_root . $1 . '/index.html';
    my $req = HTTP::Request->new(GET => $url);
    my $res = $ua->request($req);
    if (! $res->is_success )
    {
      printf("cannot get details for %s, $res->status_line\n", $url);
      next;  
    }
    $g_total++;
    parse_details($1, $res->content);
  }  
}

# Check the outcome of the response
if ($res->is_success) {
  gather_packs($ua, $res->content);
  printf("total %d\n", $g_total);
  dump_versions();
}
else {
    print $res->status_line, "\n";
}
