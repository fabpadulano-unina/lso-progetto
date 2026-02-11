#ifndef PLAYER_H
#define PLAYER_H

#include "common.h"

#define USERS_FILE "data/users.txt"


/* registra un nuovo utente nel file users.txt, ritorna 0 se ok, -1 se nickname già esistente, -2 se errore file */
int player_register(const char* nickname, const char* password);

/* verifica le credenziali di login, ritorna 1 se credenziali corrette, 0 se errate, -1 se errore */
int player_authenticate(const char* nickname, const char* password);

/* controlla se un nickname esiste già, ritorna 1 se esiste, 0 se non esiste, -1 se errore */
int player_exists(const char* nickname);

/* inizializza l'array di giocatori */
void players_init(Player players[MAX_PLAYERS]);

/* trova uno slot libero nell'array giocatori*/
int players_find_free_slot(Player players[MAX_PLAYERS]);

/* trova un giocatore per socket, ritorna l'indice del giocatore*/
int players_find_by_socket(Player players[MAX_PLAYERS], int socket_fd);

/* trova un giocatore per nickname, ritorna l'indice del giocatore*/
int players_find_by_nickname(Player players[MAX_PLAYERS], const char* nickname);

/* aggiunge un giocatore alla partita, ritorna l'ID del giocatore (indice), -1 se errore */
int players_add(Player players[MAX_PLAYERS], const char* nickname, 
                int socket_fd, Position start_pos);

/* rimuove un giocatore dalla partita, ritorna 0 se ok, -1 se errore */
int players_remove(Player players[MAX_PLAYERS], int player_id);

/* aggiorna il punteggio (celle possedute) di un giocatore */
void players_update_score(Player players[MAX_PLAYERS], int player_id, int score);

/* conta quanti giocatori sono attivi in partita, ritorna il numero di giocatori attivi */
int players_count_active(Player players[MAX_PLAYERS]);

#endif 