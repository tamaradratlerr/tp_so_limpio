#ifndef UTILS_H_
#define UTILS_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<commons/config.h>
#include<string.h>
#include<assert.h>
#include<pthread.h>

#define PUERTO "4444"

typedef enum
{
	LECTURA,
	ESCRITURA
} t_ms_operation;
typedef enum
{
	MENSAJE,
	PAQUETE
} op_code;
typedef struct {
    uint32_t memory_size;	//Tamaño del memory stick en bytes
	void* memory;			// Memoria asignada (malloc)
    t_list* cpus_conectadas;// Lista de CPUs conectadas
} t_memory_stick_globals;

extern t_log* logger;
extern t_memory_stick_globals ms_globals;

int iniciar_servidor(void);
int esperar_cliente(int);

int recibir_operacion(int);
void* recibir_buffer(int*, int);
void recibir_mensaje(int);
t_list* recibir_paquete(int);
void init_memory_stick(uint32_t);
void* atender_cliente(void*);
#endif /* UTILS_H_ */
