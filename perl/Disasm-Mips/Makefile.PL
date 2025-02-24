use 5.030000;
use ExtUtils::MakeMaker;
use Config;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'Disasm::Mips',
    VERSION_FROM      => 'lib/Disasm/Mips.pm', # finds $VERSION, requires EU::MM from perl >= 5.5
    PREREQ_PM         => { 'Elf::Reader' => 0.1 }, # e.g., Module::Name => 1.1
    ABSTRACT_FROM     => 'lib/Disasm/Mips.pm', # retrieve abstract from module
    AUTHOR            => 'redp <redp@mail.ru>',
    #LICENSE           => 'perl',
    #Value must be from legacy list of licenses here
    #http://search.cpan.org/perldoc?Module%3A%3ABuild%3A%3AAPI
    XSOPT             => '-C++',
    CC                => 'g++',
    LIBS              => ["-Wl,--as-needed -L$Config{'sitearch'}/auto/Elf/Reader -l:Reader.so -lstdc++"], # e.g., '-lm'
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I/home/redp/disc/ELFIO', # e.g., '-I. -I/usr/include/other'
	# Un-comment this if you add C files to link with later:
    OBJECT            => '$(O_FILES)', # link all the C files too
);
