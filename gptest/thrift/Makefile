#!make -f
CXXFLAGS = -Ofast -g -std=gnu++17 -pthread

rpcq: rpcq.o gen-cpp/Symref.o gen-cpp/gptest_types.o
	gcc -o $@ $^ -lstdc++ -lthrift

rpcs: server.o gen-cpp/Symref.o gen-cpp/gptest_types.o
	gcc -pthread -o $@ $^ -lstdc++ -lthrift -lsqlite3

gen-cpp/Symref.cpp: gptest.thrift
	thrift --gen cpp:moveable_types gptest.thrift

clear:
	-rm *.o gen-cpp/*.o