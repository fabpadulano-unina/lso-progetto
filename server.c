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

/* ===== VARIABILI GLOBALI ===== */
Cell game_map[MAP_SIZE][MAP_SIZE];
Player players[MAX_PLAYERS];
time_t game_start_time;
time_t last_update_time;
int game_started = 0;

/* ===== PROTOTIPI FUNZIONI ===== */
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

/* ===== MAIN ===== */
int main(int argc, char* argv[]) {
    int listen_fd;
    fd_set master_set, read_set;
    int max_fd;
    struct timeval timeout;
    int port = SERVER_PORT;
    
    // Inizializza random seed
    srand(time(NULL));
    
    // Inizializza mappa
    map_init(game_map, 20); // 20% muri
    
    // Inizializza giocatori
    players_init(players);
    
    // Crea socket in ascolto
    listen_fd = create_listening_socket(port);
    if(listen_fd < 0) {
        char *msg = "Errore creazione socket\n";
        write(2, msg, strlen(msg));
        exit(1);
    }
    
    // Inizializza set di socket
    FD_ZERO(&master_set);
    FD_SET(listen_fd, &master_set);
    max_fd = listen_fd;
    

    
    // Loop principale
    while(1) {
        read_set = master_set;
        
        // Timeout per select: 1 secondo
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_set, NULL, NULL, &timeout);
        
        if(activity < 0) {
            if(errno == EINTR) continue;
            char *msg = "Errore select\n";
            write(2, msg, strlen(msg));
            break;
        }
        
        // Controlla tutti i socket
        int i;
        for(i = 0; i <= max_fd; i++) {
            if(FD_ISSET(i, &read_set)) {
                if(i == listen_fd) {
                    // Nuova connessione
                    handle_new_connection(listen_fd, &master_set, &max_fd);
                } else {
                    // Messaggio da client
                    handle_client_message(i, &master_set);
                }
            }
        }
        
        // Aggiornamenti periodici
        if(game_started) {
            time_t now = time(NULL);
            
            // Invia update globale ogni UPDATE_INTERVAL secondi
            if(now - last_update_time >= UPDATE_INTERVAL) {
                send_global_update();
                last_update_time = now;
            }
            
            // Controlla fine partita
            check_game_end();
        }
    }
    
    close(listen_fd);
    return 0;
}

/* ===== CREAZIONE SOCKET ===== */
int create_listening_socket(int port) {
    int sockfd;
    struct sockaddr_in addr;
    int opt = 1;
    
    // Crea socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        return -1;
    }
    
    // Opzioni socket
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sockfd);
        return -1;
    }
    
    // Configura indirizzo
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    // Bind
    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    // Listen
    if(listen(sockfd, 1000) < 0) {
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

/* ===== GESTIONE NUOVE CONNESSIONI ===== */
void handle_new_connection(int listen_fd, fd_set* master_set, int* max_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_fd;
    
    // Accetta nuova connessione
    new_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
    
    if(new_fd < 0) {
        return; // Errore accept, ignora
    }
    
    // Aggiungi al master set
    FD_SET(new_fd, master_set);
    
    // Aggiorna max_fd se necessario
    if(new_fd > *max_fd) {
        *max_fd = new_fd;
    }
    
}

/* ===== GESTIONE MESSAGGI CLIENT ===== */
void handle_client_message(int client_fd, fd_set* master_set) {
    int msg_type = 0;
    char buffer[4096];
    int len;
    
    len = recv_message(client_fd, &msg_type, buffer, sizeof(buffer));
    
    if(len < 0 || (len == 0 && msg_type == 0)) {
        // Connessione chiusa o errore
        int player_id = players_find_by_socket(players, client_fd);
        if(player_id >= 0) {
            players_remove(players, player_id);
        }
        
        close(client_fd);
        FD_CLR(client_fd, master_set);
        return;
    }
    
    // Gestisci in base al tipo di messaggio
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
            // Client vuole disconnettersi
            {
                int player_id = players_find_by_socket(players, client_fd);
                if(player_id >= 0) {
                    players_remove(players, player_id);
                }
                close(client_fd); 
                FD_CLR(client_fd, master_set);
            }
            break;
            
        default: { // le parentesi graffe sono per limitare la scope di err_buff
            char err_buff[64];
            int n = sprintf(err_buff, "Messaggio sconosciuto: %d\n", msg_type);
            write(2, err_buff, n);
            break;
        }
    }
}

/* ===== GESTIONE REGISTRAZIONE ===== */
void handle_register(int client_fd, AuthMsg* auth) {
    int result = player_register(auth->nickname, auth->password);
    
    if(result == 0) {
        // Registrazione OK
        send_simple_message(client_fd, MSG_OK);
    } else {
        // Nickname già esistente o errore
        send_simple_message(client_fd, MSG_ERROR);
    }
}

/* ===== GESTIONE LOGIN ===== */
void handle_login(int client_fd, AuthMsg* auth) {
    int result = player_authenticate(auth->nickname, auth->password);
    
    if(result != 1) {
        // Credenziali errate
        send_simple_message(client_fd, MSG_ERROR);
        return;
    }
    
    // Controlla se già in partita
    if(players_find_by_nickname(players, auth->nickname) >= 0) {
        send_simple_message(client_fd, MSG_ERROR);
        return;
    }
    
    // Trova posizione iniziale casuale
    Position start_pos = map_find_random_free_position(game_map);
    
    // Aggiungi giocatore
    int player_id = players_add(players, auth->nickname, client_fd, start_pos);
    
    if(player_id < 0) {
        // Partita piena
        send_simple_message(client_fd, MSG_ERROR);
        return;
    }
    
    // Conquista cella iniziale
    map_set_owner(game_map, start_pos.x, start_pos.y, player_id);
    
    // Rivela muri attorno alla posizione iniziale
    map_reveal_walls(game_map, players[player_id].walls_discovered, 
                     start_pos.x, start_pos.y);
    
    // Invia OK
    send_simple_message(client_fd, MSG_OK);
    
    // Invia mappa locale
    send_local_map(client_fd, player_id);
    
    // Avvia gioco se è il primo giocatore
    if(!game_started) {
        game_started = 1;
        game_start_time = time(NULL);
        last_update_time = time(NULL);
    }
    
   
}

/* ===== GESTIONE MOVIMENTO ===== */
void handle_move(int client_fd, MoveMsg* move) {
    int player_id = players_find_by_socket(players, client_fd);
    
    if(player_id < 0) {
        send_simple_message(client_fd, MSG_ERROR);
        return;
    }
    
    // Calcola nuova posizione
    Position old_pos = players[player_id].pos;
    Position new_pos = old_pos;
    
    switch(move->direction) {
        case DIR_UP:    new_pos.y++; break;  // y aumenta andando su
        case DIR_DOWN:  new_pos.y--; break;  // y diminuisce andando giù
        case DIR_LEFT:  new_pos.x--; break;
        case DIR_RIGHT: new_pos.x++; break;
        default:
            send_simple_message(client_fd, MSG_ERROR);
            return;
    }
    
    // Verifica che la nuova posizione sia valida e libera
    if(!map_is_free(game_map, new_pos.x, new_pos.y)) {
        send_simple_message(client_fd, MSG_ERROR);
        return;
    }
    
    // Aggiorna posizione giocatore
    players[player_id].pos = new_pos;
    
    // Conquista la nuova cella
    map_set_owner(game_map, new_pos.x, new_pos.y, player_id);
    
    // Rivela muri attorno alla nuova posizione
    map_reveal_walls(game_map, players[player_id].walls_discovered, 
                     new_pos.x, new_pos.y);
    
    // Aggiorna punteggio
    int score = map_count_cells_owned(game_map, player_id);
    players_update_score(players, player_id, score);
    
    // Invia OK e mappa locale aggiornata
    send_simple_message(client_fd, MSG_OK);
    send_local_map(client_fd, player_id);
}

/* ===== INVIO MAPPA LOCALE ===== */
void send_local_map(int client_fd, int player_id) {
    LocalMapMsg local_msg;
    Cell cells[(2*VIEW_RADIUS+1) * (2*VIEW_RADIUS+1)];
    int cell_count = 0;
    int i, j;
    
    Position pos = players[player_id].pos;
    
    // Prepara header
    local_msg.your_x = pos.x;
    local_msg.your_y = pos.y;
    local_msg.view_size = 2 * VIEW_RADIUS + 1;
    
    // Raccoglie celle nella finestra VIEW_RADIUS
    for(i = pos.x - VIEW_RADIUS; i <= pos.x + VIEW_RADIUS; i++) {
        for(j = pos.y - VIEW_RADIUS; j <= pos.y + VIEW_RADIUS; j++) {
            Cell cell;
            
            // Se fuori mappa
            if(i < 0 || i >= MAP_SIZE || j < 0 || j >= MAP_SIZE) {
                cell.type = CELL_FREE;  // Tratta come libera (non esiste)
                cell.owner_id = -1;
            } else {
                // Controlla se è un muro
                if(game_map[i][j].type == CELL_WALL) {
                    // È un muro: mostralo SOLO se scoperto
                    if(players[player_id].walls_discovered[i][j]) {
                        cell.type = CELL_WALL;  // Scoperto: mostra il muro
                    } else {
                        cell.type = CELL_FREE;  // NON scoperto: fingi sia libera!
                    }
                    cell.owner_id = -1;  // I muri non hanno proprietario
                } else {
                    // È libera: mostra sempre
                    cell.type = CELL_FREE;
                    cell.owner_id = game_map[i][j].owner_id;
                }
            }
            
            cells[cell_count++] = cell;
        }
    }
    
    // Invia: prima il header, poi le celle
    send_message(client_fd, MSG_LOCAL_MAP, &local_msg, sizeof(LocalMapMsg));
    send_all(client_fd, cells, sizeof(Cell) * cell_count);
}

/* ===== LISTA GIOCATORI ===== */
void handle_list_players(int client_fd) {
    PlayersListMsg list_msg;
    int i, count = 0;
    
    // Raccogli giocatori attivi
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
    
    // Invia lista
    send_message(client_fd, MSG_PLAYERS_LIST, &list_msg, sizeof(PlayersListMsg));
}

/* ===== AGGIORNAMENTO GLOBALE (PERIODICO) ===== */
void send_global_update() {
    GlobalMapMsg global_msg;
    int i, j, count = 0;
    
    // Calcola tempo rimanente
    time_t now = time(NULL);
    int elapsed = now - game_start_time;
    global_msg.time_remaining = GAME_DURATION - elapsed;
    
    if(global_msg.time_remaining < 0) {
        global_msg.time_remaining = 0;
    }
    
    // Raccogli info giocatori
    for(i = 0; i < MAX_PLAYERS; i++) {
        if(players[i].is_playing) {
            PlayerInfo info;
            info.id = players[i].id;
            strcpy(info.nickname, players[i].nickname);
            info.x = players[i].pos.x;
            info.y = players[i].pos.y;
            
            // Ricalcola punteggio aggiornato
            info.cells_owned = map_count_cells_owned(game_map, i);
            players_update_score(players, i, info.cells_owned);
            
            global_msg.players[count++] = info;
        }
    }
    
    global_msg.num_players = count;
    
    // Copia ownership map (solo proprietà, non muri)
    for(i = 0; i < MAP_SIZE; i++) {
        for(j = 0; j < MAP_SIZE; j++) {
            global_msg.ownership[i][j] = game_map[i][j].owner_id;
        }
    }
    
    // Invia a tutti i giocatori attivi
    for(i = 0; i < MAX_PLAYERS; i++) {
        if(players[i].is_playing) {
            send_message(players[i].socket_fd, MSG_GLOBAL_MAP, 
                        &global_msg, sizeof(GlobalMapMsg));
        }
    }
}

/* ===== CONTROLLO FINE PARTITA ===== */
void check_game_end() {
    time_t now = time(NULL);
    int elapsed = now - game_start_time;
    
    if(elapsed >= GAME_DURATION) {
        // Tempo scaduto! Fine partita
        GameOverMsg game_over;
        int i, count = 0;
        int max_score = -1;
        int winner_id = -1;
        
        // Trova vincitore e prepara classifica
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
                }
            }
        }
        
        game_over.num_players = count;
        
        // ORDINA la classifica in ordine decrescente per cells_owned
        int j, k;
        for(j = 0; j < count - 1; j++) {
            for(k = j + 1; k < count; k++) {
                if(game_over.final_ranking[k].cells_owned > game_over.final_ranking[j].cells_owned) {
                    // Swap
                    PlayerInfo temp = game_over.final_ranking[j];
                    game_over.final_ranking[j] = game_over.final_ranking[k];
                    game_over.final_ranking[k] = temp;
                }
            }
        }
        
        if(winner_id >= 0) {
            game_over.winner_id = winner_id;
            strcpy(game_over.winner_name, players[winner_id].nickname);
            game_over.winner_score = max_score;
        } else {
            game_over.winner_id = -1;
            strcpy(game_over.winner_name, "Nessuno");
            game_over.winner_score = 0;
        }
        
        // Invia game over a tutti
        for(i = 0; i < MAX_PLAYERS; i++) {
            if(players[i].is_playing) {
                send_message(players[i].socket_fd, MSG_GAME_OVER, 
                            &game_over, sizeof(GameOverMsg));
            }
        }
        
        // Resetta gioco
        game_started = 0;
        players_init(players);
        map_init(game_map, 20);
    }
}