#include "client.h"
#include <utils/utils.h>

int main(void)
{
	int conexion;
	t_log* logger;
	t_config* config;

	// LOGGER
	logger = iniciar_logger();

	// CONFIG
	config = iniciar_config(); // Lee "cliente.config" como ya tenías

	// LEER VALORES ESPECÍFICOS DE SWAP
	
	char* path_swap = config_get_string_value(config, "SWAP_FILE_PATH");
	int tamano_swap = config_get_int_value(config, "SWAP_FILE_SIZE");
	int tamano_bloque = config_get_int_value(config, "BLOCK_SIZE");
	char* ip_km = config_get_string_value(config, "IP_KM");
	char* puerto_km = config_get_string_value(config, "PUERTO_KM");

	// INICIALIZAR ARCHIVO SWAP (Consigna)
	
	inicializar_swap(path_swap, tamano_swap, tamano_bloque);

	// CONEXIÓN
	conexion = crear_conexion(ip_km, puerto_km);

	if(conexion < 0){
		log_error(logger, "Error en la conexión con Kernel Memory");
		terminar_programa(conexion, logger, config);
		return 1;
	}

	log_info(logger, "## Conectado a Kernel Memory");
	
	// Informar tamaño de bloque y tamaño total
  
	t_paquete* p = crear_paquete();
	p->codigo_operacion = HANDSHAKE_SWAP; // Usamos el enum de utils.h

	// Agregamos los datos que pide el enunciado
  
	agregar_a_paquete(p, &tamanio_bloque, sizeof(int));
	agregar_a_paquete(p, &tamanio_swap, sizeof(int));

	enviar_paquete(p, conexion);
	eliminar_paquete(p);

	atender_kernel(conexion);

	terminar_programa(conexion, logger, config);
	return 0;
}



t_log* iniciar_logger(void)
{
	
	return log_create("swap.log", "SWAP", true, LOG_LEVEL_INFO);
}

t_config* iniciar_config(void)
{
	t_config* nuevo_config = config_create("cliente.config");
	if (nuevo_config == NULL) {
		perror("No se pudo crear el config");
		exit(1);
	}
	return nuevo_config;
}

void terminar_programa(int conexion, t_log* logger, t_config* config)
{
	log_destroy(logger);
	config_destroy(config);
	liberar_conexion(conexion);
}
