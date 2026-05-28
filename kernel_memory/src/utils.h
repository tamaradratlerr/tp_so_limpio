#ifndef UTILS_H_
#define UTILS_H_

#include <commons/collections/list.h>
#include <commons/log.h> 
#include <commons/string.h>
#include <pthread.h>  
#include "../../utils/src/global_utils.h" 
#include <stdint.h>

typedef enum {
    NUEVA_CPU,
    NUEVO_KERNEL,
    SOLICITUD_INSTRUCCION,
    LEER_MEMORIA,
    ENVIAR_PROCESO, //recibir proceso de ks
    ESCRIBIR_MEMORIA,
    km_GUARDAR_CONTEXTO,
    ks_INIT_PROC,
    ks_EXIT
} op_code;


typedef struct {
    int id;
    int base;
    int limite;
} t_segmento;


typedef struct {
    int pid;
    t_list* instrucciones;    
} t_proceso;


// Variables Globales (Compartidas)


extern t_log* logger; 

extern t_list* lista_contextos;
extern pthread_mutex_t mutex_contextos;

extern t_list* lista_procesos;
extern pthread_mutex_t mutex_procesos;


void inicializar_utils(void);

int iniciar_servidor(char* puerto);
int esperar_cliente(int socket_servidor);
int recibir_operacion(int socket_cliente);
void* recibir_buffer(int* size, int socket_cliente);
void recibir_mensaje(int socket_cliente);
t_list* recibir_paquete(int socket_cliente);

// Gestión Interna de Procesos y Contextos
t_contexto* crear_contexto(int pid);
void agregar_contexto(int pid);
void manejar_crear_proceso(int socket_cliente);
int buscar_indice_proceso(int pid);
int buscar_indice_contexto(int pid);
void manejar_finalizar_proceso(int socket_cliente);

// Espejos de Ciclo de Instrucción de CPU (Mocks y Reales)
void manejar_pedido_instruccion_cpu(int socket_cliente);
void manejar_lectura_memoria(int socket_cliente);
void manejar_escritura_memoria(int socket_cliente);
void manejar_guardar_contexto(int socket_cliente);

#endif /* UTILS_H_ */