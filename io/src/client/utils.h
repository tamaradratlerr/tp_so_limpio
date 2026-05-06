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

typedef enum
{
	GENERIC,
    MENSAJE,
	PAQUETE,

    STDIN,
	STDOUT,
	SLEEP,
} op_code;

typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;


// *** llamadas a funciones *** //

//void enviar_mensaje(char* mensaje, int socket_cliente);

/* Creación de paquete segun tipo de io */
t_paquete* crear_paquete_io(op_code);

int atender_peticiones_del_KS(int, t_log*);

int recibir_operacion(int);

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);

void enviar_paquete(t_paquete* paquete, int socket_cliente);

void liberar_conexion(int socket_cliente);

void eliminar_paquete(t_paquete* paquete);

void* serializar_paquete(t_paquete* paquete, int bytes);

void enviar_mensaje(char* mensaje, int socket_cliente);

#endif /* UTILS_H_ */