#define _POSIX_C_SOURCE 200112L  
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h> 

void* serializar_paquete(t_paquete* paquete, int bytes)
{
    void * magic = malloc(bytes);
    int desplazamiento = 0;

    // 1. Código de operación
    // Usamos (char*) para que la suma sea byte a byte
    memcpy((char*)magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
    desplazamiento += sizeof(int);

    // 2. Tamaño del buffer
    memcpy((char*)magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
    desplazamiento += sizeof(int);

    // 3. El contenido real del buffer
    memcpy((char*)magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
    desplazamiento += paquete->buffer->size;

    return magic;
}

int crear_conexion(char *ip, char* puerto)
{
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // 1. Obtener info de dirección
    if(getaddrinfo(ip, puerto, &hints, &server_info) != 0) {
        fprintf(stderr, "Error en getaddrinfo\n");
        return -1;
    }

    // 2. Crear el socket
    int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if(socket_cliente == -1) {
        freeaddrinfo(server_info);
        return -1;
    }

    // 3. Intentar conectar
    if(connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        // Si entra acá, la conexión NO se realizó
        perror("ERROR AL CONECTAR"); 
        close(socket_cliente); // Cerramos el socket porque no sirve para nada
        freeaddrinfo(server_info);
        return -1; // Devolvemos -1 para avisar al main que falló
    }

    freeaddrinfo(server_info);
    return socket_cliente;
}

void enviar_mensaje(char* mensaje, int socket_cliente)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}


void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(void)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = PAQUETE;
	crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
    // 1. Usamos un puntero temporal por seguridad
    void* nuevo_stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));
    if (nuevo_stream == NULL) {
        // Manejar error de memoria si fuera necesario
        return; 
    }
    paquete->buffer->stream = nuevo_stream;

    // 2. Copiamos el tamaño del dato que estamos agregando
    memcpy((char*)paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
    
    // 3. Copiamos el dato real justo después de su tamaño
    memcpy((char*)paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

    // 4. Actualizamos el tamaño total acumulado en el buffer
    paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}
