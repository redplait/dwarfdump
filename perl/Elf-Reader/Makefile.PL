use 5.030000;
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'Elf::Reader',
    VERSION_FROM      => 'lib/Elf/Reader.pm', # finds $VERSION, requires EU::MM from perl >= 5.5
    PREREQ_PM         => {}, # e.g., Module::Name => 1.1
    ABSTRACT_FROM     => 'lib/Elf/Reader.pm', # retrieve abstract from module
    AUTHOR            => 'redp <redplait@gmail.com>',
    #LICENSE           => 'perl',
    #Value must be from legacy list of licenses here
    #http://search.cpan.org/perldoc?Module%3A%3ABuild%3A%3AAPI
    CC                => 'g++',
    CCFLAGS           => "$Config::Config{ccflags} -gdwarf-4",
    XSOPT             => '-C++',
    LIBS              => ['-lstdc++'], # e.g., '-lm'
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I/home/redp/disc/ELFIO', # e.g., '-I. -I/usr/include/other'
	# Un-comment this if you add C files to link with later:
    OBJECT            => '$(O_FILES)', # link all the C files too
);
