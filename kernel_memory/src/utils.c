#define _POSIX_C_SOURCE 200112L 
#include "utils.h"              
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>


int iniciar_servidor(char* puerto) {
    int socket_servidor;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, puerto, &hints, &servinfo);
    socket_servidor = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
    listen(socket_servidor, SOMAXCONN);

    freeaddrinfo(servinfo);
    return socket_servidor;
}

int esperar_cliente(int socket_servidor) {
    return accept(socket_servidor, NULL, NULL);
}

int recibir_operacion(int socket_cliente) {
    int cod_op;
    if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
        return cod_op;
    close(socket_cliente);
    return -1;
}

void* recibir_buffer(int* size, int socket_cliente) {
    void* buffer;
    if(recv(socket_cliente, size, sizeof(int), MSG_WAITALL) > 0) {
        buffer = malloc(*size);
        recv(socket_cliente, buffer, *size, MSG_WAITALL);
        return buffer;
    }
    return NULL;
}

void recibir_mensaje(int socket_cliente) {
    int size;
    char* buffer = recibir_buffer(&size, socket_cliente);
    log_info(logger, "Socket %d dice: %s", socket_cliente, buffer);
    free(buffer);
}

t_list* recibir_paquete(int socket_cliente) {
    int size;
    int desplazamiento = 0;
    void* buffer = recibir_buffer(&size, socket_cliente);
    t_list* valores = list_create();

    while(desplazamiento < size) {
        int tamanio;
        memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
        desplazamiento += sizeof(int);
        char* valor = malloc(tamanio);
        memcpy(valor, buffer + desplazamiento, tamanio);
        desplazamiento += tamanio;
        list_add(valores, valor);
    }
    free(buffer);
    return valores;
}