#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <commons/log.h>

typedef enum
{
    MENSAJE,
    PAQUETE,

    // CPU con el Kernel Scheduler
    CONTEXTO_EJECUTAR,
    INTERRUPT,
    TERMINO_PROCESO,

    // CPU con la Memory Stick
    FETCH_INSTRUCCION,
    LLEGO_INSTRUCCION
} op_code;

typedef struct {
    uint8_t AX, BX, CX, DX;
    uint32_t EAX, EBX, ECX, EDX, SI, DI, PC;
} t_registros;

typedef struct {
    t_registros registros;
    uint32_t limite_Memoria, PID;
} t_contexto;

typedef struct {
    int size;
    void* stream;
} t_buffer;

typedef struct {
    op_code codigo_operacion;
    t_buffer* buffer;
} t_paquete;

// --- Red ---
int crear_conexion(char* ip, char* puerto);
void liberar_conexion(int socket_cliente);

// --- Mensajes Simples ---
// Esta queda para enviar strings planos
void enviar_mensaje(char* mensaje, int socket_cliente);

// --- Paquetes ---
t_paquete* crear_paquete(op_code codigo);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void eliminar_paquete(t_paquete* paquete);

// CAMBIO AQUÍ: Se renombra para evitar conflicto con enviar_mensaje(char*, int)
void enviar_paquete(t_paquete* paquete, int socket_cliente);

// Serialización (Asegurate de que esté declarada aquí para que utils.c la reconozca)
void* serializar_paquete(t_paquete* paquete, int bytes);

// --- Recepción ---
op_code recibir_operacion(int socket_cliente);
void* recibir_buffer(int* size, int socket_cliente);
  
// --- Contexto ---
void enviar_contexto(t_contexto* contexto, int socket_cliente);
t_contexto* recibir_contexto(int socket_cliente);

#endif /* UTILS_H_ */