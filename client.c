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


Cell local_map[MAP_SIZE][MAP_SIZE];
int my_x, my_y;
int game_active = 0;

int connect_to_server(const char* ip, int port);
void do_register(int sockfd);
void do_login(int sockfd);
void show_menu();
void handle_server_message(int sockfd);
void display_local_map();
void display_global_map(GlobalMapMsg* global);
void game_loop(int sockfd);

ssize_t read_line(int fd, char *buffer, size_t n) {
    ssize_t num_read;
    size_t tot_read = 0;
    char ch;

    if (n <= 0 || buffer == NULL) return -1;

    while (tot_read < n - 1) {
        num_read = read(fd, &ch, 1); 
        
        if (num_read == -1) {
            return -1; 
        } else if (num_read == 0) {
            break; // EOF 
        } else {
            buffer[tot_read++] = ch;
            if (ch == '\n') break; 
        }
    }

    buffer[tot_read] = '\0'; 
    return tot_read;
}

int main(int argc, char* argv[]) {
    int sockfd;
    char* server_ip;
    int server_port;
    int choice;
    
    //verifica argomenti riga di comando
    if(argc != 3) {
        printf("Uso: %s <ip_server> <porta>\n", argv[0]);
        printf("Esempio: %s 127.0.0.1 8888\n", argv[0]);
        exit(1);
    }
    
    server_ip = argv[1];
    server_port = atoi(argv[2]);
    
   
    sockfd = connect_to_server(server_ip, server_port);
    if(sockfd < 0) {
        printf("Errore: impossibile connettersi al server\n");
        exit(1);
    }
    
    printf("Connesso al server %s:%d\n\n", server_ip, server_port);
    
    // menu
    while(!game_active) {
        printf("=== MENU ===\n");
        printf("1. Registrazione\n");
        printf("2. Login\n");
        printf("3. Esci\n");
        printf("Scelta: ");
        
        if(scanf("%d", &choice) != 1) {
            while(getchar() != '\n'); // pulisci buffer
            continue;
        }
        while(getchar() != '\n'); // pulisci buffer
        
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


int connect_to_server(const char* host, int port) {
    struct addrinfo hints, *res;
    char port_str[16];
    char err_buff[256]; 
    
    sprintf(port_str, "%d", port);
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err != 0) {
        int n = sprintf(err_buff, "getaddrinfo: %s\n", gai_strerror(err));
        write(2, err_buff, n);
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


void do_register(int sockfd) {
    AuthMsg auth;
    int msg_type;
    char buffer[256];
    
    printf("\n=== REGISTRAZIONE ===\n");
    printf("Nickname: ");
    fflush(stdout); 
    
    read_line(STDIN_FILENO, auth.nickname, MAX_NICKNAME);
    auth.nickname[strcspn(auth.nickname, "\n")] = 0; 
    
    printf("Password: ");
    fflush(stdout);
    
    read_line(STDIN_FILENO, auth.password, MAX_PASSWORD);
    auth.password[strcspn(auth.password, "\n")] = 0;
    
    // invia richiesta registrazione
    send_message(sockfd, MSG_REGISTER, &auth, sizeof(AuthMsg));
    
    recv_message(sockfd, &msg_type, buffer, sizeof(buffer));
    
    if(msg_type == MSG_OK) {
        printf("Registrazione completata!\n\n");
    } else {
        printf("Errore: nickname già esistente\n\n");
    }
}


void do_login(int sockfd) {
    AuthMsg auth;
    int msg_type;
    char buffer[4096];
    int len;
    
    printf("\n=== LOGIN ===\n");
    printf("Nickname: ");
    fflush(stdout);
    
    read_line(STDIN_FILENO, auth.nickname, MAX_NICKNAME);
    auth.nickname[strcspn(auth.nickname, "\n")] = 0;
    
    printf("Password: ");
    fflush(stdout);
    
    read_line(STDIN_FILENO, auth.password, MAX_PASSWORD);
    auth.password[strcspn(auth.password, "\n")] = 0;
    
    // invia richiesta login
    send_message(sockfd, MSG_LOGIN, &auth, sizeof(AuthMsg));
    
    
    len = recv_message(sockfd, &msg_type, buffer, sizeof(buffer));
    
    if(msg_type == MSG_OK) {
        printf("Login effettuato!\n");
        game_active = 1;
        
        // ricevi mappa locale iniziale
        len = recv_message(sockfd, &msg_type, buffer, sizeof(buffer));
        if(msg_type == MSG_LOCAL_MAP) {
            LocalMapMsg* local = (LocalMapMsg*)buffer;
            my_x = local->your_x;
            my_y = local->your_y;
            
            // ricevi celle
            Cell cells[local->view_size * local->view_size];
            recv_all(sockfd, cells, sizeof(Cell) * local->view_size * local->view_size);
            
            // aggiorna mappa locale
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
        

        if(FD_ISSET(STDIN_FILENO, &read_set)) {
            int n = read(STDIN_FILENO, &cmd, 1);
            
            if (n > 0) {
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
        }
        
        // messaggi dal server
        if(FD_ISSET(sockfd, &read_set)) {
            // c'è davvero qualcosa da leggere
            handle_server_message(sockfd);
            
            // se la connessione è stata chiusa, esci
            if(!game_active) {
                running = 0;
            }
        }
    }
    
    printf("Disconnesso.\n");
    game_active = 0;
}


void handle_server_message(int sockfd) {
    int msg_type;
    char buffer[100000];
    int len;
    
    len = recv_message(sockfd, &msg_type, buffer, sizeof(buffer));
    
    if(len < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            // nessun dato disponibile (non è un errore grave)
            return;
        }
        printf("Errore di connessione.\n");
        game_active = 0;
        return;
    }
    
    if(len == 0 && msg_type == 0) {
        // connessione chiusa dal server
        printf("Server disconnesso.\n");
        game_active = 0;
        return;
    }
    
    switch(msg_type) {
        case MSG_OK:
            // movimento accettato, ricevo mappa locale
            len = recv_message(sockfd, &msg_type, buffer, sizeof(buffer));
            if(msg_type == MSG_LOCAL_MAP) {
                LocalMapMsg* local = (LocalMapMsg*)buffer;
                my_x = local->your_x;
                my_y = local->your_y;
                Cell cells[local->view_size * local->view_size];
                recv_all(sockfd, cells, sizeof(Cell) * local->view_size * local->view_size);
                
                // aggiorna mappa locale
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
                
                // pulisce un po' lo schermo
                printf("\n\n");
                printf("**********************************\n");
                printf("* PARTITA TERMINATA        *\n");
                printf("**********************************\n\n");
                
                if (game_over->winner_id != -1) {
                     printf("   VINCITORE: %s \n", game_over->winner_name);
                     printf("   PUNTEGGIO: %d celle conquistate\n", game_over->winner_score);
                } else {
                     printf("   NESSUN VINCITORE (Pareggio o nessuno ha giocato)\n");
                }
                
                printf("\n--- CLASSIFICA FINALE ---\n");
                for(i = 0; i < game_over->num_players; i++) {
                    printf("%d) %s: %d celle\n", 
                            i + 1, 
                            game_over->final_ranking[i].nickname, 
                            game_over->final_ranking[i].cells_owned);
                }
                printf("-------------------------\n");
                printf("\nPartita terminata. Premi Invio per uscire...");
                game_active = 0;
            }
            break;
    }
}


void display_local_map() {
    int i, j;
    int start_x = my_x - VIEW_RADIUS;
    int start_y = my_y + VIEW_RADIUS; 
    int end_x = my_x + VIEW_RADIUS;
    int end_y = my_y - VIEW_RADIUS;
    
    printf("\n=== MAPPA LOCALE ===\n");
    

    for(j = start_y; j >= end_y; j--) {
        for(i = start_x; i <= end_x; i++) {
            if(i < 0 || i >= MAP_SIZE || j < 0 || j >= MAP_SIZE) {
                printf(" ?");  // fuori mappa
                continue;
            }
            if(i == my_x && j == my_y) {
                printf(" X");  
            } else if(local_map[i][j].type == CELL_WALL) {
                printf(" #");  // muro
            } else if(local_map[i][j].owner_id >= 0) {
                printf(" %d", local_map[i][j].owner_id);  
            } else {
                printf(" .");  // libera
            }
        }
        printf("\n");
    }
    printf("\n");
}

void display_global_map(GlobalMapMsg* global) {
    int i, j;
    
    printf("\n=== AGGIORNAMENTO GLOBALE ===\n");
    printf("Tempo rimanente: %d secondi\n", global->time_remaining);
    printf("Giocatori attivi: %d\n\n", global->num_players);
    
    // mostra info giocatori
    for(i = 0; i < global->num_players; i++) {
        printf("[%d] %s - Pos: (%d,%d) - Celle: %d\n",
               global->players[i].id,
               global->players[i].nickname,
               global->players[i].x,
               global->players[i].y,
               global->players[i].cells_owned);
    }
    
    printf("\n=== MAPPA PROPRIETÀ ===\n");
    
    // stampa mappa ownership 
    for(j = MAP_SIZE - 1; j >= 0; j--) {
        for(i = 0; i < MAP_SIZE; i++) {
            // controlla se c'è un giocatore qui
            int player_here = -1;
            int k;
            for(k = 0; k < global->num_players; k++) {
                if(global->players[k].x == i && global->players[k].y == j) {
                    player_here = global->players[k].id;
                    break;
                }
            }
            
            if(player_here >= 0) {
                printf(" X");  // giocatore presente
            } else if(global->ownership[i][j] >= 0) {
                printf(" %d", global->ownership[i][j]);  // proprietario
            } else {
                printf(" .");  // nessun proprietario
            }
        }
        printf("\n");
    }
    printf("\n");
}

