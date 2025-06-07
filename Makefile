CC = gcc
CFLAGS = -c

all: server client
	@rm -f common.o

common.o: src/common.c
	$(CC) $(CFLAGS) src/common.c -o common.o

server: src/server.c common.o
	$(CC) src/server.c common.o -o server

client: src/client.c common.o
	$(CC) src/client.c common.o -o client

.PHONY: all clean

clean:
	rm -f common.o server client
