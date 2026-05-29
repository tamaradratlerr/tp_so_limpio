#ifndef UTILS_H_
#define UTILS_H_

#include <commons/collections/list.h>
#include <commons/log.h> 
#include <commons/string.h>
#include <pthread.h>  
#include "../../utils/src/global_utils.h" // Aquí ya están los op_code y funciones de red
#include <stdint.h>

typedef struct {
    int id;
    int base;
    int limite;
} t_segmento;

typedef struct {
    int pid;
    t_list* instrucciones;    
} t_proceso;

extern t_log* logger; 
extern t_list* lista_contextos;
extern pthread_mutex_t mutex_contextos;
extern t_list* lista_procesos;
extern pthread_mutex_t mutex_procesos;

void inicializar_utils(void);

// Mantén aquí solo las funciones que tú implementaste en tu utils.c
t_contexto* crear_contexto(int pid);
void agregar_contexto(int pid);
void manejar_crear_proceso(int socket_cliente);
int buscar_indice_proceso(int pid);
int buscar_indice_contexto(int pid);
void manejar_finalizar_proceso(int socket_cliente);

void manejar_pedido_instruccion_cpu(int socket_cliente);
void manejar_lectura_memoria(int socket_cliente);
void manejar_escritura_memoria(int socket_cliente);
void manejar_guardar_contexto(int socket_cliente);

#endif