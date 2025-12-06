use 5.030000;
use ExtUtils::MakeMaker;
use Config;

# download eiattrs.inc
use LWP::Simple;

# download couple of files
my $e_inc = 'eiattrs.inc';
if ( !-f $e_inc ) {
  my $url = 'https://raw.githubusercontent.com/redplait/denvdis/refs/heads/master/test/eiattrs.inc';
  my $status = getstore($url, $e_inc);
  die("cannot loaf $e_inc") if ( !is_success($status) );
}

# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'Cubin::Attrs',
    VERSION_FROM      => 'lib/Cubin/Attrs.pm', # finds $VERSION, requires EU::MM from perl >= 5.5
    PREREQ_PM         => { 'Elf::Reader' => 0.01 },
    ABSTRACT_FROM     => 'lib/Cubin/Attrs.pm', # retrieve abstract from module
    AUTHOR            => 'redp <redp@mail.ru>',
    #LICENSE           => 'perl',
    #Value must be from legacy list of licenses here
    #http://search.cpan.org/perldoc?Module%3A%3ABuild%3A%3AAPI
    XSOPT             => '-C++',
    CC                => 'g++ -std=c++20',
    LIBS              => ["-Wl,--as-needed -L$Config{'sitearch'}/auto/Elf/Reader -l:Reader.so -L../.. -lstdc++"], # e.g., '-lm'
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I. -I/home/redp/disc/ELFIO', # e.g., '-I. -I/usr/include/other'
	# Un-comment this if you add C files to link with later:
    # OBJECT            => '$(O_FILES)', # link all the C files too
);
