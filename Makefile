# Makefile per il progetto Conquista del Territorio

CC = gcc
CFLAGS = -Wall -g
LDFLAGS = 

# File oggetto comuni
COMMON_OBJ = protocol.o map.o player.o

# Target principali
all: server client

# Server
server: server.o $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o server server.o $(COMMON_OBJ) $(LDFLAGS)

server.o: server.c common.h protocol.h map.h player.h
	$(CC) $(CFLAGS) -c server.c

# Client
client: client.o protocol.o
	$(CC) $(CFLAGS) -o client client.o protocol.o $(LDFLAGS)

client.o: client.c common.h protocol.h
	$(CC) $(CFLAGS) -c client.c

# File oggetto comuni
protocol.o: protocol.c protocol.h common.h
	$(CC) $(CFLAGS) -c protocol.c

map.o: map.c map.h common.h
	$(CC) $(CFLAGS) -c map.c

player.o: player.c player.h common.h
	$(CC) $(CFLAGS) -c player.c

# Pulizia
clean:
	rm -f *.o server client users.txt

# Pulizia completa (incluso file utenti)
cleanall: clean
	rm -f users.txt

# Aiuto
help:
	@echo "Target disponibili:"
	@echo "  make         - Compila tutto (server e client)"
	@echo "  make server  - Compila solo il server"
	@echo "  make client  - Compila solo il client"
	@echo "  make clean   - Rimuove file oggetto e eseguibili"
	@echo "  make cleanall- Rimuove tutto incluso users.txt"
	@echo "  make help    - Mostra questo messaggio"

.PHONY: all clean cleanall help