CC=gcc
CFLAGS=-Wall -g -std=c99 -I include

all: client server

client: bin
	$(CC) $(CFLAGS) -o bin/client src/client.c

server: bin
	$(CC) $(CFLAGS) -o bin/server src/server.c src/list.c

test: bin
	$(CC) $(CFLAGS) -o bin/test src/client.c

bin:
	- mkdir bin

clean:
	- rm bin/client bin/server bin/test
