#include "cpu.h"
#include <commons/log.h>
#include <commons/config.h>
#include <readline/readline.h>
#include <string.h>

int main(void)
{
    /* ---------------- LOGGING ---------------- */
    t_log* logger = iniciar_logger();
    log_info(logger, "Modulo CPU iniciado");

    /* ---------------- ARCHIVOS DE CONFIGURACION ---------------- */
    t_config* config = iniciar_config();

    /* ---------------- LEER CONSOLA ---------------- */
    
    leer_consola(logger);

    /* ---------------- CONEXIONES ---------------- */
    t_cpu_sockets sockets;

    // Conectamos a cada modulo segun las claves de tu config
    printf("intentando conectar sockets");
    sockets.conexion_kernel_memory    = conexion_kernel_memory(config);
    sockets.conexion_kernel_scheduler = conexion_kernelS(config);
    sockets.conexion_kernelS_dispatch  = conexion_kernelS_dispatch(config);
    sockets.conexion_kernelS_interrupt = conexion_kernelS_interrupt(config);
    sockets.conexion_memory_stick     = conexion_memory_stick(config);

    // Validacion de conexiones 
    if (sockets.conexion_kernel_memory < 0) {
        log_error(logger, "Error al conectar con KERNEL MEMORY");
        terminar_programa(logger, config, sockets);
        return 1;
    }
    if (sockets.conexion_kernel_scheduler < 0) {
        log_error(logger, "Error al conectar con KERNEL SCHEDULER (Base)");
        terminar_programa(logger, config, sockets);
        return 1;
    }
    if (sockets.conexion_kernelS_dispatch < 0) {
        log_error(logger, "Error al conectar con KERNEL SCHEDULER (Dispatch)");
        terminar_programa(logger, config, sockets);
        return 1;
    }
    if (sockets.conexion_kernelS_interrupt < 0) {
        log_error(logger, "Error al conectar con KERNEL SCHEDULER (Interrupt)");
        terminar_programa(logger, config, sockets);
        return 1;
    }
    if (sockets.conexion_memory_stick < 0) {
        log_error(logger, "Error al conectar con MEMORY STICK");
        terminar_programa(logger, config, sockets);
        return 1;
    }

    log_info(logger, "Todas las conexiones (5/5) establecidas con éxito");

    printf("Seleccione destino (1: MS, 2: K_DISPATCH, 3: K_MEMORY): ");
    char* seleccion = readline("> ");

    if (strcmp(seleccion, "1") == 0) {
        enviar_mensaje("Mensaje a MS", sockets.conexion_memory_stick);
        paquete(sockets.conexion_kernel_memory);
    } 
    else if (strcmp(seleccion, "2") == 0) {
        enviar_mensaje("Mensaje a KS", sockets.conexion_kernel_scheduler);
        paquete(sockets.conexion_kernel_scheduler);
    
    }
    else if (strcmp(seleccion, "3") == 0) {
        enviar_mensaje("Mensaje a KM", sockets.conexion_kernel_memory);
        paquete(sockets.conexion_memory_stick);

    }
    
    free(seleccion);

    

    terminar_programa(logger, config, sockets);
    return 0;
}


t_log* iniciar_logger(void)
{
    t_log* nuevo_logger = log_create("cpu.log", "CPU", true, LOG_LEVEL_INFO);
    if (nuevo_logger == NULL) {
        perror("No se pudo crear el logger");
        exit(EXIT_FAILURE);
    }
    return nuevo_logger;
}

t_config* iniciar_config(void)
{
    t_config* nuevo_config = config_create("cpu.config");
    if (nuevo_config == NULL) {
        printf("¡No se pudo crear el config!\n");
        abort();
    }
    return nuevo_config;
}

void leer_consola(t_log* logger)
{
    char* leido = readline("> ");
    while(strcmp(leido, "") != 0){
        log_info(logger, "Mensaje: %s", leido);
        free(leido);
        leido = readline("> ");
    }
    free(leido);
}

void terminar_programa(t_log* logger, t_config* config, t_cpu_sockets sockets)
{
    // Liberar los logs y config
    if(logger) log_destroy(logger);
    if(config) config_destroy(config);
    
    // Liberar las conexiones (esto reemplaza al cerrar una sola conexion del TP0)
    liberar_conexion(sockets.conexion_kernel_memory);
    liberar_conexion(sockets.conexion_kernel_scheduler);
    liberar_conexion(sockets.conexion_kernelS_dispatch);
    liberar_conexion(sockets.conexion_kernelS_interrupt);
    liberar_conexion(sockets.conexion_memory_stick);
}

/* ---------------- IMPLEMENTACION DE CONEXIONES ---------------- */

int conexion_kernel_memory(t_config* config) {
    char* ip = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");
    return crear_conexion(ip, puerto);
}

int conexion_kernelS(t_config* config) {
    char* ip = config_get_string_value(config, "IP_KERNEL_SCHEDULER");
    char* puerto = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");
    return crear_conexion(ip, puerto);
}

int conexion_kernelS_dispatch(t_config* config) {
    char* ip = config_get_string_value(config, "IP_KERNEL_SCHEDULER");
    char* puerto = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER_DISPATCH");
    return crear_conexion(ip, puerto);
}

int conexion_kernelS_interrupt(t_config* config) {
    char* ip = config_get_string_value(config, "IP_KERNEL_SCHEDULER");
    char* puerto = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER_INTERRUPT");
    return crear_conexion(ip, puerto);
}

int conexion_memory_stick(t_config* config) {
    char* ip = config_get_string_value(config, "IP_MEMORY_STICK");
    char* puerto = config_get_string_value(config, "PUERTO_MEMORY_STICK");
    return crear_conexion(ip, puerto);
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