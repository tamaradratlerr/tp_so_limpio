#include "client.h"

int contador_pid = 0;

int main(void)
{
	
	/* ---------------- ARCHIVOS DE CONFIGURACION Y LOGGER ---------------- */	
	logger = iniciar_logger();
	config = iniciar_config();

	info_km.ip_km = config_get_string_value(config, "IP_KM");
	info_km.puerto_km = config_get_string_value(config, "PUERTO_KM");
	info_config.planificacion_algoritmo = config_get_string_value(config, "PLANIFICATION_ALGORITHM");
	info_config.listas_algortimo = config_get_string_value(config, "QUEUES_ALGORITHMS");
	info_config.intervalo_tarea = config_get_int_value(config, "RR_QUANTUM");
	info_config.tiempo_suspencion = config_get_int_value(config, "SUSPENSION_TIMEOUT");

	/*---------------------------------------------------CONEXION CON KM-------------------------------------------------------------*/


	// Creamos una conexión hacia el servidor
	printf("Intentando conectar al servidor...\n");
	info_km.conexion_km = crear_conexion(info_km.ip_km, info_km.puerto_km, logger, KERNEL_MEMORY);

	if (info_km.conexion_km != -1) {
		printf("Conexión establecida, socket: %d\n", info_km.conexion_km);
	} else {
		printf("No se pudo conectar.\n");
	}
	if(info_km.conexion_km < 0){
		log_error(logger, "error de la conexión");
		terminar_programa(info_km.conexion_km, logger, config);
		return 1; //termino el programa
	}

	// Enviamos al servidor el valor de CLAVE como mensaje
	enviar_mensaje("me conecte KS con KM", info_km.conexion_km);

	// Armamos y enviamos el paquete
	paquete(info_km.conexion_km);


	terminar_programa(info_km.conexion_km, logger, config);

}

/*----------------------------------FUNCIONES------------------------------------------*/

/* ---------------- FUNCIONES ADMINISTRATIVAS ---------------- */
t_log* iniciar_logger(void)
{
	t_log* nuevo_logger;

	nuevo_logger = log_create("KS.log", "CLIENTE", true, "INFO");
	
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

void terminar_programa(int conexion, t_log* logger, t_config* config)
{
	log_destroy(logger);
	config_destroy(config);
	
}


/* ---------------- FUNCIONES GENERALES ---------------- */
PCB* crearNuevoProceso(t_log* logger, char* path, int fd_km) {
    
    
    PCB* nuevoPcb = iniciar_pcb(contador_pid, 0, 0);
	
    enviarProcesoKM(nuevoPcb, path, fd_km);

	contador_pid++;
	
	return nuevoPcb;
}

void enviarProcesoKM(PCB* pcb, char* path, int fd_km){
	
	t_paquete* paquete = crear_paquete(ENVIAR_PROCESO);
	
	agregar_a_paquete(paquete, pcb->data.PID, sizeof(int));

	agregar_a_paquete(paquete, &path, sizeof(char*));
	

	enviar_paquete(paquete, fd_km);
    eliminar_paquete(paquete);

}


