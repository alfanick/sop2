CC=clang

all: server client

clean:
	ipcrm -S 0x00013b1a -S 0x00013b89 -M 0x000138d5 -Q 0x007ca248

server:
	$(CC) src/common.c src/server.c -o bin/server

client:
	$(CC) src/common.c src/client.c -o bin/client


