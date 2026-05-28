#include "cliente.h"

int main(void)
{
    /* ---------------- Iniciamos el Logger y Config ----------------*/
    logger = iniciar_logger();
    t_config* config = iniciar_config();

    if (logger == NULL || config == NULL) {
        return EXIT_FAILURE;
    }

    log_info(logger, "Modulo CPU iniciado");


    /* ---------------- CONEXIONES ---------------- */

       sockets->conexion_kernel_memory = conexion_kernel_memory(config, logger, KERNEL_MEMORY);
    enviar_op_code (NUEVA_CPU, sockets->conexion_kernel_memory);
    op_code handshake_km = recibir_operacion(sockets->conexion_kernel_memory); //Se espera que se devueva el op_code OK (1)
    
    if(handshake_km != 1){
        log_error(logger, "Error en HandShake con Kernel Memory.");

        return EXIT_FAILURE;
    }

    

    sockets->conexion_memory_stick = conexion_memory_stick(config, logger, MEMORY_STICK);
    enviar_op_code (NUEVA_CPU, sockets->conexion_memory_stick);
    op_code handshake_ms = recibir_operacion(sockets->conexion_memory_stick); //Se espera que se devueva el op_code OK (1)
    
     if(handshake_ms != 1){
        log_error(logger, "Error en HandShake con Memory Stick.");

        return EXIT_FAILURE;
    }
    
    
    
    sockets->conexion_kernel_scheduler = conexion_kernelS(config, logger, KERNEL_SCHEDULER);
    enviar_op_code (NUEVA_CPU, sockets->conexion_kernel_scheduler);
    op_code handshake_ks = recibir_operacion(sockets->conexion_kernel_scheduler); //Se espera que se devueva el op_code OK (1)
    
     if(handshake_ks != 1){
        log_error(logger, "Error en HandShake con Kernel Scheduler.");

        return EXIT_FAILURE;
    }


    // Validacion de conexiones (Si falla una conexión crítica, cerramos)
    if (sockets->conexion_kernel_memory < 0 || sockets->conexion_kernel_scheduler < 0 || sockets->conexion_memory_stick < 0) {
        
        log_error(logger, "Error al establecer conexiones iniciales."); //Error en valor de los so
        
        terminar_programa(logger, config, sockets);
        
        return EXIT_FAILURE;
    }

    log_info(logger, "Todas las conexiones establecidas con éxito");


    /*----- SOLICITAMOS UN PROCESO -----*/
    control_loop00 = 1;
    while(control_loop00 == 1)
    {
        
        enviar_op_code (CPU_LIBRE, sockets->conexion_kernel_scheduler); //Al iniciar una CPU obligatoriamente debemos mandar el CPU_LIBRE y esperar un PID (KERNEL SCHEDULER)
        proceso_en_ejecucion->pid = recibir_pid(sockets->conexion_kernel_scheduler);

        control_loop = 1;
        while (control_loop == 1){
        
            char* instruccion_raw = fetch(sockets); /* Fase Fetch */
            if (instruccion_raw == NULL) return EXIT_FAILURE;
            
            decode(instruccion_raw); /* Fase Decode */
            
            contexto_actual->pc++; //La sumatoria en 1 del PC se hace en esta parte para evitar errores

            execute(); /* Fase Execute */

            enviar_op_code (DESALOJO, sockets->conexion_kernel_scheduler); //Se le consulta al KS si se debe desalojar.
            int cod_op = recibir_op_code (sockets->conexion_kernel_scheduler);
            if (cod_op == DESALOJO) {
                interrupciones();
                continue; // Saltamos el ciclo de ejecución actual
            }
        }

            liberar_instruccion(instruccion_decodificada);
            instruccion_decodificada = NULL;
            proceso_en_ejecucion->pid = NULL;
            control_loop00 = apagar(); //apagar sea una funcion que segun el valor de una variable global corta la cpu o no
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
    t_config* nuevo_config  = config_create(path);
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

char* fetch() {

    log_info(logger, "[FETCH] Solicitando instruccion para PID: %d, PC: %u", 
             contexto_actual->pid, 
             contexto_actual->pc);

    
    enviar_op_code(FETCH, sockets->kernel_memory); //Le informamos al KM que vamos a solicitarle una instruccion

    t_paquete* paquete = crear_paquete(FETCH);
    agregar_a_paquete(paquete, &(contexto_actual->pid), sizeof(int));
    agregar_a_paquete(paquete, &(contexto_actual->pc), sizeof(uint32_t));

    enviar_paquete(paquete, sockets->conexion_kernel_memory); // Enviamos a KM los datos que necesita para identificar la intruccion
    eliminar_paquete(paquete);

    
    char* instruccion_raw = recibir_mensaje(sockets->conexion_kernel_memory); // recibimos el string de la instrucción
    
    if (instruccion_raw == NULL) {
        log_error(logger, "Error en fetch");
        return NULL;
    }
    
    return instruccion_raw;

}

void decode(char* instruccion_raw) {
    char** tokens = string_split(instruccion_raw, " ");
    
    if (tokens == NULL) return EXIT;

    instruccion_decodificada = malloc(sizeof(t_instruccion));
    instruccion_decodificada->cant_params = 0;

    // identificamos el tipo de instrucción (el primer token)
    instruccion_decodificada->codigo = identificar_codigo(tokens[0]);

    // cargamos los parámetros (los tokens restantes)
    int i = 1;
    while(tokens[i] != NULL) {
        instruccion_decodificada->params[instruccion_decodificada->cant_params] = strdup(tokens[i]);
        instruccion_decodificada->cant_params++;
        i++;
    }

    // todos los free
    string_iterate_lines(tokens, free);
    free(tokens);
    free(instruccion_raw);
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
        
        case MUTEX_CREATE:
            ejecutar_mutex_create(instr);
            break;    
        
        case MUTEX_LOCK:
            ejecutar_mutex_lock(instr);
            break;

        case MUTEX_UNLOCK:
            ejecutar_mutex_unlock(instr);
            break;

        case MEM_ALLOC:
            ejecutar_mem_alloc(instr);
            break;

        case MEM_FREE:
            ejecutar_mem_free(instr);
            break;

        case SLEEP:
            ejecutar_sleep(instr); 
            break;
        
        case STDOUT:
            ejecutar_stdout(instr);
            break;
            
        case STDIN:
            ejecutar_stdin(instr);
            break;
        
        case INIT_PROC:
            ejecutar_init_proc(instr);
            break;

        case EXIT:
            ejecutar_exit();
            break;

        default:
            log_warning(logger, "Instruccion no definida en execute");
            break;
    }
    
}

void interrupciones(){
    //  sería la interrupción de COMPACTACIÖN por parte de la ks asi que habría que sacar el proceso
    //  corriendo actualmente en esta cpu (y se lo enviamos a todas las cpus)

    gestionar_desalojo_por_syscall(-1, DESALOJO);

}

/* ---------------- FUNCIONES COMPLEMENTARIAS PARA CICLO DE INTRUCCION ---------------- */

t_instruccion_code identificar_codigo(char* token) { // función para traducir el string al enum de instru
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

/* ---------------- FUNCIONES DE EXECUTE POR INTRUCCION ---------------- */

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

void ejecutar_mutex_create(t_instruccion* instr){

    char* mutex_id = instr->params[0];
    op_code err;

    enviar_op_code (MUTEX_CREATE, sockets->conexion_kernel_scheduler); //Envia la señal
    
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if(err != OK) return EXIT_FAILURE;//MARCAR EL ERROR.

    enviar_mensaje (mutex_id, sockets->conexion_kernel_scheduler); // Se manda el nombre del semaforo

    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK) return EXIT_FAILURE; //MARCAR ERROR
    
    log_info(logger, ""); //Completar LOG
}

void ejecutar_mutex_lock (t_instruccion* instr){

    char* mutex_id = instr->params[0];
    op_code err;

    enviar_op_code (MUTEX_LOCK, sockets->conexion_kernel_scheduler); //Envia la señal

    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if(err != OK) return EXIT_FAILURE;//MARCAR ERROR

    enviar_mensaje (mutex_id, sockets->conexion_kernel_scheduler); // Se manda el nombre del semaforo

    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK) return EXIT_FAILURE; //MARCAR ERROR

    log_info (logger, ""); //Completar LOG
}

void ejecutar_mutex_unlock (t_instruccion* instr){

    char* mutex_id = instr->params[0];
    op_code err;

    enviar_op_code (MUTEX_UNLOCK, sockets->conexion_kernel_scheduler); //Envia la señal

    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if(err != OK) return EXIT_FAILURE;//MARCAR ERROR

    enviar_mensaje (mutex_id, sockets->conexion_kernel_scheduler); // Se manda el nombre del semaforo

    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK) return EXIT_FAILURE; //MARCAR ERROR

    log_info (logger, ""); //Completar LOG
}

void ejecutar_mem_alloc (t_instruccion* instr){

    char* id_segmento = instr->params[0];
    char* tamanio = instr->params[1];
    op_code err;

    enviar_op_code (MEM_ALLOC, sockets->conexion_kernel_scheduler); //Envia la señal
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if(err != OK) return EXIT_FAILURE; //MARCAR ERROR

    enviar_mensaje (id_segmento, sockets->conexion_kernel_scheduler); 
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK) return EXIT_FAILURE; //MARCAR ERROR


    enviar_mensaje (tamanio, sockets->conexion_kernel_scheduler); 
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK) return EXIT_FAILURE; //MARCAR ERROR
    log_info (logger, ""); //Completar LOG
            
}

void ejecutar_mem_free (t_instruccion* instr){

    char* id_segmento = instr->params[0];
    op_code err;

    enviar_op_code (MEM_FREE, sockets->conexion_kernel_scheduler); //Envia la señal
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if(err != OK) return EXIT_FAILURE;

    enviar_mensaje (id_segmento, sockets->conexion_kernel_scheduler); 
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK) return EXIT_FAILURE;

}

void ejecutar_sleep(t_instruccion* instr) {
    
    char* tiempo = strdup(instr->params[0]);
    
    gestionar_desalojo_por_syscall(tiempo, ks_SLEEP);    

    if (recibir_operacion(sockets->conexion_kernel_scheduler) != OK) {
        log_info(logger, "Syscall SLEEP NO ACEPTADA por KS");
    }
}

void ejecutar_stdout(t_instruccion* instr) {
    char* reg_dir = instr->params[0];
    char* reg_tam = instr->params[1];

    uint32_t pid_actual = proceso_en_ejecucion->pid;
    
    uint32_t direccion_logica;
    void* ptr_dir = obtener_registro(reg_dir);
    direccion_logica = es_registro_32bits(reg_dir) ? *(uint32_t*)ptr_dir : (uint32_t)(*(uint8_t*)ptr_dir);

    uint32_t tamanio;
    void* ptr_tam = obtener_registro(reg_tam);
    tamanio = es_registro_32bits(reg_tam) ? *(uint32_t*)ptr_tam : (uint32_t)(*(uint8_t*)ptr_tam);

    log_info(logger, "PID: %u - Ejecutando STDOUT en interfaz: %s", pid_actual, interfaz_nombre);

    t_paquete* paquete = crear_paquete(ks_IO_STDOUT);

    agregar_a_paquete(paquete, &pid_actual, sizeof(uint32_t));
    agregar_a_paquete(paquete, &direccion_logica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));

    // Enviar a ks y desalojar proceso
    enviar_paquete(paquete, sockets->conexion_kernel_scheduler);
    eliminar_paquete(paquete);

    gestionar_desalojo_por_syscall(-1, ks_IO_STDOUT);
}

void ejecutar_stdin(t_instruccion* instr) {

    char* reg_dir = instr->params[1];
    char* reg_tam = instr->params[2];

    uint32_t direccion_logica;
    uint32_t tamanio;

    // --- Extraer Dirección ---
    void* ptr_dir = obtener_registro(reg_dir);
    if (es_registro_32bits(reg_dir)) {
        direccion_logica = *(uint32_t*)ptr_dir;
    } else {
        direccion_logica = (uint32_t)(*(uint8_t*)ptr_dir); // Casteo de 8 bits
    }

    // --- Extraer Tamaño ---
    void* ptr_tam = obtener_registro(reg_tam);
    if (es_registro_32bits(reg_tam)) {
        tamanio = *(uint32_t*)ptr_tam;
    } else {
        tamanio = (uint32_t)(*(uint8_t*)ptr_tam);
    }

    
    uint32_t tamanio = obtener_tamanio_del_registro(instr->params[1]); 
    uint32_t direccion_logica = obtener_direccion_del_registro(instr->params[1]);  //HACER obtener_direccion_del_registro --> MMU
    uint32_t pid_actual = proceso_en_ejecucion->pid; 

    t_paquete* paquete = crear_paquete(ks_IO_STDIN);

    agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
    agregar_a_paquete(paquete, &direccion_logica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &pid_actual, sizeof(uint32_t));
    

    enviar_paquete(paquete, sockets->conexion_kernel_scheduler);
    eliminar_paquete(paquete);

    gestionar_desalojo_por_syscall(-1, ks_IO_STDIN);
}

void ejecutar_init_proc(t_instruccion* instr) {
   
    char* path = instr->params[0];
    int prioridad = atoi(instr->params[1]);

    log_info(logger, "PID %d solicitando crear nuevo proceso: %s", contexto_actual->pid, path);

    enviar_op_code(ks_INIT_PROC, sockets->conexion_kernel_scheduler);

    t_paquete* paquete = crear_paquete(ks_INIT_PROC);
    agregar_a_paquete(paquete, path, strlen(path) + 1);
    agregar_a_paquete(paquete, &prioridad, sizeof(int));
    enviar_paquete(paquete, sockets->conexion_kernel_scheduler);
    eliminar_paquete(paquete);

    if (recibir_operacion(sockets->conexion_kernel_scheduler) == OK) {
        log_info(logger, "Proceso creado exitosamente por el Kernel.");
    }
}

void ejecutar_exit() {
    
    log_info(logger, "PID: %d - Ejecutando EXIT", contexto_actual->pid);

    enviar_op_code(gl_EXIT, sockets->conexion_kernel_scheduler);

    t_paquete* paquete = crear_paquete(gl_EXIT);
    agregar_a_paquete(paquete, &contexto_actual->pid, sizeof(int));
    enviar_paquete(paquete, sockets->conexion_kernel_scheduler);
    eliminar_paquete(paquete);

    if (recibir_operacion(sockets->conexion_kernel_scheduler) == OK) {
        log_info(logger, "EXIT confirmado. Limpiando CPU.");
        limpiar_contexto_actual();
        control_loop = 0; //hace que se vuelva a mandar CPU_LIBRE
    }
}

/* ------------------ Funciones Auxiliares ------------------*/

void limpiar_contexto_actual() {
    log_info(logger, "Limpiando contexto del proceso actual...");

    if (contexto_actual != NULL) {
        free(contexto_actual);
        contexto_actual = NULL;
    }

    log_info(logger, "Contexto limpio. CPU lista para recibir nuevo PID.");
}

void enviar_contexto_a_kernel_memory() {

    int err;
    
    enviar_op_code (CONTEXTO, sockets->conexion_kernel_memory);

    t_paquete* paquete = crear_paquete(CONTEXTO);

    agregar_a_paquete(paquete, &contexto_actual->pid, sizeof(int));
    agregar_a_paquete(paquete, &contexto_actual->pc, sizeof(uint32_t));
    
    // Registros 8 bits
    agregar_a_paquete(paquete, &contexto_actual->ax, sizeof(uint8_t));
    agregar_a_paquete(paquete, &contexto_actual->bx, sizeof(uint8_t));
    agregar_a_paquete(paquete, &contexto_actual->cx, sizeof(uint8_t));
    agregar_a_paquete(paquete, &contexto_actual->dx, sizeof(uint8_t));
    
    // Registros 32 bits
    agregar_a_paquete(paquete, &contexto_actual->eax, sizeof(uint32_t));
    agregar_a_paquete(paquete, &contexto_actual->ebx, sizeof(uint32_t));
    agregar_a_paquete(paquete, &contexto_actual->ecx, sizeof(uint32_t));
    agregar_a_paquete(paquete, &contexto_actual->edx, sizeof(uint32_t));
    agregar_a_paquete(paquete, &contexto_actual->si, sizeof(uint32_t));
    agregar_a_paquete(paquete, &contexto_actual->di, sizeof(uint32_t));

    enviar_paquete(paquete, sockets->conexion_kernel_memory);
    eliminar_paquete(paquete);

    err = recibir_op_code (sockets->conexion_kernel_memory);
    if (err != OK) return EXIT_FAILURE;

}

void* leer_de_memoria(uint32_t dir_fisica, int tamanio) {

    enviar_op_code(LEER_MEMORIA, sockets->conexion_kernel_memory); 
   
    // preparar el paquete (Protocolo: DIRECCION_FISICA, TAMANIO)
    t_paquete* paquete = crear_paquete(LEER_MEMORIA);
    agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    
    enviar_paquete(paquete, sockets->conexion_kernel_memory);
    eliminar_paquete(paquete);

    // recibir la respuesta (el buffer con los datos)
    // asumiendo que el protocolo del KM devuelve un buffer de bytes
    void* buffer = malloc(tamanio);
    buffer = recibir_paquete(sockets->conexion_kernel_memory);
    
    return buffer;
}

void escribir_en_memoria(uint32_t dir_fisica, void* buffer, int tamanio) {
    
    enviar_op_code(ESCRIBIR_MEM, sockets->conexion_kernel_memory);

    // preparar el paquete (Protocolo: DIRECCION_FISICA, TAMANIO, DATA)
    t_paquete* paquete = crear_paquete(ESCRIBIR_MEMORIA);
    agregar_a_paquete(paquete, &dir_fisica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(int));
    agregar_a_paquete(paquete, buffer, tamanio);
    
    enviar_paquete(paquete, sockets->conexion_kernel_memory);
    eliminar_paquete(paquete);
    
    // esperar confirmación de la memoria
    int resultado = recibir_operacion(sockets->conexion_kernel_memory);
    if (resultado != OK) {
        log_error(logger, "Error al escribir en memoria física %u", dir_fisica);
        
        return EXIT_FAILURE;
    }
}

void gestionar_desalojo_por_syscall(char* valor, op_code tipo_operacion) {

    enviar_contexto_a_kernel_memory(); 
    t_paquete* paquete = crear_paquete(tipo_operacion); // <-- El OP_CODE es dinámico

    
    if(strcmp(valor) == -1){
        agregar_a_paquete(paquete, &contexto_actual->pid, sizeof(int));
    }
    else{
        agregar_a_paquete(paquete, &contexto_actual->pid, sizeof(int));
        agregar_a_paquete(paquete, valor, strlen(valor) + 1);
    }
     
    
    //enviar_op_code (identificador, sockets->conexion_kernel_scheduler); "idea para que el KS sepa que va a recibir"

    enviar_paquete(paquete, sockets->conexion_kernel_scheduler);
    eliminar_paquete(paquete);

    op_code status = recibir_operacion(sockets->conexion_kernel_scheduler);
    
    if(status == OK) {
        log_info(logger, "Desalojo confirmado. Limpiando CPU.");
        limpiar_contexto_actual();
    }
}

int pedir_direccion_mmu (int32_t dir_logica){ //Hacer

}

