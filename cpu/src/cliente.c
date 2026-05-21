#include "cliente.h"
#include <commons/log.h>
#include <commons/config.h>
#include <readline/readline.h>
#include <string.h>
#include <stdlib.h>

t_log* logger;
 t_cpu_sockets sockets;

int main(void)
{
    /* Iniciamos el Logger y Config*/
    logger = iniciar_logger();
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
    
        char* instruccion_raw = fetch(sockets); 

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

char* fetch() {
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
    char* instruccion_raw = recibir_string(sockets.conexion_kernel_memory); //Hacer recibir string
    
    if (instruccion_raw == NULL) {
        log_error(logger, "Error en fetch");
        return NULL;
    }
    
    return instruccion_raw;

}



t_instruccion* instruccion_decodificada; 

void decode(char* instruccion_raw) {
    char** tokens = string_split(instruccion_raw, " ");
    
    if (tokens == NULL) return EXIT;

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



void* obtener_registro(char* nombre) {
    // registros de 8 bits
    if (strcmp(nombre, "AX") == 0) return &(contexto_actual->ax);
    if (strcmp(nombre, "BX") == 0) return &(contexto_actual->bx);
    if (strcmp(nombre, "CX") == 0) return &(contexto_actual->cx);
    if (strcmp(nombre, "DX") == 0) return &(contexto_actual->dx);

    // registros de 32 bits
    if (strcmp(nombre, "EAX") == 0) return &(contexto_actual->eax);
    if (strcmp(nombre, "EBX") == 0) return &(contexto_actual->ebx);
    if (strcmp(nombre, "ECX") == 0) return &(contexto_actual->ecx);
    if (strcmp(nombre, "EDX") == 0) return &(contexto_actual->edx);
    if (strcmp(nombre, "SI") == 0)  return &(contexto_actual->si);
    if (strcmp(nombre, "DI") == 0)  return &(contexto_actual->di);
    if (strcmp(nombre, "PC") == 0)  return &(contexto_actual->pc);

    log_error(logger, "Registro inexistente: %s", nombre);
    return NULL;
}

bool es_registro_32bits(char* nombre) {
    // si empieza con E o es SI o DI o PC  es de 32 bits
    return (nombre[0] == 'E' || strcmp(nombre, "SI") == 0 || strcmp(nombre, "DI") == 0 || strcmp(nombre, "PC") == 0);
}

void execute() {
    t_instruccion* instr = instruccion_decodificada;
    
    switch (instr->codigo) {
        case NOOP:      
            ejecutar_noop(instr);      
            break;
        
        case SET:      
            ejecutar_set(instr);      
            break;

         case MOV_IN:      
            ejecutar_mov_out(instr);      
            break;

        case MOV_OUT:      
            ejecutar_mov_out(instr);      
            break;

        case SUM:      
            ejecutar_sum(instr);      
            break;

        case SUB:
            ejecutar_sub(instr);
            break;

        case JNZ:
            ejecutar_jnz(instr);
            break;
    
        case COPY_MEM:
            ejecutar_copy_mem(instr);
            break;
        
        case SLEEP:

            break;
        
        case STDOUT:

            break;
        
        case STDIN:

            break;
        
        case INIT_PROC:

            break;

        case EXIT:
        
            break;

        default:
            log_warning(logger, "Instruccion no definida en execute");
            break;
    }
}

void ejecutar_noop (t_instruccion* instr){

    //No hace nada

}

void ejecutar_set (t_instruccion* instr){

    char* reg_dest_nombre = instr->params[0];

    if (es_registro_32bits(reg_dest_nombre)){
        uint32_t valor = (uint32_t) strtoul(instr->params[1], NULL, 10);
        uint32_t* dest = (uint32_t*)obtener_registro(reg_dest_nombre);
        
        *dest = valor;

        log_info(logger, "[EXEC] SET 32b: %s = %u", reg_dest_nombre, *dest);
    }
    else {
        uint8_t valor = (uint8_t) strtoul(instr->params[1], NULL, 10);
        uint8_t* dest = (uint8_t*)obtener_registro(reg_dest_nombre);

        *dest = valor;

        log_info(logger, "[EXEC] SUM 8b: %s = %u", reg_dest_nombre, *dest);
    }

}

void ejecutar_mov_in (t_instruccion* instr){

    char* reg_dest_nombre = instr->params[0];

    if (es_registro_32bits(reg_dest_nombre)){
        uint32_t* dest = (uint32_t*)obtener_registro(reg_dest_nombre);
        uint32_t dir_fisica = pedir_direccion_a_mmu(contexto_actual->si);
    
        // Comunicación con Memory Stick
        void* buffer = leer_de_memoria(dir_fisica, sizeof(uint32_t));
        *dest = *(uint32_t*)buffer;
        free(buffer);

        log_info(logger, "[EXEC] MOV_IN 32b: %s = %u", reg_dest_nombre, *dest);
    }   
    else {
        uint8_t* dest = (uint8_t*)obtener_registro(reg_dest_nombre);
        uint8_t dir_fisica = pedir_direccion_a_mmu(contexto_actual->si);
    
        // Comunicación con Memory Stick
        void* buffer = leer_de_memoria(dir_fisica, sizeof(uint32_t));
        *dest = *(uint8_t*)buffer;
        free(buffer);

        log_info(logger, "[EXEC] MOV_IN 32b: %s = %u", reg_dest_nombre, *dest);


    }
}

void ejecutar_mov_out (t_instruccion* instr){

    char* reg_dest_nombre = instr->params[0];

    if (es_registro_32bits(reg_dest_nombre)){
        uint32_t* dest = (uint32_t*)obtener_registro(reg_dest_nombre);
        uint32_t dir_fisica = pedir_direccion_a_mmu(contexto_actual->di);
    
        // Comunicación con Memory Stick
        void* buffer = (void*)dest;
        escribir_en_memoria(dir_fisica, buffer, sizeof(uint32_t));
        free(buffer);

        log_info(logger, "[EXEC] MOV_OUT 32b: %s = %u", reg_dest_nombre, *dest);
    }   
    else {
        uint8_t* dest = (uint8_t*)obtener_registro(reg_dest_nombre);
        uint8_t dir_fisica = pedir_direccion_a_mmu(contexto_actual->di);
    
        // Comunicación con Memory Stick
        void* buffer = (void*)dest;
        escribir_en_memoria(dir_fisica, buffer, sizeof(uint8_t));
        free(buffer);

        log_info(logger, "[EXEC] MOV_OUT 8b: %s = %u", reg_dest_nombre, *dest);


    }

}










void ejecutar_sum(t_instruccion* instr) {
    
    char* reg_dest_nombre = instr->params[0];
    char* reg_orig_nombre = instr->params[1];

    if (es_registro_32bits(reg_dest_nombre)) {
        uint32_t* dest = (uint32_t*)obtener_registro(reg_dest_nombre);
        uint32_t* orig = (uint32_t*)obtener_registro(reg_orig_nombre);
        *dest += *orig;
        log_info(logger, "[EXEC] SUM 32b: %s = %u", reg_dest_nombre, *dest);
    } else {
        uint8_t* dest = (uint8_t*)obtener_registro(reg_dest_nombre);
        uint8_t* orig = (uint8_t*)obtener_registro(reg_orig_nombre);
        *dest += *orig;
        log_info(logger, "[EXEC] SUM 8b: %s = %u", reg_dest_nombre, *dest);
    }
}

void ejecutar_sub(t_instruccion* instr) {
    char* reg_dest_nombre = instr->params[0];
    char* reg_orig_nombre = instr->params[1];

    if (es_registro_32bits(reg_dest_nombre)) {
        uint32_t* dest = (uint32_t*)obtener_registro(reg_dest_nombre);
        uint32_t* orig = (uint32_t*)obtener_registro(reg_orig_nombre);
        *dest -= *orig;
        log_info(logger, "[EXEC] SUB 32b: %s = %u", reg_dest_nombre, *dest);
    } else {
        uint8_t* dest = (uint8_t*)obtener_registro(reg_dest_nombre);
        uint8_t* orig = (uint8_t*)obtener_registro(reg_orig_nombre);
        *dest -= *orig;
        log_info(logger, "[EXEC] SUB 8b: %s = %u", reg_dest_nombre, *dest);
    }
}

void ejecutar_jnz(t_instruccion* instr) {
    // los JNZ --> gemini dice que no es necesario los de 8 bits xq generalmente operan sobre registros de 32 bits (como el PC o contadores)
    uint32_t* registro = (uint32_t*)obtener_registro(instr->params[0]);
    if (*registro != 0) {

        /*atoi toma una cadena de caracteres q contiene números y la traduce a su valor numérico real en meomria
        --> Si no usas atoi estarías intentando asignar una dirección de memoria (donde vive el texto "5")*/
        contexto_actual->pc = (uint32_t)atoi(instr->params[1]);
        log_info(logger, "[EXEC] JNZ: Salto a PC %u", contexto_actual->pc);
    }
}

void ejecutar_copy_mem(t_instruccion* instr) {
    
    // obtener tamaño que puede ser un número fijo (atoi) o un registro
    int tamanio;
    if (isdigit(instr->params[0][0])) {
        tamanio = atoi(instr->params[0]);
    } else {
        // Si el parámetro no es un número, asumimos que es un registro
        uint32_t* reg_tam = (uint32_t*)obtener_registro(instr->params[0]);
        tamanio = *reg_tam;
    }

    // obtener direcciones lógicas desde los registros SI y DI
    uint32_t* dir_logica_origen = (uint32_t*)obtener_registro("SI");
    uint32_t* dir_logica_destino = (uint32_t*)obtener_registro("DI");

    // COMUNICACIÓN COM MMU (CP 3?) para obtener direcciones físicas
    uint32_t dir_fisica_origen = pedir_direccion_a_mmu(*dir_logica_origen);
    uint32_t dir_fisica_destino = pedir_direccion_a_mmu(*dir_logica_destino);
    
    // Comunicación con Memory Stick
    void* buffer = leer_de_memoria(dir_fisica_origen, tamanio);
    escribir_en_memoria(dir_fisica_destino, buffer, tamanio);
    
    free(buffer);
    
    log_info(logger, "[EXEC] COPY_MEM: Copiados %d bytes de SI(log:%u) a DI(log:%u)", 
             tamanio, *dir_logica_origen, *dir_logica_destino);
}


void* leer_de_memoria(uint32_t dir_fisica, int tamanio) {

    // preparar el paquete (Protocolo: DIRECCION_FISICA, TAMANIO)
    t_paquete* paquete = crear_paquete(LEER_MEMORIA);
    agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    
    enviar_paquete(paquete, sockets.conexion_kernel_memory);
    eliminar_paquete(paquete);

    // recibir la respuesta (el buffer con los datos)
    // asumiendo que el protocolo de las chicas devuelve un buffer de bytes
    void* buffer = malloc(tamanio);
    recibir_datos(sockets.conexion_kernel_memory, buffer, tamanio); 
    
    return buffer;
}

void escribir_en_memoria(uint32_t dir_fisica, void* buffer, int tamanio) {
    // preparar el paquete (Protocolo: DIRECCION_FISICA, TAMANIO, DATA)
    t_paquete* paquete = crear_paquete(ESCRIBIR_MEMORIA);
    agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    agregar_a_paquete(paquete, buffer, tamanio);
    
    enviar_paquete(paquete, sockets.conexion_kernel_memory);
    eliminar_paquete(paquete);
    
    // 2. Esperar confirmación de la memoria
    int resultado = recibir_operacion(sockets.conexion_kernel_memory);
    if (resultado != OK) {
        log_error(logger, "Error al escribir en memoria física %u", dir_fisica);
    }
}

/*  Cuando hagamos MMU tenemos que hacer:
   1) pedir_direccion_a_mmu  

*/

void interrupciones(); /*COMPLETAR*/