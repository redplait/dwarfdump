#!make -f
# target gcc
TGCC=/usr/local/bin/gcc
# see https://gcc.gnu.org/onlinedocs/gccint/Plugins-building.html
GCC_PLUGIN_INCLUDE_PATH ?= $(shell $(TGCC) -print-file-name=plugin)/include
CXXFLAGS = -fPIC -shared -fno-rtti -g -std=gnu++14 -w -I $(GCC_PLUGIN_INCLUDE_PATH)

HOST_GCC=/usr/bin/gcc
# for sqlite replace pers_text.cpp with pers_sqlite.cpp for PLUGIN_SOURCE_FILES and add -lsqlite3 to PLUGIN_LIBS
PLUGIN_SOURCE_FILES=gptest.cpp pers_text.cpp
PLUGIN_LIBS=-lstdc++

gptest.so: $(PLUGIN_SOURCE_FILES)
	$(HOST_GCC) -shared $(CXXFLAGS) $^ -o $@ $(PLUGIN_LIBS)
