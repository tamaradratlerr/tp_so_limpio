#include "client.h"

int memory_stick_client(void)
{
	int conexion;
	char* ip;
	char* puerto;
	char* clave;

	t_log* logger;
	t_config* config;

	/* Inicio el logging */
	logger = iniciar_logger();
	log_info(logger, "Log cliente iniciado.");

	/* Cargo configuración */
	config = iniciar_config();
	if (config == NULL) {
		abort();
	}
	ip = config_get_string_value(config, "IP");
	puerto = config_get_string_value(config, "PUERTO");
	clave = config_get_string_value(config, "CLAVE");
	// Loggeamos el valor de config
	log_info(logger, ip);
	log_info(logger, puerto);

	/* ---------------- LEER DE CONSOLA ---------------- */
	leer_consola(logger);

	// Creamos una conexión hacia el servidor
	conexion = crear_conexion(ip, puerto);

	// Enviamos al servidor el valor de CLAVE como mensaje
	enviar_mensaje(clave, conexion);
	// Armamos y enviamos el paquete
	paquete(conexion);

	terminar_programa(conexion, logger, config);

}

t_log* iniciar_logger(void)
{
	t_log* nuevo_logger;

	char *file = "tp0.log";
	char *process_name = "tp0_logs";
	bool is_active_console = true;
	t_log_level level = LOG_LEVEL_INFO;
	
	nuevo_logger = log_create(file, process_name, is_active_console, level);
	return nuevo_logger;
}

t_config* iniciar_config(void)
{
	t_config* nuevo_config;

	char *path = "cliente.config";

	nuevo_config = config_create(path);
	return nuevo_config;
}

void leer_consola(t_log* logger)
{
	char* leido;
	while (1)
	{
		// La primera te la dejo de yapa
		leido = readline("> ");

		if (!strcmp(leido, "")){
			/* Es linea vacía. */
			break;
		}
		// El resto, las vamos leyendo y logueando hasta recibir un string vacío
		if(leido){
			add_history(leido);
		}
		log_info(logger, leido);
		// ¡No te olvides de liberar las lineas antes de regresar!
	}
	free(leido);
}

void paquete(int conexion)
{
	// Ahora toca lo divertido!
	char* leido;
	t_paquete* paquete = crear_paquete();

	// Leemos y esta vez agregamos las lineas al paquete
	while (1)
	{
		leido = readline("> ");

		if (!strcmp(leido, "")){
			break;
		}

		if(leido){
			agregar_a_paquete(paquete, leido, strlen(leido) + 1);
		}
	}
	free(leido);
	enviar_paquete(paquete, conexion);

	// ¡No te olvides de liberar las líneas y el paquete antes de regresar!
	eliminar_paquete(paquete);
	
}

void terminar_programa(int conexion, t_log* logger, t_config* config)
{
	/* Y por ultimo, hay que liberar lo que utilizamos (conexion, log y config) 
	  con las funciones de las commons y del TP mencionadas en el enunciado */
	  log_destroy(logger);
	  config_destroy(config);
	  liberar_conexion(conexion);
}
