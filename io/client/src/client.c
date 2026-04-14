#include "client.h"
#include <commons/log.h>
#include <commons/config.h>
#include <readline/readline.h>
#include <string.h>


int main(void)
{
	int conexion;
	char* ip;
	char* puerto;
	char* valor;

	t_log_level log_level;

	char* ip_io, puerto_io, io;

	t_log* logger;
	t_config* config;


	/* ---------------- logger y config ---------------- */

	logger = iniciar_logger(log_level);

	config = iniciar_config();

	ip_io = config_get_string_value(config, "IP_IO");

	puerto_io = config_get_string_value(config, "PUERTO_IO");

	//log_level = config_get_string_value(config, "LOG_LEVEL"); //revisar funcionamiento de esto
	
    io = config_get_string_value(config, "IO"); //en duda con este
	
	log_info(logger, "Prueba de funcionamiento de logger");

	
// *** Conecxion a servidor *** //

    printf("Intentando conectar al servidor...\n");
	conexion = crear_conexion(ip_io, puerto_io);

	if (conexion != -1) {
		log_info(logger, "Conexion establecida, socket: %d\n", conexion);
        printf("Conexión establecida, socket: %d\n", conexion);
	} else {
		printf("No se pudo conectar.\n");
        log_info(logger, "No se puedo conectar.");
	}

	if(conexion < 0){ //se repiten las dos funciones pero es para ver como funciona cada una
		log_error(logger, "error de la conexión");
		terminar_programa(conexion, logger, config);
		return 1; //termino el programa
	}

// Enviamos un mensaje a KS para comprobar la conexion
	enviar_mensaje("Conecto IO con KS", conexion);

//funcion paquete que se debe revisar como funciona
	//paquete(conexion);

	terminar_programa(conexion, logger, config);

	return 0;
}



// *** Funciones *** //

t_log* iniciar_logger(t_log_level level) //Ingreso valor del t_log_level para configurar la salida del mismo
{
	t_log* nuevo_logger;

	nuevo_logger = log_create("KS.log", "CLIENTE", true, level);
	
	return nuevo_logger;
}

t_config* iniciar_config(void) 
{
	t_config* nuevo_config;

	nuevo_config = config_create("cliente.config");

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
}