#include "client.h"
t_log* logger;
int main(int argc, char* argv[]) //./bin/swap [Archivo Config]
//significa que el programa tiene que recibir el path del config por argumento argv[1]
{
	int conexion;
	t_config* config;

// confirmacion
	if(argc != 2) {
		printf("Uso: %s [Archivo Config]\n", argv[0]);
		return 1;
	}

	// 1. LOGGER
	logger = iniciar_logger();

	// 2. CONFIG
	// 2. CONFIG
    config = iniciar_config(argv[1]); // Lee el config recibido por argumento // Lee "client.config" 

	// 3. LEER VALORES ESPECÍFICOS DE SWAP
	// Mantenemos tipos de datos simples (int y char*)
	char* path_swap = config_get_string_value(config, "SWAP_FILE_PATH");
	int tamano_swap = config_get_int_value(config, "SWAP_FILE_SIZE");
	int tamano_bloque = config_get_int_value(config, "BLOCK_SIZE");
	char* ip_km = config_get_string_value(config, "IP_KM");
	char* puerto_km = config_get_string_value(config, "PUERTO_KM");

//evitar divisiones invalidas al momento de calcular la cantidad de bloques y que asi queden de tamaño fijo
if(tamano_swap <= 0 || tamano_bloque <= 0) {
	log_error(logger, "El tamaño de SWAP y el tamaño de bloque tienen que ser mayores a 0");
	terminar_programa(-1, logger, config);
	return 1;
}

if(tamano_swap % tamano_bloque != 0) {
	log_error(logger, "El tamaño de SWAP tiene que ser múltiplo del tamaño de bloque");
	terminar_programa(-1, logger, config);
	return 1;
}

	// 4. INICIALIZAR ARCHIVO SWAP (Consigna)
	// Esta función debe crear el archivo con ftruncate
	inicializar_swap(path_swap, tamano_swap, tamano_bloque);

	// 5. CONEXIÓN
	conexion = crear_conexion(ip_km, puerto_km , logger, SWAP);

	if(conexion < 0){
		log_error(logger, "Error en la conexión con Kernel Memory");
		terminar_programa(conexion, logger, config);
		return 1;
	}

	log_info(logger, "## Conectado a Kernel Memory");

	
	
	// CUMPLIR CONSIGNAS: Informar tamaño de bloque y tamaño total
	t_paquete* p = crear_paquete(HANDSHAKE_SWAP);

	// Agregamos los datos que pide el enunciado usando ints normales
	agregar_a_paquete(p, &tamano_bloque, sizeof(int));
	agregar_a_paquete(p, &tamano_swap, sizeof(int));

	enviar_paquete(p, conexion);
	eliminar_paquete(p);

	// 6. ATENDER (Bucle para que el módulo no muera)
	atender_kernel(conexion, logger);

	terminar_programa(conexion, logger, config);
	return 0;
}

// funciones de soporte

t_log* iniciar_logger(void)
{
	// Cambié el nombre del archivo de log para que sea representativo
	return log_create("swap.log", "SWAP", true, LOG_LEVEL_INFO);
}


void terminar_programa(int conexion, t_log* logger, t_config* config)
{
	log_destroy(logger);
	config_destroy(config);
	liberar_conexion(conexion);
}
