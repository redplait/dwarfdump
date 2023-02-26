EHDR = ../ELFIO
SRC = main.cc ElfFile.cc TreeBuilder.cc JsonRender.cc PlainRender.cc

all:
	g++ -O3 -I $(EHDR) $(SRC) -o dumper -Wall

dumper.g: $(SRC)
	g++ -g -I $(EHDR) $(SRC) -o dumper.d -Wall
	objdump -g dumper.d > dumper.g