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


// def de estructura de paquete que viaja a K.S
typedef enum
{
	//original TP0
    MENSAJE,
	//PAQUETE, 
    
    // *** Comunicacion con K.S *** //
    SLEEP,
    STDIN,
    STDOUT
	
}op_code;

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

int crear_conexion(char* ip, char* puerto);

//creacion de paquete segun tipo de mensaje
t_paquete* crear_sleep(void);
t_paquete* crear_stdin(void);
t_paquete* crear_stdout(void);

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);

void enviar_paquete(t_paquete* paquete, int socket_cliente);

void liberar_conexion(int socket_cliente);

void eliminar_paquete(t_paquete* paquete);

void* serializar_paquete(t_paquete* paquete, int bytes);

#endif /* UTILS_H_ */