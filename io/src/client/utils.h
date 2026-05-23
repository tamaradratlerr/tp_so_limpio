#ifndef UTILS_H_
#define UTILS_H_


#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<commons/log.h>
#include<commons/collections/list.h>


typedef enum
{
	GENERIC,
    MENSAJE,
	PAQUETE,

    STDIN,
	STDOUT,
	SLEEP,
} op_code;

/* Este ya no va mas */
typedef struct
{
	int size;
	void* stream;
} t_buffer;


typedef struct {
    uint32_t size; // Tamaño del payload
    uint32_t offset; // Desplazamiento dentro del payload
    void* stream; // Payload
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;


//void enviar_mensaje(char* mensaje, int socket_cliente);

/* Creación de paquete segun tipo de io */
t_paquete* crear_paquete_io(op_code);

int atender_peticiones_del_KS(int, t_log*);

int recibir_operacion(int);

void agregar_a_paquete(t_paquete*, void*, int);

void* serializar_paquete(t_paquete*, int);

void enviar_mensaje(char*, int);

void enviar_paquete(t_paquete*, int)

t_list* recibir_paquete(int);

void eliminar_paquete(t_paquete*);

void liberar_conexion(int);

#endif /* UTILS_H_ */