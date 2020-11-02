
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include "aes256.h"

//#define PORT 54321
#define MAX_EVENTS 41



struct socket_pair_t {
    int socket_in;
    int socket_out;
};

struct socket_pair_t sockets[MAX_EVENTS];
int socket_count = 1;



int key(const char *key, unsigned int **w) {
    unsigned char tmp[32];
    if (strlen(key) == 32) {
        for (int i = 0; i < 32; i++) {
            tmp[i] = (unsigned char)key[i];
        }
    } else if (strlen(key) == 64) {
        for (int i = 0; i < 32; i++) {
            char hex[] = { '0', 'x', 0, 0, 0 };
            hex[2] = key[2 * i];
            hex[3] = key[2 * i + 1];
            tmp[i] = (unsigned char)strtol(hex, NULL, 16);
        }
    } else {
        return 4;
    }
    
    key_expansion256(tmp, w);
    
    return 0;
}

int aes_socket_to_socket(int socket_in, int socket_out, short encrypt, unsigned int *w) {
    // TODO: error
    unsigned char buffer_size = 15;
    char buffer[buffer_size];
    int bytes = 0;
    int sent = 0;
    unsigned char offset = 0;
    
    unsigned char block_size = 16;
    unsigned char crypto_data[block_size];
    
    if (encrypt) {
        do {
            printf("read...\n");
            bytes = read(socket_in, buffer, buffer_size); printf("bytes: %d\n", bytes);
            offset = buffer_size - bytes; printf("offset: %d\n", offset);
            memcpy(crypto_data, (unsigned char *)buffer, bytes); printf("cryptodata <-- buffer\n");
            memset(&crypto_data[bytes], 0, offset); printf("&cryptodata[%d] <-- %dx 0\n", bytes, offset);
            crypto_data[buffer_size] = offset; printf("cryptodata[%d] = %d\n", buffer_size, offset);
            printf("encrypt...\n");
            cipher256(crypto_data, crypto_data, w);
            printf("send...\n");
            sent = 0;
            do {
                sent += write(socket_out, &crypto_data[sent], block_size - sent);
            } while (sent < block_size);
            
        } while (bytes == buffer_size);
    } else {
        do {
            printf("read...\n");
            bytes = 0;
            do {
                bytes += read(socket_in, &crypto_data[bytes], block_size - bytes);
            } while (bytes < block_size); printf("bytes: %d\n", bytes);
            printf("decrypt...\n");
            inv_cipher256(crypto_data, crypto_data, w);
            
            offset = crypto_data[buffer_size]; printf("offset: %d\n", offset);
            memcpy(buffer, (char *)crypto_data, buffer_size - offset); printf("buffer <-- cryptodata[%d]", buffer_size - offset);
            printf("send...\n");
            sent = 0;
            do {
                sent += write(socket_out, &buffer[sent], buffer_size - offset - sent);
            } while (sent < buffer_size - offset);
        } while (offset == 0);
    }
    
    
    /*do {
        if (encrypt) {
            bytes = read(socket_in, buffer, buffer_size);
            offset = buffer_size - bytes;
            memcpy(crypto_data, (unsigned char *)buffer, bytes);
            memcpy(&crypto_data[bytes], 0, offset);
            crypto_data[buffer_size] = offset;
        } else {
            bytes = 0;
            do {
                bytes += read(socket_in, crypto_data, block_size);
            } while (bytes < 16);
            
            
        }
        
        while (sent < bytes)
            sent += write(socket_out, &buffer[sent], bytes - sent);
        //printf("sent: %d\n", sent);
        sent = 0;
        
    } while (bytes == buffer_size);*/
    
    return 0;
}

int main(int argc, char *argv[]) {
    // TODO: errors
    if (argc < 6 || argc > 7) {
        printf("%s -e/d <ip> <port> <endpoint ip> <endpoint port> [<key (32 bit hex)>]\n", argv[0]);
        return 0;
    }
    
    char *default_key = "6cc93c21583532b8aa615d39c3503f485baaca7dbc8913a33a66351c479941bc";
    
    short encrypt = 0;
    char ip[16];
    int port;
    char endpoint_ip[16];
    int endpoint_port;
    unsigned int w[60];
    
    if (strcmp(argv[1], "-e") == 0) {
        encrypt = 1;
    } else if (strcmp(argv[1], "-d") == 0) {
        encrypt = 0;
    } else {
        printf("Invalid argument!\n");
        return 0;
    }
    
    if (inet_addr(argv[2]) == -1) {
        printf("Invalid ip!\n");
        return 0;
    } else {
        memcpy(ip, argv[2], strlen(argv[2]));
    }
    
    if ((port = atoi(argv[3])) == 0) {
        printf("Invalid port!\n");
    }
    
    if (inet_addr(argv[4]) == -1) {
        printf("Invalid ip!\n");
        return 0;
    } else {
        memcpy(endpoint_ip, argv[4], strlen(argv[4]));
    }
    
    if ((endpoint_port = atoi(argv[5])) == 0) {
        printf("Invalid port!\n");
    }
    
    if (argc == 7) {
        if (key(argv[6], &w)) {
            printf("Invalid key!\n");
        }
    } else {
        key(default_key, &w);
    }
    
    for (int i = 0; i < MAX_EVENTS; i++) {
        sockets[i].socket_in = -1;
        sockets[i].socket_out = -1;
    }
    
    // server
    struct sockaddr_in server;
    int server_socket;
    
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    
    server_socket = socket(PF_INET, SOCK_STREAM, 0);
    bind(server_socket, (struct sockaddr *) &server, sizeof(server));
    listen(server_socket, 1);
    
    // kqueue
    
    struct kevent evset;
    struct kevent evlist[MAX_EVENTS] = { 0 };
    int kq, nev;
    
    kq = kqueue();
    
    EV_SET(&evset, server_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    kevent(kq, &evset, 1, NULL, 0, NULL);
    
    for (;;) {
        nev = kevent(kq, NULL, 0, evlist, MAX_EVENTS, NULL);
        
        for (int i = 0; i < MAX_EVENTS; i++) {
            if (evlist[i].flags & EV_ERROR) {
                printf("[error]: EV_ERROR: %s\n", strerror(evlist[i].data));
            } else if (evlist[i].flags & EV_EOF) {
                int fd = evlist[i].ident;
                
                for (int j = 0; j < MAX_EVENTS; j++) {
                    if (sockets[j].socket_in == fd || sockets[j].socket_out == fd) {
                        EV_SET(&evset, sockets[j].socket_in, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                        kevent(kq, &evset, 1, NULL, 0, NULL);
                        
                        EV_SET(&evset, sockets[j].socket_out, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                        kevent(kq, &evset, 1, NULL, 0, NULL);
                        
                        close(sockets[j].socket_in);
                        close(sockets[j].socket_out);
                        
                        sockets[j].socket_in = -1;
                        sockets[j].socket_out = -1;
                        
                        break;
                    }
                }
            } else if (evlist[i].ident == server_socket) {
                int new_connection = accept(server_socket, NULL, NULL);
                
                if (socket_count < MAX_EVENTS - 3) {
                    struct sockaddr_in endpoint;
                    int endpoint_socket;
                    
                    endpoint.sin_family = AF_INET;
                    endpoint.sin_port = htons(endpoint_port);
                    endpoint.sin_addr.s_addr = inet_addr(endpoint_ip);
                    
                    endpoint_socket = socket(PF_INET, SOCK_STREAM, 0);
                    connect(endpoint_socket, (struct sockaddr *) &endpoint, sizeof(endpoint));
                    
                    EV_SET(&evset, new_connection, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
                    kevent(kq, &evset, 1, NULL, 0, NULL);
                    
                    EV_SET(&evset, endpoint_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
                    kevent(kq, &evset, 1, NULL, 0, NULL);
                    
                    for (int j = 0; j < MAX_EVENTS; j++) {
                        if (sockets[j].socket_in == -1 && sockets[j].socket_out == -1) {
                            sockets[j].socket_in = new_connection;
                            sockets[j].socket_out = endpoint_socket;
                            break;
                        }
                    }
                    
                } else {
                    close(new_connection);
                }
                
            } else if (evlist[i].filter == EVFILT_READ) {
                short to_endpoint = 1;
                int socket_read = -1, socket_write = -1;
                socket_read = evlist[i].ident;
                
                for (int j = 0; j < MAX_EVENTS; j++) {
                    if (socket_read == sockets[j].socket_in) {
                        socket_write = sockets[j].socket_out;
                        to_endpoint = 1;
                        break;
                    } else if (socket_read == sockets[j].socket_out) {
                        socket_write = sockets[j].socket_in;
                        to_endpoint = 0;
                        break;
                    }
                }
                
                if (socket_read != -1 && socket_write != -1) {
                    if (to_endpoint)
                        aes_socket_to_socket(socket_read, socket_write, encrypt, w);
                    else
                        aes_socket_to_socket(socket_read, socket_write, !encrypt, w);
                }
            }
        }
    }
    
    
    // test oneway connection
    /*struct sockaddr_in endpoint;
    int endpoint_socket;
    
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(endpoint_port);
    endpoint.sin_addr.s_addr = inet_addr(endpoint_ip);
    
    endpoint_socket = socket(PF_INET, SOCK_STREAM, 0);
    connect(endpoint_socket, (struct sockaddr *) &endpoint, sizeof(endpoint));
    
    int new_connection = accept(server_socket, NULL, NULL);
    
    
    printf("listening for data...\n");
    aes_socket_to_socket(new_connection, endpoint_socket, encrypt, w);
    printf("client --> server\n");
    aes_socket_to_socket(endpoint_socket, new_connection, !encrypt, w);
    printf("client <-- server\n");
    printf("done\n");
    
    close(endpoint_socket);*/
    close(server_socket);
    
    return 0;
}
