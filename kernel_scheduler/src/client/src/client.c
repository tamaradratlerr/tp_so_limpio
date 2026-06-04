#include "client.h"

int inicializar_kernel(void)
{
	
	/* ---------------- ARCHIVOS DE CONFIGURACION Y LOGGER ---------------- */	
	
	config = iniciar_config();

	t_log_level log_level = t_log_level_from_string (config_get_string_value(config, "LOG_LEVEL"));
	info_km.ip_km = config_get_string_value(config, "IP_KM");
	info_km.puerto_km = config_get_string_value(config, "PUERTO_KM");
	info_config.planificacion_algoritmo = config_get_string_value(config, "PLANIFICATION_ALGORITHM");
	info_config.listas_algortimo = config_get_string_value(config, "QUEUES_ALGORITHMS");
	info_config.intervalo_tarea = config_get_int_value(config, "RR_QUANTUM");
	info_config.tiempo_suspencion = config_get_int_value(config, "SUSPENSION_TIMEOUT");

	logger = iniciar_logger(log_level);
	/*---------------------------------------------------CONEXION CON KM-------------------------------------------------------------*/


	// Creamos una conexión hacia el servidor
	printf("Intentando conectar al Kernel Memory (Servidor)\n");
	info_km.conexion_km = crear_conexion(info_km.ip_km, info_km.puerto_km, logger, KERNEL_MEMORY);

	if (info_km.conexion_km != -1) {
		printf("## Conectado a Kernel Memory,socket: %d\n", info_km.conexion_km); /*Logger Obligatorio*/
	}
	if(info_km.conexion_km < 0){
		log_error(logger, "error de la conexión con Kernel Memory (Servidor)");
		terminar_programa(info_km.conexion_km, logger, config);
		return 1; //termino el programa
	}

	// Enviamos al servidor el valor de CLAVE como mensaje
	enviar_op_code(OK, info_km.conexion_km);

	inicio_todo = true;
	//terminar_programa(info_km.conexion_km, logger, config);

}

/*----------------------------------FUNCIONES------------------------------------------*/

/* ---------------- FUNCIONES ADMINISTRATIVAS ---------------- */
t_log* iniciar_logger(t_log_level log_level)
{
	t_log* nuevo_logger;

	nuevo_logger = log_create("KS.log", "Kernel Scheduler", true, log_level);
	
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







