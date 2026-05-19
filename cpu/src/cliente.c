#include "cliente.h"
#include <commons/log.h>
#include <commons/config.h>
#include <readline/readline.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
    /* Iniciamos el Logger y Config*/

    t_cpu_sockets sockets;
    t_log* logger = iniciar_logger();
    t_config* config = iniciar_config();

    if (logger == NULL || config == NULL) {
        return EXIT_FAILURE;
    }

    log_info(logger, "Modulo CPU iniciado");


    /* ---------------- CONEXIONES ---------------- */

    sockets.conexion_kernel_memory    = conexion_kernel_memory(config, logger, KERNEL_MEMORY);
    enviar_op_code (NUEVA_CPU, sockets.conexion_kernel_memory);
    op_code handshake = recibir_operacion(sockets.conexion_kernel_memory); //Se espera que se devueva el op_code OK (1)
    
    if(handshake = 1){
        log_error(logger, "Error en HandShake con Kernel Memory.");
    }

    

    sockets.conexion_memory_stick     = conexion_memory_stick(config, logger, MEMORY_STICK);
    enviar_op_code (NUEVA_CPU, sockets.conexion_memory_stick);
    op_code handshake = recibir_operacion(sockets.conexion_memory_stick); //Se espera que se devueva el op_code OK (1)
    
     if(handshake = 1){
        log_error(logger, "Error en HandShake con Memory Stick.");
    }
    
    
    
    sockets.conexion_kernel_scheduler = conexion_kernelS(config, logger, KERNEL_SCHEDULER);
    enviar_op_code (NUEVA_CPU, sockets.conexion_kernel_scheduler);
    op_code handshake = recibir_operacion(sockets.conexion_kernel_scheduler); //Se espera que se devueva el op_code OK (1)
    
     if(handshake = 1){
        log_error(logger, "Error en HandShake con Kernel Scheduler.");
    }


    // Validacion de conexiones (Si falla una conexión crítica, cerramos)
    if (sockets.conexion_kernel_memory < 0 || sockets.conexion_kernel_scheduler < 0 || sockets.conexion_memory_stick < 0) {
        
        log_error(logger, "Error al establecer conexiones iniciales."); //Error en valor de los so
        
        terminar_programa(logger, config, sockets);
        
        return EXIT_FAILURE;
    }

    log_info(logger, "Todas las conexiones establecidas con éxito");


    //Al iniciar una CPU obligatoriamente debemos mandar el CPU_LIBRE y esperar un PID (KERNEL SCHEDULER)
    enviar_op_code (CPU_LIBRE, sockets.conexion_kernel_scheduler);

    //Recibir PID
    int PID = recibir_pid(sockets.conexion_kernel_scheduler);

    int control_loop = 1;
    while (control_loop = 1){
    fetch();

    decode();

    execute();

    interrupciones();
    
    }
}

/* ---------------- FUNCIONES ADMINISTRATIVAS ---------------- */

t_log* iniciar_logger(void)
{
    // Usamos LOG_LEVEL_INFO por defecto para asegurar que se vea por consola
    t_log* nuevo_logger = log_create("cpu.log", "CPU", true, LOG_LEVEL_INFO);
    if (nuevo_logger == NULL) {
        perror("No se pudo crear el logger");
    }
    return nuevo_logger;
}

t_config* iniciar_config(void)
{
    char* path = "cpu.config";
    t_config* nuevo_config = config_create(path);
    if (nuevo_config == NULL) {
        printf("¡No se pudo crear el config!\n");
    }
    return nuevo_config;
}

void terminar_programa(t_log* logger, t_config* config, t_cpu_sockets sockets)
{
    if(logger) log_destroy(logger);
    if(config) config_destroy(config);
    
    // Cerramos todos los sockets abiertos
    liberar_conexion(sockets.conexion_kernel_memory);
    liberar_conexion(sockets.conexion_kernel_scheduler);
    liberar_conexion(sockets.conexion_memory_stick);
}



/* ---------------- IMPLEMENTACION DE CONEXIONES ---------------- */

int conexion_kernel_memory(t_config* config, t_log* logger, module_name module) {

    char* ip = config_get_string_value(config, "IP_KERNEL_MEMORY");
    char* puerto = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");

    log_info(logger, "Iniciando conexion con KERNEL MEMORY");

    return crear_conexion(ip, puerto, logger, module);
}

int conexion_kernelS(t_config* config, t_log* logger, module_name module) {

    char* ip = config_get_string_value(config, "IP_KERNEL_SCHEDULER");
    char* puerto = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");
    
    log_info(logger, "Iniciando conexion con KERNEL SCHEDULER");
    
    return crear_conexion(ip, puerto, logger, module);
}

int conexion_memory_stick(t_config* config, t_log* logger, module_name module) {
    
    char* ip = config_get_string_value(config, "IP_MEMORY_STICK");
    char* puerto = config_get_string_value(config, "PUERTO_MEMORY_STICK");
    
    log_info(logger, "Iniciando conexion con MEMORY STICK");
    
    return crear_conexion(ip, puerto, logger, module);
}


/* ---------------- IMPLEMENTACION DE CLICO DE INTRUCCION ---------------- */

int recibir_pid (int socket_cliente){

    int PID;
    if (recv(socket_cliente, &PID, sizeof(int), MSG_WAITALL) > 0) {
        return PID;
    } else {
        close(socket_cliente);
        return -1; 
    }
}

void fetch(); /*COMPLETAR*/

void decode(); /*COMPLETAR*/

void execute(); /*COMPLETAR*/

void interrupciones(); /*COMPLETAR*/