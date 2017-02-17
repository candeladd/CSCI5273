
CC = gcc
CFLAGS = -c -g -Wall -Wextra
LFLAGS = -Wall -Wextra -pthread

.PHONY: all clean

all: client server

client: client.o 
	$(CC) $(LFLAGS) $^ -o $@

client.o: client.c
	$(CC) $(CFLAGS) $<

server: server.o 
	$(CC) $(LFLAGS) $^ -o $@

server.o: server.c
	$(CC) $(CFLAGS) $<



clean:
	rm -f server
	rm -f *.o
	rm -f *~
	rm -f results.txt