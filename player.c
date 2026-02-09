#include "player.h"
#include <stdio.h>  
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include <fcntl.h> 

/* Legge dal file descriptor fd finché non trova \n o finisce il buffer.
   Restituisce il numero di byte letti. */
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

/* ===== FUNZIONI FILE UTENTI ===== */

/* Controlla se un nickname esiste nel file */
int player_exists(const char* nickname) {
    int fd; 
    char line[128];
    char* nick;
    
    fd = open(USERS_FILE, O_RDONLY);
    if(fd < 0) {
        return 0; // File non esiste o errore, nessun utente
    }
    
    // read_line restituisce 0 se è EOF
    while(read_line(fd, line, sizeof(line)) > 0) {
        // Estrai il nickname (tutto prima dei ':')
        nick = strtok(line, ":");
        
        if(nick != NULL && strcmp(nick, nickname) == 0) {
            close(fd);
            return 1; // Trovato!
        }
    }
    
    close(fd);
    return 0; // Non trovato
}

/* Registra un nuovo utente */
int player_register(const char* nickname, const char* password) {
    int fd;
    char buffer[256];
    int len;

    // Controlla se esiste già
    if(player_exists(nickname)) {
        return -1; // Nickname già usato
    }
    
    // O_WRONLY: Scrittura
    // O_CREAT: Crea se non esiste
    // O_APPEND: Scrivi alla fine del file
    // 0644: Permessi rw-r--r--
    fd = open(USERS_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd < 0) {
        return -2; // Errore apertura file
    }
    
    len = sprintf(buffer, "%s:%s\n", nickname, password);
    
    write(fd, buffer, len);
    
    close(fd);
    
    return 0; // OK
}

/* Autentica un utente */
int player_authenticate(const char* nickname, const char* password) {
    int fd;
    char line[128];
    char* nick;
    char* pass;
    
    fd = open(USERS_FILE, O_RDONLY);
    if(fd < 0) {
        return 0; // File non esiste, nessun utente
    }
    
    // Leggi ogni riga
    while(read_line(fd, line, sizeof(line)) > 0) {
        // Estrai nickname (prima parte)
        nick = strtok(line, ":");
        // Estrai password (seconda parte)
        pass = strtok(NULL, ":\n");
        
        // Controlla se il nickname corrisponde
        if(nick != NULL && strcmp(nick, nickname) == 0) {
            close(fd);
            
            // Controlla password
            if(pass != NULL && strcmp(pass, password) == 0) {
                return 1; // OK: credenziali corrette
            } else {
                return 0; // Password errata
            }
        }
    }
    
    close(fd);
    return 0; // Nickname non trovato
}

/* ===== FUNZIONI GESTIONE GIOCATORI IN PARTITA ===== */

/* Inizializza array giocatori */
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

/* Trova slot libero */
int players_find_free_slot(Player players[MAX_PLAYERS]) {
    int i;
    for(i = 0; i < MAX_PLAYERS; i++) {
        if(players[i].is_playing == 0) {
            return i;
        }
    }
    return -1; // Pieno
}

/* Trova giocatore per socket */
int players_find_by_socket(Player players[MAX_PLAYERS], int socket_fd) {
    int i;
    for(i = 0; i < MAX_PLAYERS; i++) {
        if(players[i].is_playing && players[i].socket_fd == socket_fd) {
            return i;
        }
    }
    return -1;
}

/* Trova giocatore per nickname */
int players_find_by_nickname(Player players[MAX_PLAYERS], const char* nickname) {
    int i;
    for(i = 0; i < MAX_PLAYERS; i++) {
        if(players[i].is_playing && strcmp(players[i].nickname, nickname) == 0) {
            return i;
        }
    }
    return -1;
}

/* Aggiunge un giocatore */
int players_add(Player players[MAX_PLAYERS], const char* nickname, 
                int socket_fd, Position start_pos) {
    int slot = players_find_free_slot(players);
    
    if(slot == -1) {
        return -1; // Nessuno slot libero
    }
    
    // Inizializza giocatore
    players[slot].id = slot;
    strcpy(players[slot].nickname, nickname);
    players[slot].socket_fd = socket_fd;
    players[slot].pos = start_pos;
    players[slot].cells_owned = 1; // Parte con 1 cella (la sua posizione)
    players[slot].is_playing = 1;
    memset(players[slot].walls_discovered, 0, sizeof(players[slot].walls_discovered));
    
    return slot;
}

/* Rimuove un giocatore */
int players_remove(Player players[MAX_PLAYERS], int player_id) {
    if(player_id < 0 || player_id >= MAX_PLAYERS) {
        return -1;
    }
    
    players[player_id].is_playing = 0;
    players[player_id].socket_fd = -1;
    players[player_id].cells_owned = 0;
    
    return 0;
}

/* Aggiorna punteggio */
void players_update_score(Player players[MAX_PLAYERS], int player_id, int score) {
    if(player_id >= 0 && player_id < MAX_PLAYERS) {
        players[player_id].cells_owned = score;
    }
}

/* Conta giocatori attivi */
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