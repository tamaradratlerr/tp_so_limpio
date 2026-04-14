#include "client.h"
#include <commons/log.h>
#include <commons/config.h>
#include <readline/readline.h>
#include <string.h>


int main(void)
{
	/*---------------------------------------------------PARTE 2-------------------------------------------------------------*/

	int conexion;
	char* ip;
	char* puerto;
	char* valor;
	t_log_level log_level;
	char* ip_km,	puerto_km,	planificacion_algoritmo,	listas_algortimo;
	int intervalo_tarea, tiempo_suspencion;

	t_log* logger;
	t_config* config;

	/* ---------------- LOGGING ---------------- */

	logger = iniciar_logger();


	// Usando el logger creado previamente
	// Escribi: "Hola! Soy un log"


	/* ---------------- ARCHIVOS DE CONFIGURACION ---------------- */

	config = iniciar_config();

	// Usando el config creado previamente, leemos los valores del config y los 
	// dejamos en las variables 'ip', 'puerto' y 'valor'

	ip_km = config_get_string_value(config, "IP_KM");

	puerto_km = config_get_string_value(config, "PUERTO_KM");

	log_level = config_get_string_value(config, "LOG_LEVEL");
	planificacion_algoritmo = config_get_string_value(config, "PLANIFICATION_ALGORITHM");
	listas_algortimo = config_get_string_value(config, "QUEUES_ALGORITHMS");
	intervalo_tarea = config_get_int_value(config, "RR_QUANTUM");
	tiempo_suspencion = config_get_int_value(config, "SUSPENSION_TIMEOUT");
	
	
	log_info(logger, "Soy un Log");
	/* ---------------- LEER DE CONSOLA ---------------- */

	leer_consola(logger);

	/*---------------------------------------------------PARTE 3-------------------------------------------------------------*/

	// ADVERTENCIA: Antes de continuar, tenemos que asegurarnos que el servidor esté corriendo para poder conectarnos a él

	// Creamos una conexión hacia el servidor
	printf("Intentando conectar al servidor...\n");
	conexion = crear_conexion(ip_km, puerto_km);

	if (conexion != -1) {
		printf("Conexión establecida, socket: %d\n", conexion);
	} else {
		printf("No se pudo conectar.\n");
	}
	if(conexion < 0){
		log_error(logger, "error de la conexión");
		terminar_programa(conexion, logger, config);
		return 1; //termino el programa
	}

	// Enviamos al servidor el valor de CLAVE como mensaje
	enviar_mensaje("me conecte KS con KM", conexion);

	// Armamos y enviamos el paquete
	paquete(conexion);

	terminar_programa(conexion, logger, config);

	/*---------------------------------------------------PARTE 5-------------------------------------------------------------*/
	// Proximamente
}

t_log* iniciar_logger(void)
{
	t_log* nuevo_logger;

	nuevo_logger = log_create("KS.log", "CLIENTE", true, log_level);
	
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

void leer_consola(t_log* logger)
{
	char* leido;

	// La primera te la dejo de yapa
	leido = readline("> ");

	// El resto, las vamos leyendo y logueando hasta recibir un string vacío
	while(strcmp(leido, "") != 0){
		
		log_info(logger, "Mensaje: %s", leido);
		free(leido);
		leido = readline("> ");
	
	}

	// ¡No te olvides de liberar las lineas antes de regresar!
	
}

void paquete(int conexion)
{
	// Ahora toca lo divertido!
	char* leido;
	t_paquete* paquete = crear_paquete();

	// Leemos y esta vez agregamos las lineas al paquete
	leido = readline("> ");
	
	while(strcmp(leido, "") != 0){
		agregar_a_paquete(paquete, leido, strlen(leido) + 1);

		//libero espacio
		free(leido);
		leido = readline("> ");
	}
	
	free(leido);

	enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
	// ¡No te olvides de liberar las líneas y el paquete antes de regresar!
	
}

void terminar_programa(int conexion, t_log* logger, t_config* config)
{
	log_destroy(logger);
	config_destroy(config);
	/* Y por ultimo, hay que liberar lo que utilizamos (conexion, log y config) 
	  con las funciones de las commons y del TP mencionadas en el enunciado */
}
