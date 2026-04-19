#include "client.h"

//argc es la cantidad de argumentos que se agregan por línea de comando (argument count).
//argv es un array de strings que contiene los string ingresados (argument vector).

//argumento 0: intruccion - agumento 1: direc. archivo.config - argumento 2: Tipo de intruccion de IO.
// void validar_argumentos(int argc, char** argv) {
//     if (argc != 3) { 
//         fprintf(stderr, "Error, faltan argumentos. \nUso: %s <arg1> <arg2>\n", argv[0]);
//         exit(EXIT_FAILURE);
//     }
// }

//int main(int argc, char** argv)
int main(void)
{
	//validar_argumentos(argc, argv);
// *** ---------- Variables ---------- *** //

	int conexion;

	char* clave;
	char* ip_io; 
	char* puerto_io; 
	char* log_level; //Variable para darle el valor de LOG_LEVEL desde el config
	
	char* io;

	

	t_log* logger;
	t_config* config;

	

	//io = argv[2]; //Le damos valor al tipo de intruccion.
	// io = "STDIN";
	// printf("Valor de Tipo = ", io);


	/* ---------------- logger y config ---------------- */


	//config = config_create(argv[1]);
	config = iniciar_config();
	if (config == NULL) {
    fprintf(stderr, "No se pudo abrir el archivo de configuración\n");
    return EXIT_FAILURE;
	}

	clave = config_get_string_value(config, "CLAVE");
	ip_io = config_get_string_value(config, "IP_IO");
	puerto_io = config_get_string_value(config, "PUERTO_IO");
	log_level = config_get_string_value(config, "LOG_LEVEL"); 

	
	logger = iniciar_logger(log_level);
	
	log_info(logger, "Prueba de funcionamiento de logger");
	log_info(logger, ip_io);
	log_info(logger, puerto_io);


	
// *** Conexion a servidor *** //

    printf("Intentando conectar al servidor...\n");
	log_info(logger, "Intentando concetar al servidor...\n");

	conexion = crear_conexion(ip_io, puerto_io);

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


//*** Uso la funcion de las commons *** //
t_config* iniciar_config(void) 
{
	t_config* nuevo_config;

	char *path = "IO.config";

	nuevo_config = config_create(path);

	//Chequeamos si hubo error al crear el config
    if (nuevo_config == NULL) {
        printf("¡No se pudo crear el config!\n");
        abort(); // Terminamos la ejecución como pide la consigna
    }

	return nuevo_config;
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