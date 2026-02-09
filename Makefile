CC = gcc
CFLAGS = -Wall -g

COMMON_OBJ = protocol.o map.o player.o

all: server client

server: server.o $(COMMON_OBJ)
	mkdir -p data
	$(CC) $(CFLAGS) server.o $(COMMON_OBJ) -o server

server.o: server.c common.h protocol.h map.h player.h
	$(CC) $(CFLAGS) -c server.c

client: client.o protocol.o
	$(CC) $(CFLAGS) client.o protocol.o -o client

client.o: client.c common.h protocol.h
	$(CC) $(CFLAGS) -c client.c

protocol.o: protocol.c protocol.h common.h
	$(CC) $(CFLAGS) -c protocol.c

map.o: map.c map.h common.h
	$(CC) $(CFLAGS) -c map.c

player.o: player.c player.h common.h
	$(CC) $(CFLAGS) -c player.c

clean:
	rm -f *.o server client
