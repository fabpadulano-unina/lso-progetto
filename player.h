#ifndef PLAYER_H
#define PLAYER_H

#include "common.h"

#define USERS_FILE "data/users.txt"

/* ===== GESTIONE CREDENZIALI (FILE) ===== */

/* Registra un nuovo utente nel file users.txt
 * Ritorna: 0 se ok, -1 se nickname già esistente, -2 se errore file */
int player_register(const char* nickname, const char* password);

/* Verifica le credenziali di login
 * Ritorna: 1 se credenziali corrette, 0 se errate, -1 se errore */
int player_authenticate(const char* nickname, const char* password);

/* Controlla se un nickname esiste già
 * Ritorna: 1 se esiste, 0 se non esiste, -1 se errore */
int player_exists(const char* nickname);

/* ===== GESTIONE GIOCATORI IN PARTITA ===== */

/* Inizializza l'array di giocatori */
void players_init(Player players[MAX_PLAYERS]);

/* Trova uno slot libero nell'array giocatori
 * Ritorna: indice dello slot, -1 se pieno */
int players_find_free_slot(Player players[MAX_PLAYERS]);

/* Trova un giocatore per socket
 * Ritorna: indice del giocatore, -1 se non trovato */
int players_find_by_socket(Player players[MAX_PLAYERS], int socket_fd);

/* Trova un giocatore per nickname
 * Ritorna: indice del giocatore, -1 se non trovato */
int players_find_by_nickname(Player players[MAX_PLAYERS], const char* nickname);

/* Aggiunge un giocatore alla partita
 * Ritorna: ID del giocatore (indice), -1 se errore */
int players_add(Player players[MAX_PLAYERS], const char* nickname, 
                int socket_fd, Position start_pos);

/* Rimuove un giocatore dalla partita
 * Ritorna: 0 se ok, -1 se errore */
int players_remove(Player players[MAX_PLAYERS], int player_id);

/* Aggiorna il punteggio (celle possedute) di un giocatore */
void players_update_score(Player players[MAX_PLAYERS], int player_id, int score);

/* Conta quanti giocatori sono attivi in partita
 * Ritorna: numero di giocatori attivi */
int players_count_active(Player players[MAX_PLAYERS]);

#endif /* PLAYER_H */