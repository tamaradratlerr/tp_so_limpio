
#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>


typedef enum {
    MENSAJE = 1,
    PAQUETE = 2
} op_code;

typedef struct {
    uint32_t ax, bx, cx, dx, pc;
} t_registros;

typedef struct {
    uint32_t id_segmento;
    uint32_t base;
    uint32_t limite;
} t_segmento;

typedef struct {
    uint32_t pid;
    t_registros registros;
    t_list* tabla_segmentos; 
} t_contexto;

typedef struct {
    uint32_t pid;
    char* path_instrucciones;
} t_proceso;

// GLOBALES
extern t_log* logger;
extern t_config* config;

int iniciar_servidor(char* puerto);
int esperar_cliente(int socket_servidor);
int recibir_operacion(int socket_cliente);
void* recibir_buffer(int* size, int socket_cliente);
void recibir_mensaje(int socket_cliente);
t_list* recibir_paquete(int socket_cliente);

#endif
