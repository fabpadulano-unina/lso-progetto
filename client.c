#include "common.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>

/* ===== VARIABILI GLOBALI CLIENT ===== */
Cell local_map[MAP_SIZE][MAP_SIZE];
int my_x, my_y;
int game_active = 0;

/* ===== PROTOTIPI FUNZIONI ===== */
int connect_to_server(const char* ip, int port);
void do_register(int sockfd);
void do_login(int sockfd);
void show_menu();
void handle_server_message(int sockfd);
void display_local_map();
void display_global_map(GlobalMapMsg* global);
void game_loop(int sockfd);

/* ===== MAIN ===== */
int main(int argc, char* argv[]) {
    int sockfd;
    char* server_ip;
    int server_port;
    int choice;
    
    // Verifica argomenti riga di comando
    if(argc != 3) {
        printf("Uso: %s <ip_server> <porta>\n", argv[0]);
        printf("Esempio: %s 127.0.0.1 8888\n", argv[0]);
        exit(1);
    }
    
    server_ip = argv[1];
    server_port = atoi(argv[2]);
    
    // Connessione al server
    sockfd = connect_to_server(server_ip, server_port);
    if(sockfd < 0) {
        printf("Errore: impossibile connettersi al server\n");
        exit(1);
    }
    
    printf("Connesso al server %s:%d\n\n", server_ip, server_port);
    
    // Menu iniziale
    while(!game_active) {
        printf("=== MENU ===\n");
        printf("1. Registrazione\n");
        printf("2. Login\n");
        printf("3. Esci\n");
        printf("Scelta: ");
        
        if(scanf("%d", &choice) != 1) {
            while(getchar() != '\n'); // Pulisci buffer
            continue;
        }
        while(getchar() != '\n'); // Pulisci buffer
        
        switch(choice) {
            case 1:
                do_register(sockfd);
                break;
            case 2:
                do_login(sockfd);
                if(game_active) {
                    game_loop(sockfd);
                }
                break;
            case 3:
                close(sockfd);
                exit(0);
            default:
                printf("Scelta non valida\n");
        }
    }
    
    close(sockfd);
    return 0;
}

/* ===== CONNESSIONE AL SERVER ===== */
int connect_to_server(const char* host, int port) {
    struct addrinfo hints, *res;
    char port_str[16];
    
    sprintf(port_str, "%d", port);
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }
    
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return -1;
    }
    
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sockfd);
        freeaddrinfo(res);
        return -1;
    }
    
    freeaddrinfo(res);
    return sockfd;
}

/* ===== REGISTRAZIONE ===== */
void do_register(int sockfd) {
    AuthMsg auth;
    int msg_type;
    char buffer[256];
    
    printf("\n=== REGISTRAZIONE ===\n");
    printf("Nickname: ");
    fgets(auth.nickname, MAX_NICKNAME, stdin);
    auth.nickname[strcspn(auth.nickname, "\n")] = 0; // Rimuovi \n
    
    printf("Password: ");
    fgets(auth.password, MAX_PASSWORD, stdin);
    auth.password[strcspn(auth.password, "\n")] = 0;
    
    // Invia richiesta registrazione
    send_message(sockfd, MSG_REGISTER, &auth, sizeof(AuthMsg));
    
    // Ricevi risposta
    recv_message(sockfd, &msg_type, buffer, sizeof(buffer));
    
    if(msg_type == MSG_OK) {
        printf("Registrazione completata!\n\n");
    } else {
        printf("Errore: nickname già esistente\n\n");
    }
}

/* ===== LOGIN ===== */
void do_login(int sockfd) {
    AuthMsg auth;
    int msg_type;
    char buffer[4096];
    int len;
    
    printf("\n=== LOGIN ===\n");
    printf("Nickname: ");
    fgets(auth.nickname, MAX_NICKNAME, stdin);
    auth.nickname[strcspn(auth.nickname, "\n")] = 0;
    
    printf("Password: ");
    fgets(auth.password, MAX_PASSWORD, stdin);
    auth.password[strcspn(auth.password, "\n")] = 0;
    
    // Invia richiesta login
    send_message(sockfd, MSG_LOGIN, &auth, sizeof(AuthMsg));
    
    // Ricevi risposta
    len = recv_message(sockfd, &msg_type, buffer, sizeof(buffer));
    
    if(msg_type == MSG_OK) {
        printf("Login effettuato!\n");
        game_active = 1;
        
        // Ricevi mappa locale iniziale
        len = recv_message(sockfd, &msg_type, buffer, sizeof(buffer));
        if(msg_type == MSG_LOCAL_MAP) {
            LocalMapMsg* local = (LocalMapMsg*)buffer;
            my_x = local->your_x;
            my_y = local->your_y;
            
            // Ricevi celle
            Cell cells[local->view_size * local->view_size];
            recv_all(sockfd, cells, sizeof(Cell) * local->view_size * local->view_size);
            
            // Aggiorna mappa locale
            int idx = 0;
            int i, j;
            for(i = my_x - VIEW_RADIUS; i <= my_x + VIEW_RADIUS; i++) {
                for(j = my_y - VIEW_RADIUS; j <= my_y + VIEW_RADIUS; j++) {
                    if(i >= 0 && i < MAP_SIZE && j >= 0 && j < MAP_SIZE) {
                        local_map[i][j] = cells[idx];
                    }
                    idx++;
                }
            }
            
            printf("\nPosizione iniziale: (%d, %d)\n", my_x, my_y);
            display_local_map();
        }
    } else {
        printf("Errore: credenziali errate o già connesso\n\n");
    }
}

/* ===== GAME LOOP ===== */
void game_loop(int sockfd) {
    fd_set read_set;
    struct timeval timeout;
    char cmd;
    int running = 1;
    
    printf("\n=== COMANDI ===\n");
    printf("w = su, s = giù, a = sinistra, d = destra\n");
    printf("l = lista giocatori, m = mostra mappa, q = quit\n\n");
    
    while(running) {
        FD_ZERO(&read_set);
        FD_SET(STDIN_FILENO, &read_set);
        FD_SET(sockfd, &read_set);
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms
        
        int activity = select(sockfd + 1, &read_set, NULL, NULL, &timeout);
        
        if(activity < 0) {
            if(errno == EINTR) continue;
            break;
        }
        
        // Input da tastiera
        if(FD_ISSET(STDIN_FILENO, &read_set)) {
            cmd = getchar();
            
            MoveMsg move;
            int valid_move = 1;
            
            switch(cmd) {
                case 'w': move.direction = DIR_UP; break;
                case 's': move.direction = DIR_DOWN; break;
                case 'a': move.direction = DIR_LEFT; break;
                case 'd': move.direction = DIR_RIGHT; break;
                case 'l':
                    send_simple_message(sockfd, MSG_LIST_PLAYERS);
                    valid_move = 0;
                    break;
                case 'm':
                    display_local_map();
                    valid_move = 0;
                    break;
                case 'q':
                    send_simple_message(sockfd, MSG_QUIT);
                    running = 0;
                    valid_move = 0;
                    break;
                default:
                    valid_move = 0;
            }
            
            if(valid_move) {
                send_message(sockfd, MSG_MOVE, &move, sizeof(MoveMsg));
            }
        }
        
        // Messaggi dal server
        if(FD_ISSET(sockfd, &read_set)) {
            // C'è davvero qualcosa da leggere
            handle_server_message(sockfd);
            
            // Se la connessione è stata chiusa, esci
            if(!game_active) {
                running = 0;
            }
        }
    }
    
    printf("Disconnesso.\n");
    game_active = 0;
}

/* ===== GESTIONE MESSAGGI SERVER ===== */
void handle_server_message(int sockfd) {
    int msg_type;
    char buffer[8192];
    int len;
    
    len = recv_message(sockfd, &msg_type, buffer, sizeof(buffer));
    
    if(len < 0) {
        // Errore di lettura
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            // Nessun dato disponibile (non è un errore grave)
            return;
        }
        printf("Errore di connessione.\n");
        game_active = 0;
        return;
    }
    
    if(len == 0 && msg_type == 0) {
        // Connessione chiusa dal server
        printf("Server disconnesso.\n");
        game_active = 0;
        return;
    }
    
    switch(msg_type) {
        case MSG_OK:
            // Movimento accettato, ricevi mappa locale
            len = recv_message(sockfd, &msg_type, buffer, sizeof(buffer));
            if(msg_type == MSG_LOCAL_MAP) {
                LocalMapMsg* local = (LocalMapMsg*)buffer;
                my_x = local->your_x;
                my_y = local->your_y;
                
                // Ricevi celle
                Cell cells[local->view_size * local->view_size];
                recv_all(sockfd, cells, sizeof(Cell) * local->view_size * local->view_size);
                
                // Aggiorna mappa locale
                int idx = 0;
                int i, j;
                for(i = my_x - VIEW_RADIUS; i <= my_x + VIEW_RADIUS; i++) {
                    for(j = my_y - VIEW_RADIUS; j <= my_y + VIEW_RADIUS; j++) {
                        if(i >= 0 && i < MAP_SIZE && j >= 0 && j < MAP_SIZE) {
                            local_map[i][j] = cells[idx];
                        }
                        idx++;
                    }
                }
                
                printf("Posizione: (%d, %d)\n", my_x, my_y);
                display_local_map();
            }
            break;
            
        case MSG_ERROR:
            printf("Movimento non valido (muro o fuori mappa)\n");
            break;
            
        case MSG_GLOBAL_MAP:
            {
                GlobalMapMsg* global = (GlobalMapMsg*)buffer;
                display_global_map(global);
            }
            break;
            
        case MSG_PLAYERS_LIST:
            {
                PlayersListMsg* list = (PlayersListMsg*)buffer;
                int i;
                printf("\n=== GIOCATORI CONNESSI (%d) ===\n", list->count);
                for(i = 0; i < list->count; i++) {
                    printf("%s - Posizione: (%d,%d) - Celle: %d\n",
                           list->players[i].nickname,
                           list->players[i].x,
                           list->players[i].y,
                           list->players[i].cells_owned);
                }
                printf("\n");
            }
            break;
            
        case MSG_GAME_OVER:
            {
                GameOverMsg* game_over = (GameOverMsg*)buffer;
                int i;
                printf("\n=== PARTITA TERMINATA ===\n");
                printf("Vincitore: %s con %d celle!\n\n",
                       game_over->winner_name, game_over->winner_score);
                printf("Classifica:\n");
                for(i = 0; i < game_over->num_players; i++) {
                    printf("%d. %s - %d celle\n",
                           i+1,
                           game_over->final_ranking[i].nickname,
                           game_over->final_ranking[i].cells_owned);
                }
                printf("\n");
                game_active = 0;
            }
            break;
    }
}

/* ===== VISUALIZZAZIONE MAPPA LOCALE ===== */
void display_local_map() {
    int i, j;
    int start_x = my_x - VIEW_RADIUS;
    int start_y = my_y + VIEW_RADIUS; // Sistema cartesiano
    int end_x = my_x + VIEW_RADIUS;
    int end_y = my_y - VIEW_RADIUS;
    
    printf("\n=== MAPPA LOCALE ===\n");
    
    // Stampa dall'alto verso il basso (sistema cartesiano)
    for(j = start_y; j >= end_y; j--) {
        for(i = start_x; i <= end_x; i++) {
            if(i < 0 || i >= MAP_SIZE || j < 0 || j >= MAP_SIZE) {
                printf(" ?");  // Fuori mappa
                continue;
            }
            
            if(i == my_x && j == my_y) {
                printf(" X");  // Tu sei qui
            } else if(local_map[i][j].type == CELL_WALL) {
                printf(" #");  // Muro
            } else if(local_map[i][j].owner_id >= 0) {
                printf(" %d", local_map[i][j].owner_id);  // Proprietario
            } else {
                printf(" .");  // Libera
            }
        }
        printf("\n");
    }
    printf("\n");
}

/* ===== VISUALIZZAZIONE MAPPA GLOBALE ===== */
void display_global_map(GlobalMapMsg* global) {
    int i, j;
    
    printf("\n=== AGGIORNAMENTO GLOBALE ===\n");
    printf("Tempo rimanente: %d secondi\n", global->time_remaining);
    printf("Giocatori attivi: %d\n\n", global->num_players);
    
    // Mostra info giocatori
    for(i = 0; i < global->num_players; i++) {
        printf("[%d] %s - Pos: (%d,%d) - Celle: %d\n",
               global->players[i].id,
               global->players[i].nickname,
               global->players[i].x,
               global->players[i].y,
               global->players[i].cells_owned);
    }
    
    printf("\n=== MAPPA PROPRIETÀ ===\n");
    
    // Stampa mappa ownership (sistema cartesiano: dall'alto verso il basso)
    for(j = MAP_SIZE - 1; j >= 0; j--) {
        for(i = 0; i < MAP_SIZE; i++) {
            // Controlla se c'è un giocatore qui
            int player_here = -1;
            int k;
            for(k = 0; k < global->num_players; k++) {
                if(global->players[k].x == i && global->players[k].y == j) {
                    player_here = global->players[k].id;
                    break;
                }
            }
            
            if(player_here >= 0) {
                printf(" X");  // Giocatore presente
            } else if(global->ownership[i][j] >= 0) {
                printf(" %d", global->ownership[i][j]);  // Proprietario
            } else {
                printf(" .");  // Nessun proprietario
            }
        }
        printf("\n");
    }
    printf("\n");
}

