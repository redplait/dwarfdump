EHDR = ../ELFIO
SRC=main.cc ElfFile.cc TreeBuilder.cc JsonRender.cc PlainRender.cc


all: $(SRC)
	g++ -O3 -I $(EHDR) $(SRC) -o dumper -Wall

dumper.d: $(SRC)
	g++ -g -I $(EHDR) $(SRC) -o dumper.d -Wall

dumper.g: dumper.d	
	objdump -g dumper.d > dumper.g

dumper32.d: $(SRC)
	g++ -m32 -g -I $(EHDR) $(SRC) -o dumper32.d -Wall

dumper32.g: dumper32.d	
	objdump -g dumper32.d > dumper32.g
