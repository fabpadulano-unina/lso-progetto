#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdlib.h>


#define MAP_SIZE 20
#define MAX_PLAYERS 1000         
#define MAX_NICKNAME 20
#define MAX_PASSWORD 20
#define VIEW_RADIUS 3          // raggio visione per scoprire ostacoli
#define UPDATE_INTERVAL 5      // secondi tra aggiornamenti globali
#define GAME_DURATION 180      // 3 minuti
#define SERVER_PORT 8888

// tipi di celle
#define CELL_FREE 0
#define CELL_WALL 1

// tipi messaggi
#define MSG_REGISTER 1         
#define MSG_LOGIN 2           
#define MSG_MOVE 3             
#define MSG_LIST_PLAYERS 4    
#define MSG_QUIT 5             

#define MSG_OK 10              // operazione riuscita
#define MSG_ERROR 11           
#define MSG_LOCAL_MAP 12       
#define MSG_GLOBAL_MAP 13      // aggiornamento globale periodico
#define MSG_PLAYERS_LIST 14   
#define MSG_GAME_OVER 15       

// direzioni
#define DIR_UP 0
#define DIR_DOWN 1
#define DIR_LEFT 2
#define DIR_RIGHT 3




typedef struct {
    int x;
    int y;
} Position;

/* inviato prima di ogni messaggio */
typedef struct {
    int type;
    int length;  // lunghezza dei dati 
} MsgHeader;


typedef struct {
    char nickname[MAX_NICKNAME];
    char password[MAX_PASSWORD];
} AuthMsg;


typedef struct {
    int direction;  // DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT
} MoveMsg;

/* info su un singolo giocatore */
typedef struct {
    int id;
    char nickname[MAX_NICKNAME];
    int x, y;
    int cells_owned;
} PlayerInfo;

/* lista giocatori connessi */
typedef struct {
    int count;
    PlayerInfo players[MAX_PLAYERS];
} PlayersListMsg;

/* mappa locale (inviata dopo ogni movimento) */
typedef struct {
    int your_x, your_y;        
    int view_size;             // Dimensione finestra (es. 7 se VIEW_RADIUS=3)
} LocalMapMsg;


/* mappa globale (inviata periodicamente a tutti) */
typedef struct {
    int num_players;
    PlayerInfo players[MAX_PLAYERS];
    int ownership[MAP_SIZE][MAP_SIZE];  // solo ownership, -1 = nessun proprietario
    int time_remaining;
} GlobalMapMsg;

/* messaggio fine partita */
typedef struct {
    int winner_id;
    char winner_name[MAX_NICKNAME];
    int winner_score;
    int num_players;
    PlayerInfo final_ranking[MAX_PLAYERS]; 
} GameOverMsg;


typedef struct {
    int type;      // CELL_FREE o CELL_WALL
    int owner_id;  // ID del proprietario (-1 se nessuno)
} Cell;


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

#endif 