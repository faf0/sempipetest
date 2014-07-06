CC = gcc
CPP = g++
CPPFLAGS += -I./ -Wall -pedantic -D_SVID_SOURCE
CFLAGS += -std=c99 -O3
CXXFLAGS += -O3
LDFLAGS  += -L./
LOADLIBES = -pthread 

.PHONY: all, clean

PROGNAME := sempipetest 

all:	sempipetest
sempipetest:	sempipetest.o

clean:
	rm *.o

