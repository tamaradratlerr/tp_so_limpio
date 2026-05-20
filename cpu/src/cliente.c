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
    op_code handshake_km = recibir_operacion(sockets.conexion_kernel_memory); //Se espera que se devueva el op_code OK (1)
    
    if(handshake_km != 1){
        log_error(logger, "Error en HandShake con Kernel Memory.");
    }

    

    sockets.conexion_memory_stick     = conexion_memory_stick(config, logger, MEMORY_STICK);
    enviar_op_code (NUEVA_CPU, sockets.conexion_memory_stick);
    op_code handshake_ms = recibir_operacion(sockets.conexion_memory_stick); //Se espera que se devueva el op_code OK (1)
    
     if(handshake_ms != 1){
        log_error(logger, "Error en HandShake con Memory Stick.");
    }
    
    
    
    sockets.conexion_kernel_scheduler = conexion_kernelS(config, logger, KERNEL_SCHEDULER);
    enviar_op_code (NUEVA_CPU, sockets.conexion_kernel_scheduler);
    op_code handshake_ks = recibir_operacion(sockets.conexion_kernel_scheduler); //Se espera que se devueva el op_code OK (1)
    
     if(handshake_ks != 1){
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
    while (control_loop == 1){
    
        char* instruccion_raw = fetch(); 

    if (instruccion_raw != NULL) {
        decode(instruccion_raw); 
    }

    execute();

    interrupciones();

    liberar_instruccion(instruccion_decodificada);
    instruccion_decodificada = NULL;
    
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


t_contexto* contexto_actual; 

void fetch() {
    log_info(logger, "[FETCH] Solicitando instruccion para PID: %d, PC: %u", 
             contexto_actual->pid, 
             contexto_actual->pc);

    // solicitud para la km
    // FIJANOS QUE EL PPROTOCOLO CONCIDA CON KM (PID y PC)
    t_paquete* paquete = crear_paquete(SOLICITUD_INSTRUCCION);
    agregar_a_paquete(paquete, &(contexto_actual->pid), sizeof(int));
    agregar_a_paquete(paquete, &(contexto_actual->pc), sizeof(uint32_t));

    enviar_paquete(paquete, sockets.conexion_kernel_memory);
    eliminar_paquete(paquete);

    // recibimos el string de la instrucción
    char* instruccion_raw = recibir_string(sockets.conexion_kernel_memory);
    
    if (instruccion_raw == NULL) {
        log_error(logger, "Error en fetch");
        return NULL;
    }
    
    return instruccion_raw;

}



t_instruccion* instruccion_decodificada; 

void decode(char* instruccion_raw) {
    char** tokens = string_split(instruccion_raw, " ");
    
    instruccion_decodificada = malloc(sizeof(t_instruccion));
    instruccion_decodificada->cant_params = 0;

    // identificamos el tipo de instrucción (el primer token)
    instruccion_decodificada->codigo = identificar_codigo(tokens[0]);

    // cargamos los parámetros (los tokens restantes)
    int i = 1;
    while (tokens[i] != NULL) {
        instruccion_decodificada->params[instruccion_decodificada->cant_params] = strdup(tokens[i]);
        instruccion_decodificada->cant_params++;
        i++;
    }

    // todos los free
    string_iterate_lines(tokens, free);
    free(tokens);
    free(instruccion_raw);
}

// función para traducir el string al enum de instru


/* no use switch proque cuando le pregunte a gemini si estaba bien programado me dijo
   que: Usar if-else con strcmp es necesario en C porque los switch no pueden evaluar 
   strings directamente*/

t_instruccion_code identificar_codigo(char* token) {
    if (strcmp(token, "NOOP") == 0)          return NOOP;
    if (strcmp(token, "SET") == 0)           return SET;
    if (strcmp(token, "SUM") == 0)           return SUM;
    if (strcmp(token, "SUB") == 0)           return SUB;
    if (strcmp(token, "JNZ") == 0)           return JNZ;
    if (strcmp(token, "COPY_MEM") == 0)      return COPY_MEM;
    if (strcmp(token, "MOV_IN") == 0)        return MOV_IN;
    if (strcmp(token, "MOV_OUT") == 0)       return MOV_OUT;
    if (strcmp(token, "MUTEX_CREATE") == 0)  return MUTEX_CREATE;
    if (strcmp(token, "MUTEX_LOCK") == 0)    return MUTEX_LOCK;
    if (strcmp(token, "MUTEX_UNLOCK") == 0)  return MUTEX_UNLOCK;
    if (strcmp(token, "MEM_ALLOC") == 0)     return MEM_ALLOC;
    if (strcmp(token, "MEM_FREE") == 0)      return MEM_FREE;
    if (strcmp(token, "SLEEP") == 0)         return SLEEP;
    if (strcmp(token, "STDOUT") == 0)        return STDOUT;
    if (strcmp(token, "STDIN") == 0)         return STDIN;
    if (strcmp(token, "INIT_PROC") == 0)     return INIT_PROC;
    if (strcmp(token, "EXIT") == 0)          return EXIT;

    // caso por defecto si no reconoce el comando
    if (token == NULL) return EXIT;
}

void execute(); /*COMPLETAR*/

void interrupciones(); /*COMPLETAR*/