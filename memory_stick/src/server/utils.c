#include"utils.h"

t_log* logger;
t_memory_stick_globals ms_globals;

void iterator(char* value) {
	log_info(logger,"%s", value);
}

int iniciar_servidor(void)
{
	int err;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	err = getaddrinfo(NULL, PUERTO, &hints, &servinfo); /* If 0 => ok*/
	if (err){
		log_error(logger, "Error on getaddrinfo.");
		abort();
	}

	// Creamos el socket de escucha del servidor
	int socket_servidor = socket(servinfo->ai_family,
                        servinfo->ai_socktype,
                        servinfo->ai_protocol);
	if(socket_servidor == -1){
		log_error(logger, "Error on create socket.");
		abort();
	}

	// Habilito reutilización de un mismo puerto.
	err = setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));
	if (err == -1){
		log_error(logger, "Error on setsockopt.");
		abort();
	}

	// Asociamos el socket a un puerto
	
	err = bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
	if (err == -1) {
		log_error(logger, "Error on bind.");
		abort();
	}

	err = listen(socket_servidor, SOMAXCONN);
	if(err == -1){
		log_error(logger, "Error on listen.");
		abort();
	}

	// Escuchamos las conexiones entrantes

	freeaddrinfo(servinfo);
	log_trace(logger, "Listo para escuchar a mi cliente");

	return socket_servidor;
}

/* Funcion bloqueante */
int esperar_cliente(int socket_servidor)
{
	// Aceptamos un nuevo cliente
	int socket_cliente = accept(socket_servidor, NULL, NULL);
	if(socket_cliente == -1){
		log_error(logger, "Error on accept.");
		abort();
	}

	log_info(logger, "Se conectó un cliente!");

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

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

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

/*
 * Inicializa las estructuras de memoria del Memory Stick
 * Reserva memoria con malloc()
 */
void init_memory_stick(uint32_t tamanio) {

    ms_globals.memory_size = tamanio;
	ms_globals.memory = malloc(tamanio);
    if (ms_globals.memory == NULL) {
        log_error(logger, "ERROR: No se pudo reservar %d bytes de memoria", tamanio);
    	abort();
    }
    // Inicializar a 0
    memset(ms_globals.memory, 0, tamanio);
    log_info(logger, "Memory Stick inicializado: %d bytes reservados", tamanio);
    // Inicializar lista de CPUs
    ms_globals.cpus_conectadas = list_create();
}

// handler del cliente
void* atender_cliente(void* arg) {
    int cliente_fd = *(int*)arg;
    free(arg);

    t_list* lista;

    while (1) {
        int cod_op = recibir_operacion(cliente_fd);

        switch (cod_op) {
        case MENSAJE:
            recibir_mensaje(cliente_fd);
            break;

        case PAQUETE:
            lista = recibir_paquete(cliente_fd);
            log_info(logger, "Me llegaron los siguientes valores:\n");
            list_iterate(lista, (void*) iterator);
            break;

        case -1:
            log_error(logger, "El cliente se desconecto");
            close(cliente_fd);
            return NULL;

        default:
            log_warning(logger,"Operacion desconocida");
            break;
        }
    }
}
