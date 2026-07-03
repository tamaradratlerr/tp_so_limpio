

#ifndef MOCK_H_
#define MOCK_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include "utils.h"          // ajustar nombre real si difiere
#include "../../utils/src/global_utils.h"

/* ============= PROTOCOLO (ajustar si el real difiere) ============= */

typedef enum {
    OK = 0,
    ERROR = 1,
    INIT_PROC = 2
} op_code;

typedef struct {
    int size;
    void* stream;
} t_buffer;

typedef struct {
    op_code codigo_operacion;
    t_buffer* buffer;
} t_paquete;

t_paquete* crear_paquete(op_code code);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void eliminar_paquete(t_paquete* paquete);


#endif