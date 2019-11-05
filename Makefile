CC=g++
CFLAGS=-O3 -Wall -Wextra -std=gnu++11 -pthread

all: demo

pool.o:	pool.cpp pool.h $(headers)
	$(CC) $(CFLAGS) -c pool.cpp

demo: demo.cpp pool.o $(headers)
	$(CC) $(CFLAGS) -o demo demo.cpp pool.o

clean:
	/bin/rm -f *.o 
distclean: clean
	/bin/rm -f *.o demo
build: distclean all

