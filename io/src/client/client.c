#include "client.h"

int main(int argc, char** argv)
{
	validar_argumentos(argc, argv);

	int conexion;
	char* clave;
	char* io_ip; 
	char* io_port; 
	char* log_level;
	char* io;
	t_log* logger;
	
	/* Inicio configuración */
	t_config* config = config_create(argv[1]);
	/* Chequeamos si hubo error al crear config */
    if (config == NULL) {
        printf("¡No se pudo crear el config!\n");
        abort();
    }




	/* Revisar desde la VM la commmons la funcion "iniciar_config" */




	/* Asigno el tipo de IO a la variable "io" */
	io = argv[2];
	printf("Tipo de IO ingresada: %s", io);


	/* ---------------- logger y config ---------------- */
	config = iniciar_config();
	if (config == NULL) {
		fprintf(stderr, "No se pudo abrir el archivo de configuración\n");
		return EXIT_FAILURE;
	}

	clave = config_get_string_value(config, "CLAVE");
	io_ip = config_get_string_value(config, "IO_IP");
	io_port = config_get_string_value(config, "IO_PORT");
	log_level = config_get_string_value(config, "LOG_LEVEL");

	
	logger = iniciar_logger(log_level);
	
	log_info(logger, "Prueba de funcionamiento de logger");
	log_info(logger, io_ip);
	log_info(logger, io_port);


	
// *** Conexion a servidor *** //

    printf("Intentando conectar al servidor...\n");
	log_info(logger, "Intentando concetar al servidor...\n");

	conexion = crear_conexion(io_ip, io_port);

	if (conexion != -1) {
		log_info(logger, "Conexion establecida, socket: %d\n", conexion);
        printf("Conexión establecida, socket: %d\n", conexion);
		log_info(logger, "## Conectado a Kernel Scheduler"); //*** --- Logger obligatorio para Entrega --- ***//
	} else {
		printf("No se pudo conectar.\n");
        log_info(logger, "No se puedo conectar.");
		return 1;
	}

	// if(conexion < 0){ //se repiten las dos funciones pero es para ver como funciona cada una
	// 	log_error(logger, "error de la conexión");
	// 	terminar_programa(conexion, logger, config);
	// 	return 1; //termino el programa
	//}

// Enviamos un mensaje a KS para comprobar la conexion
	enviar_mensaje(clave, conexion);

	log_info(logger, "Esperando porr IO desde K.S.");

//funcion paquete que se debe revisar como funciona
	//paquete(conexion);

	terminar_programa(conexion, logger, config);

	return 0;
}



// *** Funciones *** //
//*** Uso la funcion de las commons *** //
t_log* iniciar_logger(char *log_level) //Ingreso valor del t_log_level para configurar la salida del mismo
{
	t_log* nuevo_logger;

	char *file = "io.log";
	char *process_name = "io_logs";
	bool is_active_console = true;
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
