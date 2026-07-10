#include"utils.h"


// DESP VER QUE BORRAR Y AGREGAR 


t_log* logger;
t_memory_stick_globals ms_globals;

void iterator(char* value) {
	log_info(logger,"%s", value);
}

int iniciar_servidor(char* server_port)
{
	int err;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	err = getaddrinfo(NULL, server_port, &hints, &servinfo); /* If 0 => ok*/
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

/* AGREGAR los free, segun se vayan agregando variables globales */
void free_all_globals(void)
{
	free(ms_globals.memory);
	list_destroy(ms_globals.cpus_conectadas);
}