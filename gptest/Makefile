#!make -f
# target gcc
TGCC=/usr/local/bin/gcc
# see https://gcc.gnu.org/onlinedocs/gccint/Plugins-building.html
GCC_PLUGIN_INCLUDE_PATH ?= $(shell $(TGCC) -print-file-name=plugin)/include
CXXFLAGS = -fPIC -shared -fno-rtti -g -std=gnu++14 -w -I $(GCC_PLUGIN_INCLUDE_PATH)
# for profiler
PROSRC=eread.cc lditer.cc prof.cc
PROFLAGS=-pg -I ../../ELFIO/

HOST_GCC=/usr/bin/gcc
# for sqlite replace pers_text.cpp with pers_sqlite.cpp for PLUGIN_SOURCE_FILES and add -lsqlite3 to PLUGIN_LIBS
PLUGIN_SOURCE_FILES=gptest.cpp pers_text.cpp
PLUGIN_LIBS=-lstdc++

gptest.so: $(PLUGIN_SOURCE_FILES) $(PROSRC)
	$(HOST_GCC) $(PROFLAGS) -shared $(CXXFLAGS) $^ -o $@ $(PLUGIN_LIBS) -lsqlite3

myrtl: gptest.so
	$(TGCC) -fplugin=./gptest.so -c -o 1.o -shared $(CXXFLAGS) gptest.cpp

rtl: gptest.so
	$(TGCC) -fdump-rtl-final -c -o 1.o -shared $(CXXFLAGS) gptest.cpp

test: gptest.so
	-rm 1.db
	$(TGCC) -fplugin=./gptest.so -fplugin-arg-gptest-db=./1.db -c -o 1.o -shared $(CXXFLAGS) gptest.cpp
