 
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


void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

// *** creamos los paquetes para las tres opciones de mensajes *** //
t_paquete* crear_paquete_io(op_code io)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = io;
	crear_buffer(paquete);
	return paquete;
}

int atender_peticiones_del_KS(int fd_conexion, t_log* logger)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));	/* Liberar al final */
	paquete->buffer = malloc(sizeof(t_buffer));		/* Liberar al final */

	/*   DESDE ACA FALTA LO ULTIMO PARA MANEJAR CADA IO */
    t_list* lista;

	/* Leo la IO (Código de operación) que envió el Kernel Scheduler */
	int cod_op = recibir_operacion(fd_conexion);
	char* mseg;
	char* useg;

	/* Realizo la accion de la IO correspondiente */
	switch (cod_op) {
		case SLEEP:
			/*Recibo tiempo T en milisegundos del KS.*/
			recibir_mensaje(fd_conexion, paquete_io);
			mseg = paquete_io->buffer->stream;
			//	pid = paquete_io->buffer->stream; ACA TENGO QUE PEDIR EL PID DE ALGUNA MANERA PARA IMPRIMIRLO.

			/* Ejecuto el tiempo de sleep que me envió el Kernel Scheduler */
			log_info(logger, "## PID: %s - Haciendo sleep por %s milisegundos.", pid, mseg);
			useg = atoi(mseg) * 1000;
			usleep(useg);
			/* Le aviso al KS que fue OK */
			enviar_mensaje("Finalizo OK",fd_conexion);
			break;

		case STDIN:

			/* FALTA */

			lista = recibir_mensaje(fd_conexion);	/* FALTA */
			log_info(logger, "Me llegaron los siguientes valores:\n");
			list_iterate(lista, (void*) iterator);	/* FALTA */
			break;

		case STDOUT:

			/* FALTA */

			lista = recibir_mensaje(fd_conexion);	/* FALTA */
			break;

		case -1:

			log_error(logger, "El cliente se desconectó");
			close(fd_conexion);
			return NULL;

		default:
			log_warning(logger,"IO desconocida.");
			
			/* Deberia avisar al KS que la IO no existe */
			/* FALTA */
			
			break;
	}

	eliminar_paquete(paquete_io);
}

op_code recibir_operacion(int fd) {
    
    op_code cod_op;
    if (recv(fd, &cod_op, sizeof(op_code), MSG_WAITALL) > 0) {
        return cod_op;
    } else {
        close(fd);
        return -1; 
    }
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

void* recibir_buffer(int* size, int socket_cliente)
{
	void * buffer;
	/* Recibo el tamaño del stream */
	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	/* Recibo el stream */
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

void recibir_mensaje(int socket_cliente, t_paquete* paquete_io)
{
	int size;
	int desplazamiento = 0;
	int tamanio;
	
	/* Recibo el stream */
	char* buffer = recibir_buffer(&size, socket_cliente);

	/* Copio el tamaño*/
	paquete_io->buffer->size = size;

	/* Reservo memoria para el stream */
	paquete_io->buffer->stream = malloc(size);

	/* Copio el stream */
	memcpy(paquete_io->buffer->stream, buffer, size);

	free(buffer);
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

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

t_list* recibir_paquete(int socket_cliente)		// ver si es necesario, si no, repito el recibir_mensaje 2 veces y listo.
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
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