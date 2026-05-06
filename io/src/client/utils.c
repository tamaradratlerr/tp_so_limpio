 
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
t_paquete* crear_paquete_io(op_code io)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = io;
	crear_buffer(paquete);
	return paquete;
}

int atender_peticiones_del_KS(int fd_conexion, t_log* logger)
{
	t_paquete* paquete_io = crear_paquete_io(GENERIC);

	/* Quizas esta lista me sirva para STDIN, para buffear el mensaje a enviar al KS*/
    t_list* lista;

	/* Leo la IO que envió el Kernel Scheduler */
	int cod_op = recibir_operacion(fd_conexion, paquete_io);

	/* Realizo la accion de la IO correspondiente */
	switch (cod_op) {
		case SLEEP:
			/*Recibo tiempo T en milisegundos del KS.*/
			
			/* FALTA */

			recibir_paquete(fd_conexion);	/* FALTA */
			break;

		case STDIN:

			/* FALTA */

			lista = recibir_paquete(fd_conexion);	/* FALTA */
			log_info(logger, "Me llegaron los siguientes valores:\n");
			list_iterate(lista, (void*) iterator);	/* FALTA */
			break;

		case STDOUT:

			/* FALTA */

			lista = recibir_paquete(fd_conexion);	/* FALTA */
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

int recibir_operacion(int fd_conexion, t_paquete* paquete_io)
{
	if(recv(fd_conexion, &(paquete_io->codigo_operacion), sizeof(op_code), MSG_WAITALL) > 0)
	{
		return paquete_io;
	}
	else
	{
		eliminar_paquete(paquete_io);
		close(fd_conexion);
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