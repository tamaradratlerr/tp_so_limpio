#include "client.h"

int main(void)
{
	/*---------------------------------------------------PARTE 2-------------------------------------------------------------*/

	int conexion;
	char* ip;
	char* puerto;
	char* valor;
	char* clave;

	t_log* logger;
	t_config* config;

	/* ---------------- LOGGING ---------------- */
	logger = iniciar_logger();
	// Usando el logger creado previamente
	// Escribi: "Hola! Soy un log"
	log_info(logger, "Hola! Soy un log");


	/* ---------------- ARCHIVOS DE CONFIGURACION ---------------- */
	config = iniciar_config();
	if (config == NULL) {
		abort();
	}

	// Usando el config creado previamente, leemos los valores del config y los 
	// dejamos en las variables 'ip', 'puerto' y 'valor'
	ip = config_get_string_value(config, "IP");
	puerto = config_get_string_value(config, "PUERTO");
	valor = config_get_string_value(config, "VALOR");
	clave = config_get_string_value(config, "CLAVE");
	// Loggeamos el valor de config
	log_info(logger, ip);
	log_info(logger, puerto);
	log_info(logger, valor);

	/* ---------------- LEER DE CONSOLA ---------------- */
	leer_consola(logger);

	/*---------------------------------------------------PARTE 3-------------------------------------------------------------*/

	// ADVERTENCIA: Antes de continuar, tenemos que asegurarnos que el servidor esté corriendo para poder conectarnos a él

	// Creamos una conexión hacia el servidor
	conexion = crear_conexion(ip, puerto);

	// Enviamos al servidor el valor de CLAVE como mensaje
	enviar_mensaje(clave, conexion);
	// Armamos y enviamos el paquete
	paquete(conexion);

	terminar_programa(conexion, logger, config);

	/*---------------------------------------------------PARTE 5-------------------------------------------------------------*/
	// Proximamente
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
