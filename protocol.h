
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "common.h"

/* ===== FUNZIONI SEND/RECEIVE ===== */

/* Invia un messaggio completo (header + dati) su un socket 
 * Ritorna: 0 se ok, -1 se errore */
int send_message(int sockfd, int msg_type, void* data, int data_len);

/* Riceve un messaggio completo (header + dati) da un socket
 * Ritorna: numero di byte ricevuti del payload, -1 se errore, 0 se connessione chiusa
 * msg_type: viene riempito con il tipo di messaggio ricevuto
 * buffer: buffer dove salvare i dati (deve essere abbastanza grande)
 * buffer_size: dimensione massima del buffer */
int recv_message(int sockfd, int* msg_type, void* buffer, int buffer_size);

/* Invia un messaggio semplice solo con il tipo (nessun dato) */
int send_simple_message(int sockfd, int msg_type);

/* ===== FUNZIONI DI UTILITÀ ===== */

/* Legge esattamente n bytes da un socket (gestisce letture parziali)
 * Ritorna: n se ok, -1 se errore, 0 se connessione chiusa */
int recv_all(int sockfd, void* buffer, int n);

/* Invia esattamente n bytes su un socket (gestisce scritture parziali)
 * Ritorna: n se ok, -1 se errore */
int send_all(int sockfd, void* buffer, int n);

#endif /* PROTOCOL_H */