#include "client.h"


int cliente_Kernel_Scheduler (int argc, char *argv[])
{
    char* archivo_config = argv[1];
    char* archivo_procesos = argv[2];

    config = iniciar_config(archivo_config);


	t_log_level log_level = log_level_from_string (config_get_string_value(config, "LOG_LEVEL"));
	info_km.ip_km = config_get_string_value(config, "IP_KM");
	info_km.puerto_km = config_get_string_value(config, "PUERTO_KM");
	info_config.planificacion_algoritmo = config_get_string_value(config, "PLANIFICATION_ALGORITHM");
	planificacion_algoritmo = info_config.planificacion_algoritmo; 
	info_config.listas_algortimo = config_get_string_value(config, "QUEUES_ALGORITHMS");
	info_config.intervalo_tarea = config_get_int_value(config, "RR_QUANTUM");
	
	char* valor_preemption = config_get_string_value(config, "QUEUE_PREEMPTION");
	info_config.preemption = strcmp(valor_preemption, "TRUE") == 0;

	info_config.tiempo_suspencion = config_get_int_value(config, "SUSPENSION_TIMEOUT");

	logger = iniciar_logger(log_level);
	log_debug(logger, "Logger iniciado Correctamente");

	iniciar_listas_suple();
    log_debug(logger, "Listas Suplementarias Iniciadas");
    
	Iniciar_listas_procesos();
    log_debug(logger,"Listas de Procesos Iniciadas");


	/* planificación */

	if(strcmp(info_config.planificacion_algoritmo, "CMN") == 0)
	{
		// copio el string porque lo voy a modificar
		char* colas_string = strdup(info_config.listas_algortimo);

		// elimino el '[' inicial
		if(colas_string[0] == '[')
		{
			memmove(colas_string, colas_string + 1, strlen(colas_string));
		}

		// elimino el ']' final
		int largo = strlen(colas_string);

		if(colas_string[largo - 1] == ']')
		{
			colas_string[largo - 1] = '\0';
		}

		char** algoritmos_array = string_split(colas_string, ",");

		for(int i = 0; algoritmos_array[i] != NULL; i++)
		{
			string_trim(&algoritmos_array[i]);
		}

		int cantidad_colas = 0;

		while(algoritmos_array[cantidad_colas] != NULL)
		{
			cantidad_colas++;
		}

		iniciar_planificador_CMN(
			algoritmos_array,
			cantidad_colas,
			info_config.intervalo_tarea
		);

		free(colas_string);
	}

	/*---------------------------------------------------CONEXION CON KM-------------------------------------------------------------*/


	// Creamos una conexión hacia el servidor
	log_info(logger,"Iniciando Conexion con Kernel Memory {Kernel_Scheduler ==> Kernel_Memory}");
	
	if(!mock)
	{
		
		info_km.conexion_km = crear_conexion(info_km.ip_km, info_km.puerto_km, logger, KERNEL_MEMORY);
		
		if (info_km.conexion_km != -1) 
		{
			log_info(logger,"## Conectado a Kernel Memory ==> Socket: [%d]", info_km.conexion_km); /*Logger Obligatorio*/
		}

		if(info_km.conexion_km < 0)
		{
			log_error(logger, "error de la conexión con Kernel Memory (Servidor)");
			terminar_programa(logger, config, info_km);
			return 1; 
		}
		
		enviar_op_code(OK, info_km.conexion_km);
	}	

	
	PCB* nuevo_pcb;

	// Enviamos al servidor el valor de CLAVE como mensaje
	if(mock)
	{	
		log_info(logger,"## Conectado a Kernel Memory ==> Socket: [MOCK]"); /*Logger Obligatorio*/
		log_debug(logger,"Iniciando Proceso Inicial");
		nuevo_pcb = crearNuevoProceso_mock(archivo_procesos, 1, info_km.conexion_km);
	}	

	else
	{
		log_debug(logger,"Iniciando Proceso Inicial");
		nuevo_pcb = crearNuevoProceso(archivo_procesos, 1, info_km.conexion_km);
	}	
	
	cambiar_estado_pcb(nuevo_pcb, RDY);
	agregar_proceso_lista(nuevo_pcb);
	eliminar_proceso_Lista(nuevo_pcb);
	
	return 0; //Fin de Main
	
}

/*----------------------------------FUNCIONES------------------------------------------*/

/* ---------------- FUNCIONES ADMINISTRATIVAS ---------------- */
t_log* iniciar_logger(t_log_level log_level)
{
	t_log* nuevo_logger;

	nuevo_logger = log_create("KS.log", "Kernel Scheduler", true, log_level);
	
	return nuevo_logger;
}








