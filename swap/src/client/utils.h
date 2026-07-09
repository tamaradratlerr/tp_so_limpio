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
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h> //para usar mutex
#include <commons/collections/list.h> //para listas

// 1. Identificadores de las operaciones (Protocolo)

typedef enum {
    MENSAJE,
    PAQUETE,
    HANDSHAKE_SWAP,
    LECTURA_BLOQUE,
    ESCRITURA_BLOQUE,
    RESPUESTA_OK,
    RESPUESTA_ERROR,
    RESPUESTA_DATOS
} op_code;

//variables globales


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


// especificos swap

// TAD para que SWAP informe al inicio (Tamaño total y de bloque)

typedef struct {

    int tamanio_total;

    int tamanio_bloque;

} t_handshake_swap;



// 3. TAD para Lectura/Escritura (Número de bloque y datos)

typedef struct {

    int numero_bloque;

    int bloque_size;   // Cuántos bytes mide el contenido (BLOCK_SIZE)

    void* contenido;   // Los datos del proceso

} t_paquete_bloque;


int crear_conexion(char* ip, char* puerto);
void enviar_mensaje(char* mensaje, int socket_cliente);

t_paquete* crear_paquete(void);

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void liberar_conexion(int socket_cliente);
void eliminar_paquete(t_paquete* paquete);

int recibir_operacion(int socket_cliente);
void* recibir_buffer(int* size, int socket_cliente);
t_list* recibir_paquete(int socket_cliente);


//"SWAP contará con un único archivo cuyo path y tamaño serán definidos por SWAP_FILE_PATH y SWAP_FILE_SIZE"
void inicializar_swap(char* path, int tamanio_swap, int tamanio_bloque);

void atender_kernel(int socket_km, t_log* logger);

void manejar_lectura_bloque(int socket_km, t_log* logger);

void manejar_escritura_bloque(int socket_km, t_log* logger);
void enviar_respuesta_simple(int socket_km, op_code respuesta);

#endif /* UTILS_H_ */
