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
#include "../../utils/src/global_utils.h" // Aquí ya están los op_code y funciones de red

// 1. Identificadores de las operaciones (Protocolo)




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



//"SWAP contará con un único archivo cuyo path y tamaño serán definidos por SWAP_FILE_PATH y SWAP_FILE_SIZE"
void inicializar_swap(char* path, int tamanio_swap, int tamanio_bloque);

void atender_kernel(int socket_km, t_log* logger);

void manejar_lectura_bloque(int socket_km, t_log* logger);

void manejar_escritura_bloque(int socket_km, t_log* logger);
void enviar_respuesta_simple(int socket_km, op_code respuesta);

#endif /* UTILS_H_ */
