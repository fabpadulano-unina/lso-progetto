
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "common.h"

/* invia un messaggio completo (header + dati) */
int send_message(int sockfd, int msg_type, void* data, int data_len);

/* Riceve un messaggio completo (header + dati) da un socket, ritorna il numero di byte ricevuti del payload, -1 se errore, 0 se connessione chiusa */
int recv_message(int sockfd, int* msg_type, void* buffer, int buffer_size);

/* invia un messaggio semplice senza dati */
int send_simple_message(int sockfd, int msg_type);

/* legge esattamente n bytes da un socket */
int recv_all(int sockfd, void* buffer, int n);

/* invia esattamente n bytes su un socket */
int send_all(int sockfd, void* buffer, int n);

#endif 