 
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
			t_paquete* paquete_io = recibir_paquete(fd_conexion);
			
			t_io_sleep* datos = (t_io_sleep*)paquete_io->buffer->stream;
			
			uint32_t pid = datos->pid;
			uint32_t mseg = datos->time;

			log_info(logger, "## PID: %u - Haciendo sleep por %u milisegundos.", pid, mseg);
			
			useconds_t useg = mseg * 1000;
			usleep(useg);
			
			enviar_mensaje("Finalizo OK", fd_conexion);
			
			eliminar_paquete(paquete_io);
			break;

		case STDIN: 1
			t_paquete* paquete_io = recibir_paquete(fd_conexion);
			
			t_io_stdin_recv* datos = (t_io_stdin_recv*)paquete_io->buffer->stream;
			
			uint32_t pid = datos->pid;
			uint32_t bytes_a_leer = datos->bytes_to_read;

			log_info(logger, "## PID: %u - Operación STDIN. Leyendo %u bytes.", pid, bytes_a_leer);
			
			char* buffer_lectura = malloc(bytes_a_leer);
			fgets(buffer_lectura, bytes_a_leer, stdin);
			
			free(buffer_lectura);
			eliminar_paquete(paquete_io);
			break;

		case STDOUT: 
			t_paquete* paquete_io = recibir_paquete(fd_conexion);
			
			void* stream_ptr = paquete_io->buffer->stream;
			
			t_io_stdout* datos = (t_io_stdout*)stream_ptr;
			stream_ptr += sizeof(t_io_stdout);
			
			char* nombre_interfaz = malloc(datos->nombre_length);
			memcpy(nombre_interfaz, stream_ptr, datos->nombre_length);
			stream_ptr += datos->nombre_length;
			
			
			uint32_t bytes_leidos = (uint32_t)((void*)stream_ptr - paquete_io->buffer->stream);
			uint32_t tam_contenido = paquete_io->buffer->size - bytes_leidos;
			
			char* texto_a_imprimir = malloc(tam_contenido + 1);
			memcpy(texto_a_imprimir, stream_ptr, tam_contenido);
			texto_a_imprimir[tam_contenido] = '\0'; // Terminador de string
			
			log_info(logger, "## PID: %u - Operación STDOUT", datos->pid);
			log_info(logger, "Interfaz: %s", nombre_interfaz);
			printf("PID %u (%s) escribio: %s\n", datos->pid, nombre_interfaz, texto_a_imprimir);
			
			enviar_mensaje("Finalizo OK", fd_conexion);
			
			free(nombre_interfaz);
			free(texto_a_imprimir);
			eliminar_paquete(paquete_io);
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