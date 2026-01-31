#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdlib.h>

/* ===== COSTANTI ===== */
#define MAP_SIZE 20
#define MAX_PLAYERS 10         // Nessun limite a priori, ma usiamo un max pratico
#define MAX_NICKNAME 20
#define MAX_PASSWORD 20
#define VIEW_RADIUS 3          // Raggio visione per scoprire ostacoli
#define UPDATE_INTERVAL 5      // Secondi tra aggiornamenti globali
#define GAME_DURATION 180      // 3 minuti
#define SERVER_PORT 8888

/* ===== TIPI DI CELLE ===== */
#define CELL_FREE 0
#define CELL_WALL 1

/* ===== TIPI DI MESSAGGI ===== */
#define MSG_REGISTER 1         // Registrazione nuovo utente
#define MSG_LOGIN 2            // Login
#define MSG_MOVE 3             // Movimento (UP/DOWN/LEFT/RIGHT)
#define MSG_LIST_PLAYERS 4     // Richiesta lista giocatori connessi
#define MSG_QUIT 5             // Disconnessione

#define MSG_OK 10              // Operazione riuscita
#define MSG_ERROR 11           // Errore generico
#define MSG_LOCAL_MAP 12       // Mappa locale dopo movimento
#define MSG_GLOBAL_MAP 13      // Aggiornamento globale periodico
#define MSG_PLAYERS_LIST 14    // Risposta con lista giocatori
#define MSG_GAME_OVER 15       // Fine partita con classifica

/* ===== DIREZIONI ===== */
#define DIR_UP 0
#define DIR_DOWN 1
#define DIR_LEFT 2
#define DIR_RIGHT 3

/* ===== STRUTTURE DATI ===== */

/* Posizione sulla mappa */
typedef struct {
    int x;
    int y;
} Position;

/* Header messaggio (inviato prima di ogni messaggio) */
typedef struct {
    int type;
    int length;  // Lunghezza dei dati che seguono
} MsgHeader;

/* Messaggio di registrazione/login */
typedef struct {
    char nickname[MAX_NICKNAME];
    char password[MAX_PASSWORD];
} AuthMsg;

/* Messaggio di movimento */
typedef struct {
    int direction;  // DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT
} MoveMsg;

/* Info su un singolo giocatore */
typedef struct {
    int id;
    char nickname[MAX_NICKNAME];
    int x, y;
    int cells_owned;
} PlayerInfo;

/* Lista giocatori connessi */
typedef struct {
    int count;
    PlayerInfo players[MAX_PLAYERS];
} PlayersListMsg;

/* Mappa locale (inviata dopo ogni movimento) */
typedef struct {
    int your_x, your_y;        // Posizione attuale del giocatore
    int view_size;             // Dimensione finestra (es. 7 se VIEW_RADIUS=3)
    // Seguono view_size*view_size celle con:
    // - type (CELL_FREE/CELL_WALL) se scoperto, -1 se non ancora scoperto
    // - owner_id (-1 se nessuno)
} LocalMapMsg;


/* Mappa globale (inviata periodicamente a tutti) */
typedef struct {
    int num_players;
    PlayerInfo players[MAX_PLAYERS];
    int ownership[MAP_SIZE][MAP_SIZE];  // Solo ownership, -1 = nessun proprietario
    int time_remaining;
} GlobalMapMsg;

/* Messaggio fine partita */
typedef struct {
    int winner_id;
    char winner_name[MAX_NICKNAME];
    int winner_score;
    int num_players;
    PlayerInfo final_ranking[MAX_PLAYERS]; 
} GameOverMsg;

/* Cella lato server */
typedef struct {
    int type;      // CELL_FREE o CELL_WALL
    int owner_id;  // ID del proprietario (-1 se nessuno)
} Cell;

/* Stato di un giocatore sul server */
typedef struct {
    int id;
    char nickname[MAX_NICKNAME];
    char password[MAX_PASSWORD];
    int socket_fd;
    Position pos;
    int cells_owned;
    int is_playing;  // 1 se in partita, 0 se solo registrato
    int walls_discovered[MAP_SIZE][MAP_SIZE];  // 1 se ha scoperto il muro
} Player;

#endif /* COMMON_H */