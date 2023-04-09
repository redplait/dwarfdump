EHDR = ../ELFIO
SRC=main.cc regnames.cc ElfFile.cc GoTypes.cc TreeBuilder.cc JsonRender.cc PlainRender.cc


all: $(SRC)
	g++ -O3 -I $(EHDR) $(SRC) -o dumper -Wall -lz

dumper.d: $(SRC)
	g++ -g -I $(EHDR) $(SRC) -o dumper.d -Wall -lz

dumper.g: dumper.d	
	objdump -g dumper.d > dumper.g

dumper32.d: $(SRC)
	g++ -m32 -g -I $(EHDR) $(SRC) -o dumper32.d -Wall -lz

dumper32.g: dumper32.d	
	objdump -g dumper32.d > dumper32.g
