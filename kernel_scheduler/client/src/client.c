#include "client.h"

int contador_pid = 0;


int main(void)
{
	
	/* ---------------- ARCHIVOS DE CONFIGURACION Y LOGGER ---------------- */	
	logger = iniciar_logger();
	config = iniciar_config();

	ip_km = config_get_string_value(config, "IP_KM");
	puerto_km = config_get_string_value(config, "PUERTO_KM");
	planificacion_algoritmo = config_get_string_value(config, "PLANIFICATION_ALGORITHM");
	listas_algortimo = config_get_string_value(config, "QUEUES_ALGORITHMS");
	intervalo_tarea = config_get_int_value(config, "RR_QUANTUM");
	tiempo_suspencion = config_get_int_value(config, "SUSPENSION_TIMEOUT");

	/*---------------------------------------------------CONEXION CON KM-------------------------------------------------------------*/


	// Creamos una conexión hacia el servidor
	printf("Intentando conectar al servidor...\n");
	conexion.km = crear_conexion(ip_km, puerto_km);

	if (conexion.km != -1) {
		printf("Conexión establecida, socket: %d\n", conexion.km);
	} else {
		printf("No se pudo conectar.\n");
	}
	if(conexion.km < 0){
		log_error(logger, "error de la conexión");
		terminar_programa(conexion.km, logger, config);
		return 1; //termino el programa
	}

	// Enviamos al servidor el valor de CLAVE como mensaje
	enviar_mensaje("me conecte KS con KM", conexion.km);

	// Armamos y enviamos el paquete
	paquete(conexion.km);


	terminar_programa(conexion.km, logger, config);

}

/*----------------------------------FUNCIONES------------------------------------------*/

/* ---------------- FUNCIONES ADMINISTRATIVAS ---------------- */
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

void terminar_programa(int conexion, t_log* logger, t_config* config)
{
	log_destroy(logger);
	config_destroy(config);
	
}


/* ---------------- FUNCIONES GENERALES ---------------- */
void crearNuevoProceso(t_log* logger, char* path, int fd_km) {
    
    
    PCB* nuevoPcb = iniciar_pcb(contador_pid, 0, 0);
	
    enviarProcesoKM(nuevoPcb, path, fd_km);

	contador_pid++;
}

void enviarProcesoKM(PCB* pcb, char* path, int fd_km){
	
	t_paquete* paquete = crear_paquete();
	
	agregar_a_paquete(paquete, pcb->data.PID, sizeof(int));

	agregar_a_paquete(paquete, pcb->data.PPID, sizeof(int));

	agregar_a_paquete(paquete, pcb->data.UID, sizeof(int));

	agregar_a_paquete(paquete, &path, sizeof(char*));
	

	enviar_paquete(paquete, fd_km);
    eliminar_paquete(paquete);

}


