#!make -f
CXXFLAGS = -Ofast -fPIC -shared -g -std=gnu++14

rpcq: rpcq.o gen-cpp/Symref.o gen-cpp/gptest_types.o
	gcc -o $@ $? -lstdc++ -lthrift

gen-cpp/Symref.cpp: gptest.thrift
	thrift --gen cpp:moveable_types gptest.thrift
