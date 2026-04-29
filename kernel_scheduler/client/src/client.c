#include "client.h"
#include <commons/log.h>
#include <commons/config.h>
#include <readline/readline.h>
#include <string.h>
#include "../utilsKS/utilsKS.h"

t_log_level log_level;

// declaracion variables globales
int contador_pid = 0;


int main(void)
{
	
	int conexion;
	char* ip;
	char* puerto;
	char* valor;
	char *ip_km, *puerto_km, *planificacion_algoritmo, *listas_algortimo;	int intervalo_tarea, tiempo_suspencion;	int intervalo_tarea, tiempo_suspencion;
	t_log* logger;
	t_config* config;

	
	logger = iniciar_logger();




	/* ---------------- ARCHIVOS DE CONFIGURACION ---------------- */

	config = iniciar_config();

	
	ip_km = config_get_string_value(config, "IP_KM");

	puerto_km = config_get_string_value(config, "PUERTO_KM");

	planificacion_algoritmo = config_get_string_value(config, "PLANIFICATION_ALGORITHM");
	listas_algortimo = config_get_string_value(config, "QUEUES_ALGORITHMS");
	intervalo_tarea = config_get_int_value(config, "RR_QUANTUM");
	tiempo_suspencion = config_get_int_value(config, "SUSPENSION_TIMEOUT");
	
	
	log_info(logger, "Soy un Log");
	/* ---------------- LEER DE CONSOLA ---------------- */

	leer_consola(logger);

	/*---------------------------------------------------CONEXION CON KM-------------------------------------------------------------*/


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



//-------CHECKPOINT 2---------


void crearNuevoProceso(t_log* logger, char* path, int fd_km) {
    
    
    //  Leemos instrucciones de UN SOLO PROCESO
    t_list* nuevoProceso = leerInstruccionesDeUnProceso(logger, path); 

	/*  creo que path lleva a un archivo txt que lee instrucción (MOV AX BX) por instruccion
	    leemos hasta que aparezca algo que nos indique que termino el proceso
	    y guardamos esas instrucciones en una lista de instrucciones a la que llamo nuevoProceso  */


	if (nuevoProceso == NULL) {
        log_error(logger, "No se pudo leer el archivo en el path: %s", path);
        return NULL;
    }

	PCB* nuevoPcb = iniciar_pcb(contador_pid++, 0, 0);
	
	
    
    /*  Le mandamos el PID y el proceso a la KM para que lo guarde
	    pero como nuevo proceso es un PUNTERO de una lista habria que serializar
	    para poder mandarlo bien  */

    enviarProcesoKM(nuevoPcb->PID, nuevoProceso, fd_km);

}

t_list* leerInstruccionesDeUnProceso(t_log* logger, char* path) {

    FILE* archivo = fopen(path, "r");
    
    if (archivo == NULL) {
        log_error(logger, "No se pudo abrir el archivo en el path: %s", path);
        return NULL;
    }

    t_list* lista_instrucciones = list_create();
    char* linea = NULL;  
    size_t tamañoLinea = 0; 
    ssize_t caracteresLeidos;

    while ((caracteresLeidos = getline(&linea, &tamañoLinea, archivo)) != -1) {

        if (caracteresLeidos > 0 && linea[caracteresLeidos - 1] == '\n') {
            linea[caracteresLeidos - 1] = '\0';
        }

        list_add(lista_instrucciones, strdup(linea));
    }
    
	//libero memoria y cierro archivo
    free(linea); 
    fclose(archivo);
        
    return lista_instrucciones;
}

void enviarProcesoKM(int pid, t_list* nuevoProceso, int fd_km){
	
	t_paquete* paquete = crear_paquete();
	
	agregar_a_paquete(paquete, &pid, sizeof(int));

	int cant_instrucciones = list_size(nuevoProceso);

	for (int i = 0; i < cant_instrucciones; i++) {
        
		// sacamos la instru d la lista de instrucciones que es el nuevo proceso
        char* instruccion = list_get(nuevoProceso, i);
        
        // calculamos su tamaño (con el \0)
        int tamanio = strlen(instruccion) + 1;

        // la metemos en el paquete
        agregar_a_paquete(paquete, instruccion, tamanio);
    }

	enviar_paquete(paquete, fd_km);
    eliminar_paquete(paquete);
	list_destroy_and_destroy_elements(nuevoProceso, (void*)free);

}