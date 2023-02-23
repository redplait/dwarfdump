EHDR = ../ELFIO

all:
	g++ -O3 -I $(EHDR) main.cc ElfFile.cc TreeBuilder.cc -o dumper -Wall

dumper.g:
	g++ -g -I $(EHDR) main.cc ElfFile.cc TreeBuilder.cc -o dumper.d -Wall
	objdump -g dumper.d > dumper.g