#ifndef UTILS_H_
#define UTILS_H_

#include <commons/collections/list.h>
#include <commons/log.h> 
#include <commons/string.h>
#include <pthread.h>     

// Estructuras del Sistema (typedef struct)
typedef struct {
    int ax, bx, cx, dx;
    int pc; 
} t_registros;

typedef struct {
    int id;
    int base;
    int limite;
} t_segmento;

typedef struct {
    int pid;
    t_registros registros;    
    t_list* tabla_segmentos;  
} t_contexto;

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

t_contexto* crear_contexto(int pid);
void agregar_contexto(int pid);
void manejar_crear_proceso(int socket_cliente);

int buscar_indice_proceso(int pid);
int buscar_indice_contexto(int pid);
void manejar_finalizar_proceso(int socket_cliente);
void manejar_pedido_instruccion_cpu(int socket_cliente);


#endif /* UTILS_H_ */