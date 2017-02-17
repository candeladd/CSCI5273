#Andrew Candelaresi
#Programming Assignment 3



CC = gcc
CFLAGS = -c -g -Wall -Wextra
LFLAGS = -lssl -lcrypto

.PHONY: all clean

all: dfc dfs

dfc: DFC.o
	$(CC) $^ -o $@ $(LFLAGS)

dfs: DFS.o
	$(CC) $^ -o $@ $(LFLAGS)

dfc.o: DFC.c
	$(CC) $(CFLAGS) $<

dfs.o: DFS.c
	$(CC) $(CFLAGS) $<

clean:
	rm -f *.o
	rm -f *~
	rm -f dfc dfs