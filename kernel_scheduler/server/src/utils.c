#define _POSIX_C_SOURCE 200112L 
#include "utils.h"              
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>



int iniciar_servidor(void)
{
	int socket_servidor;

	struct addrinfo hints, *servinfo, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, PUERTO, &hints, &servinfo);

	// Creamos el socket de escucha del servidor
    socket_servidor = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    // Asociamos el socket a un puerto
    bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);

    // Escuchamos las conexiones entrantes
    listen(socket_servidor, SOMAXCONN);
	
	freeaddrinfo(servinfo);
	log_info(logger, "Listo para escuchar Clientes");

	return socket_servidor;
}

int esperar_cliente(int socket_servidor)
{
	// Aceptamos un nuevo cliente
	int socket_cliente = accept(socket_servidor, NULL, NULL);
    
    if (socket_cliente == -1) {
        log_info(logger, "Error al aceptar al cliente");
        return -1;
    }

	log_info(logger, "Se conecto un Cliente!");

	return socket_cliente;
}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void* recibir_buffer(int* size, int socket_cliente)
{
    void * buffer;

    // 1. Recibimos el tamaño (bloqueante hasta tener los 4 bytes del int)
    if(recv(socket_cliente, size, sizeof(int), MSG_WAITALL) <= 0) {
        return NULL; // El socket se cerró o hubo error
    }

    buffer = malloc(*size);
    
    // 2. Recibimos el contenido (bloqueante hasta tener todo el stream)
    if(recv(socket_cliente, buffer, *size, MSG_WAITALL) <= 0) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

void recibir_mensaje(int socket_cliente)
{
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);
	log_info(logger, "Me llego el mensaje %s", buffer);
	free(buffer);
}

t_list* recibir_paquete(int socket_cliente)
{
    int size;
    int desplazamiento = 0;
    void * buffer;
    t_list* valores = list_create();
    int tamanio;

    buffer = recibir_buffer(&size, socket_cliente);

    while(desplazamiento < size)
    {
        // 1. Leemos el tamaño del próximo dato (casteando a char*)
        memcpy(&tamanio, (char*)buffer + desplazamiento, sizeof(int));
        desplazamiento += sizeof(int);

        // 2. Reservamos memoria para el dato
        char* valor = malloc(tamanio);
        
        // 3. Leemos el dato en sí
        memcpy(valor, (char*)buffer + desplazamiento, tamanio);
        desplazamiento += tamanio;

        // 4. Lo guardamos en la lista
        list_add(valores, valor);
    }

    free(buffer);
    return valores;
}

void iterator(char* value) {
	log_info(logger,"%s", value);
}


// Auxiliar para buscar la IO por su NOMBRE texto
IO* buscar_io_por_nombre(char* nombre_buscado) {
    IO* io_encontrada = NULL;

    pthread_mutex_lock(&mutex_ios);
    
    // Recorremos la lista elemento por elemento usando las macros de las commons
    for (int i = 0; i < list_size(list_suplementarias->io); i++) {
        IO* una_io = list_get(list_suplementarias->io, i);
        
        if (strcmp(una_io->nombre, nombre_buscado) == 0) {
            io_encontrada = una_io;
            break; // Ya la encontramos, salimos del for
        }
    }
    
    pthread_mutex_unlock(&mutex_ios);

    if (io_encontrada == NULL) {
        log_error(logger, "Kernel Error: No se encontró la interfaz de IO con nombre: %s", nombre_buscado);
    }

    return io_encontrada;
}

// Auxiliar para buscar la IO por su FILE DESCRIPTOR (Socket)
IO* buscar_io_por_fd(int fd_buscado) {
    IO* io_encontrada = NULL;

    pthread_mutex_lock(&mutex_ios);
    
    for (int i = 0; i < list_size(list_suplementarias->io); i++) {
        IO* una_io = list_get(list_suplementarias->io, i);
        
        if (una_io->fd == fd_buscado) {
            io_encontrada = una_io;
            break;
        }
    }
    
    pthread_mutex_unlock(&mutex_ios);

    if (io_encontrada == NULL) {
        log_error(logger, "Kernel Error: No se encontró la interfaz de IO con FD: %d", fd_buscado);
    }

    return io_encontrada;
}