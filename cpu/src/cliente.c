#include "cliente.h"
#include <commons/log.h>
#include <commons/config.h>
#include <readline/readline.h>
#include <string.h>
#include <stdlib.h>


int main(void)
{
    /* ---------------- INITIALIZATION ---------------- */
    t_cpu_sockets sockets;
    t_log* logger = iniciar_logger();
    t_config* config = iniciar_config();

    if (logger == NULL || config == NULL) {
        return EXIT_FAILURE;
    }

    log_info(logger, "Modulo CPU iniciado");

    /* ---------------- LEER CONSOLA ---------------- */
    // Esto es el equivalente al TP0 para probar que el logger funciona
    leer_consola(logger);

    /* ---------------- CONEXIONES ---------------- */
    printf("Intentando conectar sockets...\n");

    sockets.conexion_kernel_memory    = conexion_kernel_memory(config);
    sockets.conexion_kernel_scheduler = conexion_kernelS(config);
    sockets.conexion_kernelS_dispatch  = conexion_kernelS_dispatch(config);
    sockets.conexion_kernelS_interrupt = conexion_kernelS_interrupt(config);
    sockets.conexion_memory_stick     = conexion_memory_stick(config);

    // Validacion de conexiones (Si falla una conexión crítica, cerramos)
    if (sockets.conexion_kernel_memory < 0 || sockets.conexion_kernel_scheduler < 0) {
        log_error(logger, "Error al establecer conexiones iniciales.");
        terminar_programa(logger, config, sockets);
        return EXIT_FAILURE;
    }

    log_info(logger, "Todas las conexiones establecidas con éxito");

    /* ---------------- PRUEBA DE ENVIO ---------------- */
    printf("Seleccione destino (1: MS, 2: KS, 3: KM): ");
    char* seleccion = readline("> ");

    if (seleccion != NULL) {
        if (strcmp(seleccion, "1") == 0) {
            enviar_mensaje("Mensaje a MS", sockets.conexion_memory_stick);
            paquete(sockets.conexion_memory_stick);
        } 
        else if (strcmp(seleccion, "2") == 0) {
            enviar_mensaje("Mensaje a KS", sockets.conexion_kernel_scheduler);
            paquete(sockets.conexion_kernel_scheduler);
        }
        else if (strcmp(seleccion, "3") == 0) {
            enviar_mensaje("Mensaje a KM", sockets.conexion_kernel_memory);
            paquete(sockets.conexion_kernel_memory); // Corregido el socket aquí
        }
        free(seleccion);
    }

    terminar_programa(logger, config, sockets);
    return 0;
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
    nuevo_config = config_create(path);
    if (nuevo_config == NULL) {
        printf("¡No se pudo crear el config!\n");
    }
    return nuevo_config;
}

void leer_consola(t_log* logger)
{
    char* leido;
    while( (leido = readline("> ")) != NULL && strcmp(leido, "") != 0 ) {
        log_info(logger, "Leido de consola: %s", leido);
        free(leido);
    }
    free(leido);
}

void terminar_programa(t_log* logger, t_config* config, t_cpu_sockets sockets)
{
    if(logger) log_destroy(logger);
    if(config) config_destroy(config);
    
    // Cerramos todos los sockets abiertos
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

/* ---------------- MANEJO DE PAQUETES ---------------- */

void paquete(int conexion)
{
    char* leido;
    t_paquete* paquete = crear_paquete(PAQUETE); // Enviamos con op_code PAQUETE

    while( (leido = readline("Paquete> ")) != NULL && strcmp(leido, "") != 0 ) {
        agregar_a_paquete(paquete, leido, strlen(leido) + 1);
        free(leido);
    }
    free(leido);

    enviar_paquete(paquete, conexion);
    eliminar_paquete(paquete);
}