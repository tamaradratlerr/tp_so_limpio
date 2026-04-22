#include "server.h"

int main(int argc, char** argv)
{
	validar_argumentos(argc, argv);
	
	/* Inicio configuración */
	t_config* config	= config_create(argv[1]);
	char* file			= config_get_string_value(config, "SERVER_LOG_NAME");
	char* process_name	= config_get_string_value(config, "PROCCES_NAME"); 
	char* server_port	= config_get_string_value(config, "MEMORY_STICK_PORT");
	/* Creo el logger */
	logger = log_create(file, process_name, 1, LOG_LEVEL_DEBUG);
	if (logger == NULL)
	{
		printf("ERROR: No se pudo cargar la configuración de %s \n", file);
		abort();
	}
	config_destroy(config);

	/* Inicio memoria */
	init_memory_stick(atoi(argv[2]));

	/* Inicio server memory_stick */
	int server_fd = iniciar_servidor(server_port);
	log_info(logger, "Servidor MEMORY_STICK listo para recibir clientes \n");
	
	/*
	Creo un hilo por cada cliente que se conecta.
	*/
	while (1) {
        int socket_cliente = esperar_cliente(server_fd);

        pthread_t thread;

        int* socket_ptr = malloc(sizeof(int));
        *socket_ptr = socket_cliente;

        if (pthread_create(&thread, NULL, atender_cliente, socket_ptr) != 0)
		{
            perror("pthread_create");
            close(socket_cliente);
            free(socket_ptr);
            continue;
        }

        pthread_detach(thread);
	/*
	 * Aca se queda infinitamente reciebiendo clientes, 
	 * hay que poner semáforos después para la concurrencia.
	 */
    }

	return EXIT_SUCCESS;
}



void validar_argumentos(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Error, faltan argumentos. \nUso: %s <arg1> <arg2>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}
