#include "client.h"

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
	t_log* logger;

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
	fd_conexion = crear_conexion(io_ip, io_port);



	/* Subo hasta aca, porque voy a poner codigo en utils (develop)*/



	if (fd_conexion != -1) {
        printf("Conexión establecida, socket: %d\n", fd_conexion);
		log_info(logger, "## Conectado a Kernel Scheduler"); //*** --- Logger obligatorio para Entrega --- ***//
	} else {
		printf("No se pudo conectar.\n");
        log_info(logger, "No se puedo conectar.");
		return 1;
	}

	// if(fd_conexion < 0){ //se repiten las dos funciones pero es para ver como funciona cada una
	// 	log_error(logger, "error de la conexión");
	// 	terminar_programa(fd_conexion, logger, io_config);
	// 	return 1; //termino el programa
	//}

// Enviamos un mensaje a KS para comprobar la conexion
	enviar_mensaje(clave, fd_conexion);

	log_info(logger, "Esperando porr IO desde K.S.");

//funcion paquete que se debe revisar como funciona
	//paquete(fd_conexion);

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

	char *file = file;
	char *process_name = process_name;
	bool is_active_console = is_active_console;
	t_log_level level;

	//Condicional que me permite modificar el nivel del log a partir de lo recibido en el .config//
	if (strcmp(log_level, "INFO") == 0)
	{
		level = LOG_LEVEL_INFO;
	}
	else
	{
		level = LOG_LEVEL_DEBUG;
	}
	
	nuevo_logger = log_create(file, process_name, is_active_console, level);
	
	return nuevo_logger;
}


// **original del TP0, no me parece necesaria**

// void leer_consola(t_log* logger)
// {
// 	char* leido;

// 	// La primera te la dejo de yapa
// 	leido = readline("> ");

// 	// El resto, las vamos leyendo y logueando hasta recibir un string vacío
// 	while(strcmp(leido, "") != 0){
		
// 		log_info(logger, "Mensaje: %s", leido);
// 		free(leido);
// 		leido = readline("> ");
	
// 	}
	
// }

// **Original del TP0 me parece que no sirce pero se puede reutilizar** //
// void paquete(int conexion)
// {
// 	char* leido;
// 	t_paquete* paquete = crear_paquete();

// 	//¿¿¿Completar para cargar los paquetes???
	
// 	free(leido);

// 	enviar_paquete(paquete, conexion);
// 	eliminar_paquete(paquete);
	
	
// }


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
