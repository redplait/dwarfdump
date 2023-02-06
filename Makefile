EHDR = ../ELFIO

all:
	g++ -O3 -I $(EHDR) main.cc ElfFile.cc TreeBuilder.cc -o dumper -Wall
