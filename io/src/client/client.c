#include "client.h"
t_log* logger;

int main(int argc, char** argv)
{
	validar_argumentos(argc, argv);

	int fd_conexion;
	char* clave;
	char* io_ip; 
	char* io_port; 
	char* log_level;
	char* log_file;
	char* log_process_name;
	char* log_is_active_console;
	char* io = argv[2];

	/* Inicio configuración */
	t_config* io_config = iniciar_config(argv[1]);

	clave 					= config_get_string_value(io_config, "CLAVE");
	io_ip					= config_get_string_value(io_config, "IO_IP");		/* Ip del KS */
	io_port					= config_get_string_value(io_config, "IO_PORT");	/* Port del KS */
	log_level				= config_get_string_value(io_config, "LOG_LEVEL");
	log_file				= config_get_string_value(io_config, "LOG_FILE");
	log_process_name		= config_get_string_value(io_config, "LOG_PROCESS_NAME");
	log_is_active_console	= config_get_string_value(io_config, "LOG_IS_ACTIVE_CONSOLE");
	
	/* Inicio Logger */
	logger = iniciar_logger(log_level, log_file, log_process_name, log_is_active_console);
	log_info(logger, "Logger iniciado.");
	log_info(logger, "Ip del KS: %s", io_ip);
	log_info(logger, "Puerto KS: %s", io_port);


	/* Conexion a servidor */
	log_info(logger, "Iniciando conexión con el servidor...");
	fd_conexion = crear_conexion(io_ip, io_port, logger, KERNEL_SCHEDULER);

	/* Una vez conectados, quedamos a la espera de mensajes del KERNEL SCHEDULER.*/
	log_info(logger, "Esperando peticiones IO desde %s", getModuleName(KERNEL_SCHEDULER));
	
	while (1)
    {
		/* Bucle principal de la IO */
		enviar_op_code(IO_LIBRE, fd_conexion);

		op_code cod_op = recibir_op_code(fd_conexion);
		

	switch (cod_op) {
		case gl_IO_SLEEP: {
			t_list* lista = recibir_paquete(fd_conexion);
			
			// Extraemos los datos directamente de la lista
			uint32_t pid = *(uint32_t*)list_get(lista, 0);
			uint32_t mseg = *(uint32_t*)list_get(lista, 1);

			log_info(logger, "## PID: %u - Haciendo sleep por %u milisegundos.", pid, mseg);
			
			usleep(mseg * 1000);
			
			enviar_op_code(IO_SLEEP, fd_conexion);
			if(recibir_op_code(fd_conexion) != OK){
				log_info (logger, "Error al responder SLEEP");
				return EXIT_FAILURE;
			}

			enviar_pid(pid, fd_conexion);
			list_destroy_and_destroy_elements(lista, free);
			break;
    	}

    	case gl_IO_STDIN: {
			
			enviar_op_code(OK,fd_conexion);
			
			t_list* lista = recibir_paquete(fd_conexion);
			
			uint32_t pid = *(uint32_t*)list_get(lista, 0);
			uint32_t direccion_logica = *(uint32_t*)list_get(lista, 1);
			uint32_t bytes_a_leer = *(uint32_t*)list_get(lista, 2);

			log_info(logger, "## PID: %u - Operación STDIN. Leyendo %u bytes.", pid, bytes_a_leer);
			
			char* entrada = malloc(bytes_a_leer + 2);   // +2 por '\n' y '\0'
			char* buffer_usuario = calloc(bytes_a_leer, 1); // queda lleno de '\0'

			fgets(entrada, bytes_a_leer + 2, stdin);

			// sacar el '\n' si quedó
			entrada[strcspn(entrada, "\n")] = '\0';

			// copiar solamente la cantidad solicitada
			memcpy(buffer_usuario, entrada,
			strlen(entrada) < bytes_a_leer ? strlen(entrada) : bytes_a_leer);

			free(entrada);

			enviar_op_code(IO_STDIN, fd_conexion);
			if(recibir_op_code(fd_conexion) != OK){
				log_info (logger, "Error al responder STDIN");
				return EXIT_FAILURE;
			}

			t_paquete* paquete_retorno = crear_paquete(IO_STDIN_RETORNO);
			agregar_a_paquete(paquete_retorno, &direccion_logica, sizeof(uint32_t));
			agregar_a_paquete(paquete_retorno, &bytes_a_leer, sizeof(uint32_t));
			agregar_a_paquete(paquete_retorno, buffer_usuario, bytes_a_leer);
			agregar_a_paquete(paquete_retorno, &pid, sizeof(u_int32_t));
			
			enviar_paquete(paquete_retorno, fd_conexion); 
			
			free(buffer_usuario);
			list_destroy_and_destroy_elements(lista, free);
			eliminar_paquete(paquete_retorno);
        	break;
    	}

    case gl_IO_STDOUT: {
        
		enviar_op_code(OK,fd_conexion);
		
		t_list* lista = recibir_paquete(fd_conexion);
        
        uint32_t pid = *(uint32_t*)list_get(lista, 0);
        uint32_t tam = *(uint32_t*)list_get(lista, 1);
        void* datos_imprimir = list_get(lista, 2);

        log_info(logger, "## PID: %u - Operación STDOUT.", pid);

		write(STDOUT_FILENO, datos_imprimir, tam);
		printf("\n");

		log_info(logger, "Contenido: %.*s", tam, (char*)datos_imprimir);

		enviar_op_code(IO_STDIN, fd_conexion);
		if(recibir_op_code(fd_conexion) != OK){
			log_info (logger, "Error al responder STDIN");
			return EXIT_FAILURE;
		}
		
        enviar_pid(pid,fd_conexion);
        
        list_destroy_and_destroy_elements(lista, free);
        break;
    }


		case -1:

			log_error(logger, "El cliente se desconectó");
			close(fd_conexion);
			return NULL;

		default:
			log_warning(logger,"IO desconocida.");
			
			break;
	}

	}
	
	
	terminar_programa(fd_conexion, logger, io_config);

	return 0;
}

/*
 ****************
 *	Funciones	*
 ****************
*/

t_config* iniciar_config(char* path)
{
	t_config* nuevo_config;

	nuevo_config = config_create(path);

    if (nuevo_config == NULL) {
        printf("¡No se pudo crear el config!\n");
        abort();
    }
	return nuevo_config;
}

t_log* iniciar_logger(char *log_level, char* file, char* process_name, char* is_active_console) //Ingreso valor del t_log_level para configurar la salida del mismo
{
	t_log* nuevo_logger;

	t_log_level level;
	
	bool active = (strcmp(is_active_console, "true") == 0);
	
	//Condicional que me permite modificar el nivel del log a partir de lo recibido en el .config//
	if (strcmp(log_level, "INFO") == 0)
	{
		level = LOG_LEVEL_INFO;
	}
	else
	{
		level = LOG_LEVEL_DEBUG;
	}
	
	nuevo_logger = log_create(file, process_name, active, level);
	
	return nuevo_logger;
}


void terminar_programa(int conexion, t_log* logger, t_config* config) //libera la memoria del logger y el config
{
	log_destroy(logger);
	config_destroy(config);
	liberar_conexion(conexion);
}

void validar_argumentos(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Error, faltan argumentos. \nUso: %s <arg1> <arg2>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}

