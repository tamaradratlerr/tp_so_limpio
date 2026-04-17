#define _POSIX_C_SOURCE 200112L  
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h> 

// RED

int crear_conexion(char *ip, char* puerto)
{
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo(ip, puerto, &hints, &server_info) != 0) {
        fprintf(stderr, "Error en getaddrinfo\n");
        return -1;
    }

    int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if(socket_cliente == -1) {
        freeaddrinfo(server_info);
        return -1;
    }

    if(connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        perror("ERROR AL CONECTAR"); 
        close(socket_cliente); 
        freeaddrinfo(server_info);
        return -1; 
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

void liberar_conexion(int socket_cliente)
{
    close(socket_cliente);
}


// PAQUETE

void* serializar_paquete(t_paquete* paquete, int bytes)
{
    void * magic = malloc(bytes);
    int desplazamiento = 0;

    memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
    desplazamiento+= sizeof(int);
    memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
    desplazamiento+= sizeof(int);
    memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
    desplazamiento+= paquete->buffer->size;

    return magic;
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
    paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

    memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
    memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

    paquete->buffer->size += tamanio + sizeof(int);
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


void eliminar_paquete(t_paquete* paquete)
{
    free(paquete->buffer->stream);
    free(paquete->buffer);
    free(paquete);
}

// RECEPCION

//esta funcion es para recepcionar op_code
op_code recibir_operacion(int socket_cliente)
{
    op_code cod_op;
    // Intentamos recibir el tamaño de un int (nuestro op_code)
    // MSG_WAITALL asegura que se quede esperando hasta recibir los bytes necesarios
    if (recv(socket_cliente, &cod_op, sizeof(op_code), MSG_WAITALL) > 0)
    {
        return cod_op;
    }
    else
    {
        // Si el recv devuelve 0 o menos, la conexión se cerró o hubo error
        close(socket_cliente);
        printf("la conexión se cerró o hubo error con el opcode");
        return -1; 
    }
}

//abre el paquete y saca los datos.
void* recibir_buffer(int* size, int socket_cliente){
    void * buffer;

    // 1. Recibir el tamaño del buffer
    recv(socket_cliente, size, sizeof(int), MSG_WAITALL);

    // 2. Reservar memoria para el contenido del buffer
    buffer = malloc(*size);

    // 3. Recibir el contenido del buffer (el stream de bytes)
    recv(socket_cliente, buffer, *size, MSG_WAITALL);

    return buffer;
}

//me faltan las de contexto