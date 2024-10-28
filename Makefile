EHDR = ../ELFIO
CFLAGS=-std=c++17 -I $(EHDR)
SRC=main.cc nfilter.cc regnames.cc ElfFile.cc Elf_reloc.cc GoTypes.cc TreeBuilder.cc JsonRender.cc PlainRender.cc
OBJS=regnames.os ElfFile.os Elf_reloc.os GoTypes.os TreeBuilder.os

all: dumper libpdwl.a

dumper: $(SRC)
	g++ -g $(CFLAGS) $(SRC) -o dumper -Wall -lz

%.os: %.cc
	g++ -g -fPIC $(CFLAGS) -c -o $@ $<

libpdwl.a: $(OBJS)
	ar $(ARFLAGS) $@ $(OBJS)

dumper.d: $(SRC)
	g++ -g -gdwarf-4 $(CFLAGS) $(SRC) -o dumper.d -Wall -lz

dumper.g: dumper.d
	objdump -g dumper.d > dumper.g

dumper32.d: $(SRC)
	g++ -m32 -g -I $(EHDR) $(SRC) -o dumper32.d -Wall -lz

dumper32.g: dumper32.d
	objdump -g dumper32.d > dumper32.g
