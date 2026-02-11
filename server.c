#include "common.h"
#include "protocol.h"
#include "map.h"
#include "player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include <unistd.h> 

/* variabili globali */
Cell game_map[MAP_SIZE][MAP_SIZE];
Player players[MAX_PLAYERS];
time_t game_start_time;
time_t last_update_time;
int game_started = 0;

/* prototipi */
int create_listening_socket(int port);
void handle_new_connection(int listen_fd, fd_set* master_set, int* max_fd);
void handle_client_message(int client_fd, fd_set* master_set);
void send_global_update();
void handle_register(int client_fd, AuthMsg* auth);
void handle_login(int client_fd, AuthMsg* auth);
void handle_move(int client_fd, MoveMsg* move);
void handle_list_players(int client_fd);
void send_local_map(int client_fd, int player_id);
void check_game_end();


int main(int argc, char* argv[]) {
    int listen_fd;
    fd_set master_set, read_set;
    int max_fd;
    struct timeval timeout;
    int port = SERVER_PORT;
    
    // inizializza random seed
    srand(time(NULL));
    
    map_init(game_map, 20); // 20% muri
    
    players_init(players);
    
    listen_fd = create_listening_socket(port);
    if(listen_fd < 0) {
        char *msg = "Errore creazione socket\n";
        write(2, msg, strlen(msg));
        exit(1);
    }
    
    // inizializza set 
    FD_ZERO(&master_set);
    FD_SET(listen_fd, &master_set);
    max_fd = listen_fd;
    

    
    while(1) {
        read_set = master_set;
        
        // timeout per select di 1 secondo
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_set, NULL, NULL, &timeout);
        
        if(activity < 0) {
            if(errno == EINTR) continue;
            char *msg = "Errore select\n";
            write(2, msg, strlen(msg));
            break;
        }
        
        int i;
        for(i = 0; i <= max_fd; i++) {
            if(FD_ISSET(i, &read_set)) {
                if(i == listen_fd) {
                    handle_new_connection(listen_fd, &master_set, &max_fd);
                } else {
                    handle_client_message(i, &master_set);
                }
            }
        }
        
        // aggiornamenti periodici
        if(game_started) {
            time_t now = time(NULL);
            
            // invia update globale ogni UPDATE_INTERVAL secondi
            if(now - last_update_time >= UPDATE_INTERVAL) {
                send_global_update();
                last_update_time = now;
            }
            
            check_game_end();
        }
    }
    
    close(listen_fd);
    return 0;
}

int create_listening_socket(int port) {
    int sockfd;
    struct sockaddr_in addr;
    int opt = 1;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        return -1;
    }
    
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sockfd);
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    if(listen(sockfd, 1000) < 0) {
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

void handle_new_connection(int listen_fd, fd_set* master_set, int* max_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_fd;
    
    new_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
    
    if(new_fd < 0) {
        return; 
    }
    
    FD_SET(new_fd, master_set);
    
    if(new_fd > *max_fd) {
        *max_fd = new_fd;
    }
    
}


void handle_client_message(int client_fd, fd_set* master_set) {
    int msg_type = 0;
    char buffer[4096];
    int len;
    
    len = recv_message(client_fd, &msg_type, buffer, sizeof(buffer));
    
    if(len < 0 || (len == 0 && msg_type == 0)) {
        // connessione chiusa o errore
        int player_id = players_find_by_socket(players, client_fd);
        if(player_id >= 0) {
            players_remove(players, player_id);
        }
        
        close(client_fd);
        FD_CLR(client_fd, master_set);
        return;
    }
    
    switch(msg_type) {
        case MSG_REGISTER:
            handle_register(client_fd, (AuthMsg*)buffer);
            break;
            
        case MSG_LOGIN:
            handle_login(client_fd, (AuthMsg*)buffer);
            break;
            
        case MSG_MOVE:
            handle_move(client_fd, (MoveMsg*)buffer);
            break;
            
        case MSG_LIST_PLAYERS:
            handle_list_players(client_fd);
            break;
            
        case MSG_QUIT:
            // client vuole disconnettersi
            {
                int player_id = players_find_by_socket(players, client_fd);
                if(player_id >= 0) {
                    players_remove(players, player_id);
                }
                close(client_fd); 
                FD_CLR(client_fd, master_set);
            }
            break;
            
        default: { 
            char err_buff[64];
            int n = sprintf(err_buff, "Messaggio sconosciuto: %d\n", msg_type);
            write(2, err_buff, n);
            break;
        }
    }
}


void handle_register(int client_fd, AuthMsg* auth) {
    int result = player_register(auth->nickname, auth->password);
    
    if(result == 0) {
        send_simple_message(client_fd, MSG_OK);
    } else {
        // nickname già esistente o errore
        send_simple_message(client_fd, MSG_ERROR);
    }
}


void handle_login(int client_fd, AuthMsg* auth) {
    int result = player_authenticate(auth->nickname, auth->password);
    
    if(result != 1) {
        // credenziali errate
        send_simple_message(client_fd, MSG_ERROR);
        return;
    }
    
    // controllo se già in partita
    if(players_find_by_nickname(players, auth->nickname) >= 0) {
        send_simple_message(client_fd, MSG_ERROR);
        return;
    }
    
    // trova posizione iniziale casuale
    Position start_pos = map_find_random_free_position(game_map);
    
    int player_id = players_add(players, auth->nickname, client_fd, start_pos);
    
    if(player_id < 0) {
        send_simple_message(client_fd, MSG_ERROR);
        return;
    }
    
    // conquista cella iniziale
    map_set_owner(game_map, start_pos.x, start_pos.y, player_id);
    
    // rivela muri attorno alla posizione iniziale
    map_reveal_walls(game_map, players[player_id].walls_discovered, 
                     start_pos.x, start_pos.y);
    
    send_simple_message(client_fd, MSG_OK);
    
    send_local_map(client_fd, player_id);
    
    // avvia gioco se è il primo giocatore
    if(!game_started) {
        game_started = 1;
        game_start_time = time(NULL);
        last_update_time = time(NULL);
    }
    
   
}

/* gestione movimento */
void handle_move(int client_fd, MoveMsg* move) {
    int player_id = players_find_by_socket(players, client_fd);
    
    if(player_id < 0) {
        send_simple_message(client_fd, MSG_ERROR);
        return;
    }
    
    Position old_pos = players[player_id].pos;
    Position new_pos = old_pos;
    
    switch(move->direction) {
        case DIR_UP:    new_pos.y++; break; 
        case DIR_DOWN:  new_pos.y--; break;  
        case DIR_LEFT:  new_pos.x--; break;
        case DIR_RIGHT: new_pos.x++; break;
        default:
            send_simple_message(client_fd, MSG_ERROR);
            return;
    }
    
    if(!map_is_free(game_map, new_pos.x, new_pos.y)) {
        send_simple_message(client_fd, MSG_ERROR);
        return;
    }
    
    players[player_id].pos = new_pos;
    
    map_set_owner(game_map, new_pos.x, new_pos.y, player_id);
    
    // rivela muri attorno alla nuova posizione
    map_reveal_walls(game_map, players[player_id].walls_discovered, 
                     new_pos.x, new_pos.y);
    
    // aggiorna punteggio
    int score = map_count_cells_owned(game_map, player_id);
    players_update_score(players, player_id, score);
    
    send_simple_message(client_fd, MSG_OK);
    send_local_map(client_fd, player_id);
}

/* invio mappa locale */
void send_local_map(int client_fd, int player_id) {
    LocalMapMsg local_msg;
    Cell cells[(2*VIEW_RADIUS+1) * (2*VIEW_RADIUS+1)];
    int cell_count = 0;
    int i, j;
    
    Position pos = players[player_id].pos;
    
    // header
    local_msg.your_x = pos.x;
    local_msg.your_y = pos.y;
    local_msg.view_size = 2 * VIEW_RADIUS + 1;
    
    // raccoglie le celle nella finestra view_radius
    for(i = pos.x - VIEW_RADIUS; i <= pos.x + VIEW_RADIUS; i++) {
        for(j = pos.y - VIEW_RADIUS; j <= pos.y + VIEW_RADIUS; j++) {
            Cell cell;
            
            // se fuori mappa
            if(i < 0 || i >= MAP_SIZE || j < 0 || j >= MAP_SIZE) {
                cell.type = CELL_FREE;  // tratta come libera
                cell.owner_id = -1;
            } else {
                if(game_map[i][j].type == CELL_WALL) {
                    // mostralo SOLO se scoperto
                    if(players[player_id].walls_discovered[i][j]) {
                        cell.type = CELL_WALL;  // mostra il muro
                    } else {
                        cell.type = CELL_FREE;  // non scoperto, fingo sia libera
                    }
                    cell.owner_id = -1;  // i muri non hanno proprietario
                } else {
                    // libera
                    cell.type = CELL_FREE;
                    cell.owner_id = game_map[i][j].owner_id;
                }
            }
            
            cells[cell_count++] = cell;
        }
    }
    
    // invio prima l'header, poi le celle
    send_message(client_fd, MSG_LOCAL_MAP, &local_msg, sizeof(LocalMapMsg));
    send_all(client_fd, cells, sizeof(Cell) * cell_count);
}


void handle_list_players(int client_fd) {
    PlayersListMsg list_msg;
    int i, count = 0;
    for(i = 0; i < MAX_PLAYERS; i++) {
        if(players[i].is_playing) {
            PlayerInfo info;
            info.id = players[i].id;
            strcpy(info.nickname, players[i].nickname);
            info.x = players[i].pos.x;
            info.y = players[i].pos.y;
            info.cells_owned = players[i].cells_owned;
            
            list_msg.players[count++] = info;
        }
    }
    
    list_msg.count = count;
    
    // invia lista
    send_message(client_fd, MSG_PLAYERS_LIST, &list_msg, sizeof(PlayersListMsg));
}

void send_global_update() {
    GlobalMapMsg global_msg;
    int i, j, count = 0;
    
    // calcola tempo rimanente
    time_t now = time(NULL);
    int elapsed = now - game_start_time;
    global_msg.time_remaining = GAME_DURATION - elapsed;
    
    if(global_msg.time_remaining < 0) {
        global_msg.time_remaining = 0;
    }
    
    // raccogli info giocatori
    for(i = 0; i < MAX_PLAYERS; i++) {
        if(players[i].is_playing) {
            PlayerInfo info;
            info.id = players[i].id;
            strcpy(info.nickname, players[i].nickname);
            info.x = players[i].pos.x;
            info.y = players[i].pos.y;
            
            // ricalcola punteggio aggiornato
            info.cells_owned = map_count_cells_owned(game_map, i);
            players_update_score(players, i, info.cells_owned);
            
            global_msg.players[count++] = info;
        }
    }
    
    global_msg.num_players = count;
    
    // copia ownership map (solo proprietà, non muri)
    for(i = 0; i < MAP_SIZE; i++) {
        for(j = 0; j < MAP_SIZE; j++) {
            global_msg.ownership[i][j] = game_map[i][j].owner_id;
        }
    }
    
    for(i = 0; i < MAX_PLAYERS; i++) {
        if(players[i].is_playing) {
            send_message(players[i].socket_fd, MSG_GLOBAL_MAP, 
                        &global_msg, sizeof(GlobalMapMsg));
        }
    }
}

void check_game_end() {
    time_t now = time(NULL);
    int elapsed = now - game_start_time;
    
    if(elapsed >= GAME_DURATION) {
        // tempo scaduto

        GameOverMsg game_over;
        int i, count = 0;
        int max_score = -1;
        int winner_id = -1;
        int tie = 0; // variabile per tracciare il pareggio
        
        // trova vincitore e prepara classifica
        for(i = 0; i < MAX_PLAYERS; i++) {
            if(players[i].is_playing) {
                int score = map_count_cells_owned(game_map, i);
                
                PlayerInfo info;
                info.id = i;
                strcpy(info.nickname, players[i].nickname);
                info.x = players[i].pos.x;
                info.y = players[i].pos.y;
                info.cells_owned = score;
                
                game_over.final_ranking[count++] = info;
                
                if(score > max_score) {
                    max_score = score;
                    winner_id = i;
                    tie = 0; // nuovo massimo trovato
                } else if(score == max_score && max_score > 0) {
                    tie = 1; // trovato un altro giocatore con lo stesso punteggio massimo
                }
            }
        }
        
        game_over.num_players = count;
        
        // ORDINA la classifica in ordine decrescente per cells_owned
        int j, k;
        for(j = 0; j < count - 1; j++) {
            for(k = j + 1; k < count; k++) {
                if(game_over.final_ranking[k].cells_owned > game_over.final_ranking[j].cells_owned) {
                    PlayerInfo temp = game_over.final_ranking[j];
                    game_over.final_ranking[j] = game_over.final_ranking[k];
                    game_over.final_ranking[k] = temp;
                }
            }
        }
        

        if(winner_id >= 0 && tie == 0) {
            game_over.winner_id = winner_id;
            strcpy(game_over.winner_name, players[winner_id].nickname);
            game_over.winner_score = max_score;
        } else {
            // caso di pareggio o punteggio 0, nessun vincitore
            game_over.winner_id = -1;
            strcpy(game_over.winner_name, "Nessuno");
            game_over.winner_score = (max_score > 0) ? max_score : 0;
        }
        
        // invia game over a tutti
        for(i = 0; i < MAX_PLAYERS; i++) {
            if(players[i].is_playing) {
                send_message(players[i].socket_fd, MSG_GAME_OVER, 
                            &game_over, sizeof(GameOverMsg));
            }
        }
        
        // resetta gioco
        game_started = 0;
        players_init(players);
        map_init(game_map, 20);
    }
}