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
int connect_to_server(const char* ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;
    
    // Crea socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        return -1;
    }
    
    // Configura indirizzo server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if(inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        close(sockfd);
        return -1;
    }
    
    // Connetti
    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
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
