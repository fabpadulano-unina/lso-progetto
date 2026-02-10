#include "player.h"
#include <stdio.h>  
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <fcntl.h> 

/* legge dal file descriptor fd finché non trova \n o finisce il buffer e restituisce il numero di byte letti */
ssize_t read_line(int fd, char *buffer, size_t n) {
    ssize_t num_read;
    size_t tot_read = 0;
    char ch;

    if (n <= 0 || buffer == NULL) return -1;

    while (tot_read < n - 1) {
        num_read = read(fd, &ch, 1); 
        
        if (num_read == -1) {
            return -1; 
        } else if (num_read == 0) {
            break; // EOF 
        } else {
            buffer[tot_read++] = ch;
            if (ch == '\n') break; 
        }
    }

    buffer[tot_read] = '\0'; 
    return tot_read;
}

/* controlla se un nickname esiste nel file */
int player_exists(const char* nickname) {
    int fd; 
    char line[128];
    char* nick;
    
    fd = open(USERS_FILE, O_RDONLY);
    if(fd < 0) {
        return 0; // file non esiste o errore, nessun utente
    }
    
    // read_line restituisce 0 se è EOF
    while(read_line(fd, line, sizeof(line)) > 0) {
        // estrai il nickname (tutto prima dei ':')
        nick = strtok(line, ":");
        
        if(nick != NULL && strcmp(nick, nickname) == 0) {
            close(fd);
            return 1; //trovato
        }
    }
    
    close(fd);
    return 0; // non trovato
}

/* registra un nuovo utente */
int player_register(const char* nickname, const char* password) {
    int fd;
    char buffer[256];
    int len;

    if(player_exists(nickname)) {
        return -1; // nickname già usato
    }
    
    // 0644: permessi rw-r--r--
    fd = open(USERS_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd < 0) {
        return -2; // errore apertura file
    }
    
    len = sprintf(buffer, "%s:%s\n", nickname, password);
    
    write(fd, buffer, len);
    
    close(fd);
    
    return 0; 
}

int player_authenticate(const char* nickname, const char* password) {
    int fd;
    char line[128];
    char* nick;
    char* pass;
    
    fd = open(USERS_FILE, O_RDONLY);
    if(fd < 0) {
        return 0; // file non esiste, nessun utente
    }
    
    // leggi ogni riga
    while(read_line(fd, line, sizeof(line)) > 0) {
        // estrae nickname 
        nick = strtok(line, ":");
        // estrae password 
        pass = strtok(NULL, ":\n");
        
        if(nick != NULL && strcmp(nick, nickname) == 0) {
            close(fd);

            if(pass != NULL && strcmp(pass, password) == 0) {
                return 1; // credenziali corrette
            } else {
                return 0; // password errata
            }
        }
    }
    
    close(fd);
    return 0; // nickname non trovato
}


void players_init(Player players[MAX_PLAYERS]) {
    int i;
    for(i = 0; i < MAX_PLAYERS; i++) {
        players[i].id = -1;
        players[i].socket_fd = -1;
        players[i].is_playing = 0;
        players[i].cells_owned = 0;
        memset(players[i].nickname, 0, MAX_NICKNAME);
        memset(players[i].walls_discovered, 0, sizeof(players[i].walls_discovered));
    }
}

int players_find_free_slot(Player players[MAX_PLAYERS]) {
    int i;
    for(i = 0; i < MAX_PLAYERS; i++) {
        if(players[i].is_playing == 0) {
            return i;
        }
    }
    return -1; // pieno
}

int players_find_by_socket(Player players[MAX_PLAYERS], int socket_fd) {
    int i;
    for(i = 0; i < MAX_PLAYERS; i++) {
        if(players[i].is_playing && players[i].socket_fd == socket_fd) {
            return i;
        }
    }
    return -1;
}


int players_find_by_nickname(Player players[MAX_PLAYERS], const char* nickname) {
    int i;
    for(i = 0; i < MAX_PLAYERS; i++) {
        if(players[i].is_playing && strcmp(players[i].nickname, nickname) == 0) {
            return i;
        }
    }
    return -1;
}


int players_add(Player players[MAX_PLAYERS], const char* nickname, 
                int socket_fd, Position start_pos) {
    int slot = players_find_free_slot(players);
    
    if(slot == -1) {
        return -1; 
    }
    
    players[slot].id = slot;
    strcpy(players[slot].nickname, nickname);
    players[slot].socket_fd = socket_fd;
    players[slot].pos = start_pos;
    players[slot].cells_owned = 1; // parte con 1 cella (la sua posizione)
    players[slot].is_playing = 1;
    memset(players[slot].walls_discovered, 0, sizeof(players[slot].walls_discovered));
    
    return slot;
}


int players_remove(Player players[MAX_PLAYERS], int player_id) {
    if(player_id < 0 || player_id >= MAX_PLAYERS) {
        return -1;
    }
    
    players[player_id].is_playing = 0;
    players[player_id].socket_fd = -1;
    players[player_id].cells_owned = 0;
    
    return 0;
}

void players_update_score(Player players[MAX_PLAYERS], int player_id, int score) {
    if(player_id >= 0 && player_id < MAX_PLAYERS) {
        players[player_id].cells_owned = score;
    }
}

int players_count_active(Player players[MAX_PLAYERS]) {
    int count = 0;
    int i;
    
    for(i = 0; i < MAX_PLAYERS; i++) {
        if(players[i].is_playing) {
            count++;
        }
    }
    
    return count;
}