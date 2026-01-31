#ifndef MAP_H
#define MAP_H

#include "common.h"

/* ===== GESTIONE MAPPA ===== */

/* Inizializza la mappa con celle libere e muri casuali
 * wall_percentage: percentuale di celle che saranno muri (es. 20 = 20%) */
void map_init(Cell map[MAP_SIZE][MAP_SIZE], int wall_percentage);

/* Verifica se una posizione è valida e libera (non muro)
 * Ritorna: 1 se libera, 0 se muro o fuori mappa */
int map_is_free(Cell map[MAP_SIZE][MAP_SIZE], int x, int y);

/* Imposta il proprietario di una cella
 * Ritorna: 0 se ok, -1 se posizione non valida o è un muro */
int map_set_owner(Cell map[MAP_SIZE][MAP_SIZE], int x, int y, int owner_id);

/* Ottiene il proprietario di una cella
 * Ritorna: owner_id, o -1 se nessuno/errore */
int map_get_owner(Cell map[MAP_SIZE][MAP_SIZE], int x, int y);

/* Trova una posizione libera casuale sulla mappa
 * Utile per posizionare i giocatori all'inizio */
Position map_find_random_free_position(Cell map[MAP_SIZE][MAP_SIZE]);

/* Calcola quante celle possiede un giocatore
 * Ritorna: numero di celle possedute */
int map_count_cells_owned(Cell map[MAP_SIZE][MAP_SIZE], int player_id);

/* Rivela i muri attorno a una posizione nel raggio VIEW_RADIUS
 * Aggiorna l'array walls_discovered del giocatore */
void map_reveal_walls(Cell map[MAP_SIZE][MAP_SIZE], 
                      int walls_discovered[MAP_SIZE][MAP_SIZE],
                      int x, int y);

/* Stampa la mappa (per debug) */
void map_print(Cell map[MAP_SIZE][MAP_SIZE]);

#endif /* MAP_H */