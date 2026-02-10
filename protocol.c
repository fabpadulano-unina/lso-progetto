#include "protocol.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* legge esattamente n bytes da un socket */
int recv_all(int sockfd, void* buffer, int n) {
    int received = 0;
    char* p = (char*)buffer;
    int r;
    
    while(received < n) {
        r = recv(sockfd, p + received, n - received, 0);
        
        if(r > 0) {
            received += r;
            continue;
        }
        
        if(r == 0) {
            return received; // EOF, peer ha chiuso
        }
        
        // r < 0: errore
        if(errno == EINTR) {
            continue; // interruzione da segnale, riprova
        }
        
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return -2; 
        }
        
        return -1; // altro errore
    }
    
    return received; 
}

/* invia esattamente n bytes su un socket */
int send_all(int sockfd, void* buffer, int n) {
    int sent = 0;
    char* p = (char*)buffer;
    int w;
    
    while(sent < n) {
        w = send(sockfd, p + sent, n - sent, 0);
        
        if(w > 0) {
            sent += w;
            continue;
        }
        
        if(w == 0) {
            return -1; 
        }
        
        // w < 0 errore
        if(errno == EINTR) {
            continue; // interruzione da segnale, riprova
        }
        
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return -2; // non può scrivere ora (timeout / non-blocking)
        }
        
        return -1; // altro errore
    }
    
    return sent; 
}

/* invia un messaggio completo (header + dati) */
int send_message(int sockfd, int msg_type, void* data, int data_len) {
    MsgHeader header;
    
    header.type = msg_type;
    header.length = data_len;
    
    // invia header
    if(send_all(sockfd, &header, sizeof(MsgHeader)) != sizeof(MsgHeader)) {
        return -1;
    }
    
    // invia dati (se presenti)
    if(data_len > 0 && data != NULL) {
        if(send_all(sockfd, data, data_len) != data_len) {
            return -1;
        }
    }
    
    return 0;
}

/* riceve un messaggio completo (header + dati) */
int recv_message(int sockfd, int* msg_type, void* buffer, int buffer_size) {
    MsgHeader header;
    int result;
    
    // ricevi header
    result = recv_all(sockfd, &header, sizeof(MsgHeader));
    
    if(result <= 0) {
        return result;  // 0 = connessione chiusa, -1 = errore
    }
    
    *msg_type = header.type;
    
    // se non ci sono dati, ritorna 0
    if(header.length == 0) {
        return 0;
    }
    
    // controlla che il buffer sia abbastanza grande
    if(header.length > buffer_size) {
        return -1;
    }
    
    result = recv_all(sockfd, buffer, header.length);
    
    if(result <= 0) {
        return result;
    }
    
    return header.length;
}

/* invia un messaggio semplice senza dati */
int send_simple_message(int sockfd, int msg_type) {
    return send_message(sockfd, msg_type, NULL, 0);
}