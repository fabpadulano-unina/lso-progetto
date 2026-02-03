#include "player.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ===== FUNZIONI FILE UTENTI ===== */

/* Controlla se un nickname esiste nel file */
int player_exists(const char* nickname) {
    FILE* f;
    char line[128];
    char* nick;
    
    f = fopen(USERS_FILE, "r");
    if(f == NULL) {
        return 0; // File non esiste, nessun utente
    }
    
    // Leggi ogni riga del file
    while(fgets(line, sizeof(line), f) != NULL) {
        // Estrai il nickname (tutto prima dei ':')
        nick = strtok(line, ":");
        
        if(nick != NULL && strcmp(nick, nickname) == 0) {
            fclose(f);
            return 1; // Trovato!
        }
    }
    
    fclose(f);
    return 0; // Non trovato
}

/* Registra un nuovo utente */
int player_register(const char* nickname, const char* password) {
    FILE* f;
    
    // Controlla se esiste già
    if(player_exists(nickname)) {
        return -1; // Nickname già usato
    }
    
    // Apri file in append
    f = fopen(USERS_FILE, "a");
    if(f == NULL) {
        return -2; // Errore apertura file
    }
    
    // Scrivi: nickname:password\n
    fprintf(f, "%s:%s\n", nickname, password);
    fclose(f);
    
    return 0; // OK
}

/* Autentica un utente */
int player_authenticate(const char* nickname, const char* password) {
    FILE* f;
    char line[128];
    char* nick;
    char* pass;
    
    f = fopen(USERS_FILE, "r");
    if(f == NULL) {
        return 0; // File non esiste, nessun utente
    }
    
    // Leggi ogni riga
    while(fgets(line, sizeof(line), f) != NULL) {
        // Estrai nickname (prima parte)
        nick = strtok(line, ":");
        // Estrai password (seconda parte)
        pass = strtok(NULL, ":\n");
        
        // Controlla se il nickname corrisponde
        if(nick != NULL && strcmp(nick, nickname) == 0) {
            fclose(f);
            
            // Controlla password
            if(pass != NULL && strcmp(pass, password) == 0) {
                return 1; // OK: credenziali corrette
            } else {
                return 0; // Password errata
            }
        }
    }
    
    fclose(f);
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