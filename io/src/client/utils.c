 
#include "utils.h"


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

int crear_conexion(char *ip, char* puerto)
{
	int err;
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /* 1. Obtener info de dirección */
	err = getaddrinfo(ip, puerto, &hints, &server_info); /*if 0 => ok*/
    if(err) {
        perror("Error on getaddrinfo.");
        abort();
    }

    /* 2. Crear el socket */
    int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if(socket_cliente == -1) {
        freeaddrinfo(server_info);
		perror("Error: socket closed.");
        abort();
    }

    /* 3. Intentar conectar al socket */
	err = connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);
    if(err) {
		perror("Error on connect.");
		close(socket_cliente);
		freeaddrinfo(server_info);
		abort();
    }

	/* aca voy a generar el mensaje de  ## conectado a... y ubicar en utils (modificar firma, sumar logger y nombre de server)*/

    freeaddrinfo(server_info);
    return socket_cliente;
}

// *** funcion enviar mensaje (creo que no es necesaria) *** //

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


// *** creamos los paquetes para las tres opciones de mensajes *** //
t_paquete* crear_sleep(void) 
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = SLEEP;
	crear_buffer(paquete);
	return paquete;
}

t_paquete* crear_stdin(void)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = STDIN;
	crear_buffer(paquete);
	return paquete;
}

t_paquete* crear_stdout(void)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = STDOUT;
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



void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
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