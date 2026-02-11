#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

/* inizializza la mappa con muri casuali */
void map_init(Cell map[MAP_SIZE][MAP_SIZE], int wall_percentage) {
    int i, j;
    
    // seed inizializzato nel main

    
    // inizializza tutte le celle come libere senza proprietario
    for(i = 0; i < MAP_SIZE; i++) {
        for(j = 0; j < MAP_SIZE; j++) {
            map[i][j].type = CELL_FREE;
            map[i][j].owner_id = -1;
        }
    }
    
    // aggiungi muri casuali
    for(i = 0; i < MAP_SIZE; i++) {
        for(j = 0; j < MAP_SIZE; j++) {
            // genera numero casuale 0-99
            int random = rand() % 100;
            
            if(random < wall_percentage) {
                map[i][j].type = CELL_WALL;
            }
        }
    }
}

/* verifica se una posizione è valida e libera */
int map_is_free(Cell map[MAP_SIZE][MAP_SIZE], int x, int y) {
    // controlla se è dentro la mappa
    if(x < 0 || x >= MAP_SIZE || y < 0 || y >= MAP_SIZE) {
        return 0;
    }
    
    // Controlla se è libera (non muro)
    if(map[x][y].type == CELL_WALL) {
        return 0;
    }
    
    return 1;
}

/* imposta il proprietario di una cella */
int map_set_owner(Cell map[MAP_SIZE][MAP_SIZE], int x, int y, int owner_id) {
    // verifica che la posizione sia valida e libera
    if(!map_is_free(map, x, y)) {
        return -1;
    }
    
    map[x][y].owner_id = owner_id;
    return 0;
}


int map_get_owner(Cell map[MAP_SIZE][MAP_SIZE], int x, int y) {
    if(x < 0 || x >= MAP_SIZE || y < 0 || y >= MAP_SIZE) {
        return -1;
    }
    
    return map[x][y].owner_id;
}

/* trova una posizione libera casuale */
Position map_find_random_free_position(Cell map[MAP_SIZE][MAP_SIZE]) {
    Position pos;
    int attempts = 0;
    int max_attempts = MAP_SIZE * MAP_SIZE;
    
    do {
        pos.x = rand() % MAP_SIZE;
        pos.y = rand() % MAP_SIZE;
        attempts++;
        
        // evita loop infinito se la mappa è piena di muri
        if(attempts > max_attempts) {
            int i, j;
            for(i = 0; i < MAP_SIZE; i++) {
                for(j = 0; j < MAP_SIZE; j++) {
                    if(map[i][j].type == CELL_FREE) {
                        pos.x = i;
                        pos.y = j;
                        return pos;
                    }
                }
            }
        }
    } while(!map_is_free(map, pos.x, pos.y));
    
    return pos;
}

/* calcola quante celle possiede un giocatore */
int map_count_cells_owned(Cell map[MAP_SIZE][MAP_SIZE], int player_id) {
    int count = 0;
    int i, j;
    
    for(i = 0; i < MAP_SIZE; i++) {
        for(j = 0; j < MAP_SIZE; j++) {
            if(map[i][j].owner_id == player_id) {
                count++;
            }
        }
    }
    
    return count;
}

/* rivela i muri attorno a una posizione */
void map_reveal_walls(Cell map[MAP_SIZE][MAP_SIZE], 
                      int walls_discovered[MAP_SIZE][MAP_SIZE],
                      int x, int y) {
    int i, j;
    
    // scansiona tutte le celle nel raggio VIEW_RADIUS
    for(i = x - VIEW_RADIUS; i <= x + VIEW_RADIUS; i++) {
        for(j = y - VIEW_RADIUS; j <= y + VIEW_RADIUS; j++) {
            // salta celle fuori mappa
            if(i < 0 || i >= MAP_SIZE || j < 0 || j >= MAP_SIZE) {
                continue;
            }
            
            // se è un muro, marcalo come scoperto
            if(map[i][j].type == CELL_WALL) {
                walls_discovered[i][j] = 1;
            }
        }
    }
}

/* stampa la mappa */
void map_print(Cell map[MAP_SIZE][MAP_SIZE]) {
    int i, j;
    
    printf("\n  ");
    for(j = 0; j < MAP_SIZE; j++) {
        printf("%2d", j);
    }
    printf("\n");
    
    for(i = 0; i < MAP_SIZE; i++) {
        printf("%2d ", i);
        for(j = 0; j < MAP_SIZE; j++) {
            if(map[i][j].type == CELL_WALL) {
                printf(" #");
            } else if(map[i][j].owner_id >= 0) {
                printf(" %d", map[i][j].owner_id);
            } else {
                printf(" .");
            }
        }
        printf("\n");
    }
    printf("\n");
}