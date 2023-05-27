#!make -f
CXX=/usr/bin/g++

all: ctf ctf2

ctf: ctf.cpp al.inc data.inc aes.inc graph.inc
	$(CXX) -Wall -o ctf ctf.cpp -lstdc++ -lcrypto

ctf2: ctf2.cpp al2.inc data2.inc aes.inc graph.inc
	$(CXX) -Wall -o ctf2 ctf2.cpp -lstdc++ -lcrypto
