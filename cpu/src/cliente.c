#include "cliente.h"


/*--- Variable global para hacer pruebas sin KM y sin STICK ---*/

bool mock = false; 

/*-----                        MAIN                        -----*/

int main(int argc, char *argv[])
{

    if(argc != 3){
        printf("ERROR: Usar: ./bin/cpu [Archivo Config] [Identificador]\n");
        return 1;
    }

   
    /*---Iniciando Log y Config---*/
    char* archivo_config = argv[1];
    identificador = argv[2];
    config_cpu = malloc(sizeof(config_cpu));
    
    iniciar_log_config(archivo_config, identificador);
    config_cpu->tiempo_instruccion =
    config_get_int_value(config, "TIEMPO_INSTRUCCION");

    config_cpu->tam_max_segmento =
    config_get_int_value(config, "TAM_MAX_SEGMENTO");

    if (logger == NULL || config == NULL) {
        printf("Error al Inciar Logger o Config");
        return EXIT_FAILURE;
    }

    log_info(logger, "=====     Modulo CPU iniciado     =====");
    if(mock){log_debug(logger,"MOCK ACTIVADO");}

    /* ---------------- CONEXIONES ---------------- */

    if(!mock)
    { 
        sockets->conexion_kernel_memory = conexion_kernel_memory(config, logger, KERNEL_MEMORY);
        enviar_op_code (NUEVA_CPU, sockets->conexion_kernel_memory);
        op_code handshake_km = recibir_op_code(sockets->conexion_kernel_memory);
        
        if(handshake_km != OK)
        {
            log_error(logger, "Error en HandShake con Kernel Memory. valor OP_CODE: [%d]",handshake_km);
            return EXIT_FAILURE;
        }

    }
        sockets->memory_sticks = list_create();
        sem_init(&mutex_memory_sticks, 0, 1);

        t_mem_stick* ms_inicial = malloc(sizeof(t_mem_stick));


        ms_inicial->ip = 
            config_get_string_value(config, "IP_MEMORY_STICK");


        ms_inicial->puerto =
            config_get_string_value(config, "PUERTO_MEMORY_STICK");
        
        log_info(logger,
            "Intentando conectar a Memory Stick %s:%s",
            ms_inicial->ip,
            ms_inicial->puerto
        );

        ms_inicial->socket =
            crear_conexion(
                ms_inicial->ip,
                ms_inicial->puerto,
                logger,
                MEMORY_STICK
            );


        if(ms_inicial->socket < 0)
        {
            log_error(logger,
                "No se pudo conectar al Memory Stick inicial");

            return EXIT_FAILURE;
        }

        enviar_op_code(
            NUEVA_CPU,
            ms_inicial->socket
        );


        op_code respuesta = recibir_op_code(ms_inicial->socket);


        if(respuesta != OK)
        {
            log_error(logger,
                "Handshake con Memory Stick falló");

            return EXIT_FAILURE;
        }

        ms_inicial->base = (uint32_t) recibir_int(ms_inicial->socket);
        ms_inicial->tamanio = (uint32_t) recibir_int(ms_inicial->socket);
        
        sem_wait(&mutex_memory_sticks);

        list_add(
            sockets->memory_sticks,
            ms_inicial
        );

        sem_post(&mutex_memory_sticks);
    

    sockets->conexion_kernel_scheduler = conexion_kernelS(config, logger, KERNEL_SCHEDULER);
    
    enviar_op_code (NUEVA_CPU, sockets->conexion_kernel_scheduler);
    enviar_mensaje(identificador,sockets->conexion_kernel_scheduler);
    
    op_code handshake_ks = recibir_op_code(sockets->conexion_kernel_scheduler);
    
    if(handshake_ks != OK)
    {
        log_error(logger, "Error en HandShake con Kernel Scheduler valor OP_CODE: [%d]",handshake_ks);
        return EXIT_FAILURE;
    }

    if(!mock)
    {
        // Validacion de conexiones (Si falla una conexión crítica, cerramos)
        if (sockets->conexion_kernel_memory < 0 || sockets->conexion_kernel_scheduler < 0 || sockets->memory_sticks == NULL) 
        {
            log_error(logger, "Error al establecer conexiones iniciales."); //Error en valor de los so
            terminar_programa(logger, config, sockets);
            return EXIT_FAILURE;
        }
    }
    else if (sockets->conexion_kernel_scheduler < 0)
    {
        log_error(logger, "Error al establecer conexiones iniciales."); 
        terminar_programa(logger, config, sockets);  
        return EXIT_FAILURE;
    }

    log_info(logger, "Todas las conexiones fueron establecidas con EXITO");


    /*----- SOLICITAMOS UN PROCESO -----*/
    
    control_loop00 = 1;
    
    while(control_loop00 == 1)
    {
        int contexto_key = 0;
        enviar_op_code (CPU_LIBRE, sockets->conexion_kernel_scheduler);

        log_info(logger, "Esperando Por Procesos");

        if((proceso_en_ejecucion->pid = recibir_pid(sockets->conexion_kernel_scheduler)) == -1){

            log_error(logger, "Error en Conexion");
            return;
        }
        
        contexto_key++;

        log_info(logger,"Fue recibido el PID: [%d]",proceso_en_ejecucion->pid);

        control_loop = 1;
        while (control_loop == 1){
            
            log_debug(logger,"Nuevo Ciclo de Instruccion");
            log_debug(logger,"Valor Contexto Key [%d]",contexto_key);
            
            char* instruccion_raw;

                if(contexto_key == 1)
                { 
                    log_info(logger,"Solicitando Contexto del Proceso PID: [%d]", proceso_en_ejecucion->pid);
                    contexto_actual = recibir_contexto(sockets->conexion_kernel_memory);
                    contexto_key--;
                }
                
                instruccion_raw = fetch(sockets); /* Fase Fetch */
            
            if (instruccion_raw == NULL)
            {
                log_info(logger, "Error en Instruccion RAW post FETCH [== NULL]");
                return EXIT_FAILURE;
            }
            
            
            decode(instruccion_raw); /* Fase Decode */

            contexto_actual->pc++; //La sumatoria en 1 del PC se hace en esta parte para evitar errores
            
            if(instruccion_decodificada == NULL) {
                log_error(logger,"No hay instruccion decodificada");
                continue;
            }

            execute(); /* Fase Execute */

            interrupt();/* Fase Interrupt */
            
        }

            liberar_instruccion(instruccion_decodificada);
            instruccion_decodificada = NULL;
            proceso_en_ejecucion->pid = -1;
    }

    terminar_programa (logger, config, sockets); /*Pensar si hay algo mas que se tenga que [cerrar / terminar]*/
}

/* ---------------- FUNCIONES ADMINISTRATIVAS ---------------- */

t_log* iniciar_logger(t_log_level log_level) 
{
    // Usamos LOG_LEVEL_INFO por defecto para asegurar que se vea por consola
    t_log* nuevo_logger = log_create("cpu.log", "CPU", true, log_level);
    if (nuevo_logger == NULL) {
        perror("No se pudo crear el logger");
    }
    return nuevo_logger;
}

int iniciar_log_config(char* archivo_config, char* identificador){    
    sockets = malloc(sizeof(t_cpu_sockets));
    sockets->memory_sticks = list_create();
    proceso_en_ejecucion = malloc(sizeof(t_proceso_ejec));

    config = iniciar_config(archivo_config);

    sockets->ip_kernel_memory = config_get_string_value(config, "IP_KERNEL_MEMORY");
    sockets->puerto_kernel_memory = config_get_string_value(config, "PUERTO_KERNEL_MEMORY");

    sockets->ip_kernel_scheduler = config_get_string_value(config, "IP_KERNEL_SCHEDULER");
    sockets->puerto_kernel_scheduler = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER");
    sockets->puerto_kernel_scheduler_dispatch = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER_DISPATCH");
    sockets->puerto_kernel_scheduler_interrupt = config_get_string_value(config, "PUERTO_KERNEL_SCHEDULER_INTERRUPT");

    sockets->ip_memory_stick = config_get_string_value(config, "IP_MEMORY_STICK");
    sockets->puerto_memory_stick = config_get_string_value(config, "PUERTO_MEMORY_STICK");

    t_log_level log_level = log_level_from_string (config_get_string_value(config, "LOG_LEVEL"));
    logger = iniciar_logger(log_level);

    return 0;
}

void terminar_programa(t_log* logger, t_config* config, t_cpu_sockets* sockets)
{
    if(logger) 
        log_destroy(logger);

    if(config) 
        config_destroy(config);


    // Cerrar Memory Sticks
    if(sockets->memory_sticks != NULL)
    {
        for(int i = 0; i < list_size(sockets->memory_sticks); i++)
        {
            t_mem_stick* ms = list_get(sockets->memory_sticks, i);

            if(ms->socket >= 0)
                liberar_conexion(ms->socket);

            if(ms->ip)
                free(ms->ip);

            if(ms->puerto)
                free(ms->puerto);
            free(ms);
        }
        sem_destroy(&mutex_memory_sticks);

        list_destroy(sockets->memory_sticks);
    }


    // Cerrar Scheduler
    if(sockets->conexion_kernel_scheduler >= 0)
        liberar_conexion(sockets->conexion_kernel_scheduler);


    // Cerrar Kernel Memory si existe
    if(!mock && sockets->conexion_kernel_memory >= 0)
        liberar_conexion(sockets->conexion_kernel_memory);


    free(sockets);
}

/* ---------------- IMPLEMENTACION DE CONEXIONES ---------------- */

int conexion_kernel_memory(t_config* config, t_log* logger, module_name module) {

    log_info(logger, "Iniciando conexion con KERNEL MEMORY");

    return crear_conexion(
        sockets->ip_kernel_memory,
        sockets->puerto_kernel_memory,
        logger,
        module
    );
}

int conexion_kernelS(t_config* config, t_log* logger, module_name module) {

    log_info(logger, "Iniciando conexion con KERNEL SCHEDULER");

    return crear_conexion_reintentando(
        sockets->ip_kernel_scheduler,
        sockets->puerto_kernel_scheduler,
        logger,
        module
    );
}

/* ---------------- IMPLEMENTACION DE CLICO DE INTRUCCION ---------------- */

char* fetch(t_cpu_sockets* sockets) {

    log_info(logger, "## PID:[%d] - FETCH - Program Counter:[%d]", 
             contexto_actual->pid, contexto_actual->pc);
    
    
    int tamanio = 0; /* => Hay que darle un valor a esto anres de mandarlo*/

    uint32_t dir_fisica = pedir_direccion_mmu(contexto_actual->pc, tamanio);
    
    if (dir_fisica == ERROR_MMU) {
        log_error(logger, "Segmentation Fault en PC: %u", contexto_actual->pc);
        return NULL;
    }

    
    enviar_op_code(FETCH, sockets-> conexion_kernel_memory ); //Le informamos al KM que vamos a solicitarle una instruccion

    t_paquete* paquete = crear_paquete(FETCH);
    agregar_a_paquete(paquete, &(contexto_actual->pid), sizeof(int));
    agregar_a_paquete(paquete, &(contexto_actual->pc), sizeof(uint32_t));

    enviar_paquete(paquete, sockets->conexion_kernel_memory); // Enviamos a KM los datos que necesita para identificar la intruccion
    eliminar_paquete(paquete);

    
    char* instruccion_raw = recibir_mensaje(sockets->conexion_kernel_memory, logger); // recibimos el string de la instrucción
    
    if (instruccion_raw == NULL) {
        log_error(logger, "Error en fetch");
        return NULL;
    }
    
    return instruccion_raw;

}

int decode(char* instruccion_raw) {
    
    char** tokens = string_split(instruccion_raw, " ");
    
    if (tokens == NULL) return EXIT_FAILURE;

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
    string_iterate_lines(tokens, (void*)  free);
    free(tokens);
    free(instruccion_raw);
    return EXIT_SUCCESS;
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
            ejecutar_mov_in(instr);      
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

        case EXIT_PROC:
            ejecutar_exit();
            break;

        default:
            log_warning(logger, "Instruccion no definida en execute");
            break;
    }
    
}

void interrupt() { 
    
    if(exit_control == 1) {
        exit_control = 0;
        return;
    }

    enviar_op_code (DESALOJO, sockets->conexion_kernel_scheduler); //Se le consulta al KS si se debe desalojar.
    enviar_pid (contexto_actual->pid,sockets->conexion_kernel_scheduler);
    enviar_mensaje (identificador, sockets->conexion_kernel_scheduler);
    
    int cod_op = recibir_op_code (sockets->conexion_kernel_scheduler);
    if (cod_op == DESALOJO || cod_op == MEM_CORRUPT || cod_op == COMPACTACION) {
        log_info (logger, "## Interrupcion recibida"); /*Logger Obligatorio*/
        gestionar_desalojo_por_syscall(NULL, DESALOJO);
        if(cod_op == MEM_CORRUPT){control_loop00 = 0;}

    }
    else if(cod_op == NUEVA_MEMORY_STICK){
        recibir_memory_stick(sockets->conexion_kernel_scheduler);
    }
    else {
        
        enviar_op_code(OK, sockets->conexion_kernel_scheduler);
    }
}

/* ---------------- FUNCIONES COMPLEMENTARIAS PARA CICLO DE INTRUCCION ---------------- */

t_contexto* recibir_contexto(int socket_km) {
    
    // recibir el paquete (recibo un buffer del socket)
    int buffer_size;

    enviar_op_code(CONTEXTO, socket_km);

    enviar_pid(contexto_actual->pid,socket_km);

    void* buffer = recibir_buffer(&buffer_size, socket_km);

    // crear una estructura para almacenar lo recibido
    t_contexto* nuevo_contexto = malloc(sizeof(t_contexto));

    // Desempaquetar en el MISMO ORDEN
    int offset = 0;

    memcpy(&nuevo_contexto->pid, buffer + offset, sizeof(int));
    offset += sizeof(int);

    memcpy(&nuevo_contexto->pc, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    

    // Registros 8 bits
    memcpy(&nuevo_contexto->ax, buffer + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(&nuevo_contexto->bx, buffer + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(&nuevo_contexto->cx, buffer + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(&nuevo_contexto->dx, buffer + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);

    // Registros 32 bits
    memcpy(&nuevo_contexto->eax, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&nuevo_contexto->ebx, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&nuevo_contexto->ecx, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&nuevo_contexto->edx, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&nuevo_contexto->si, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(&nuevo_contexto->di, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Tabla de Segmentos
    nuevo_contexto->tabla_segmentos = list_create();

    int cantidad;

    memcpy(&cantidad, buffer + offset, sizeof(int));
    offset += sizeof(int);

    for(int i = 0; i < cantidad; i++)
    {
        t_segmento* seg = malloc(sizeof(t_segmento));

        memcpy(&seg->id_segmento, buffer + offset, sizeof(int));
        offset += sizeof(int);

        memcpy(&seg->base, buffer + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        memcpy(&seg->tamanio, buffer + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        list_add(nuevo_contexto->tabla_segmentos, seg);
    }

    free(buffer);
    return nuevo_contexto;
}

void liberar_instruccion(t_instruccion* instruccion) {
    if (instruccion == NULL) return;
    for (int i = 0; i < instruccion->cant_params; i++) {
        free(instruccion->params[i]);
    }
    free(instruccion);
}

void limpiar_contexto_actual() {
    
    log_info(logger, "Limpiando contexto del proceso actual...");

    if (contexto_actual != NULL) {
        free(contexto_actual);
        contexto_actual = NULL;
    }

    log_info(logger, "Contexto limpio. CPU lista para recibir nuevo PID.");
}

void gestionar_desalojo_por_syscall(char* valor, op_code tipo_operacion) {

    if(!mock){enviar_contexto_a_kernel_memory();}
    else {enviar_contexto_a_kernel_memory_mock();} 
    
    enviar_op_code(OK,sockets->conexion_kernel_scheduler);

    control_loop = 0;
    
    return; // Simplemente salimos de la función void sin retornar un valor
}

void enviar_contexto_a_kernel_memory() {

    int err;
    
    enviar_op_code (cpu_GUARDAR_CONTEXTO, sockets->conexion_kernel_memory);

    t_paquete* paquete = crear_paquete(cpu_GUARDAR_CONTEXTO);

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

    int cantidad = list_size(contexto_actual->tabla_segmentos);

    agregar_a_paquete(paquete, &cantidad, sizeof(int));

    for(int i = 0; i < cantidad; i++)
    {
        t_segmento* seg = list_get(contexto_actual->tabla_segmentos, i);

        agregar_a_paquete(paquete, &seg->id_segmento, sizeof(int));
        agregar_a_paquete(paquete, &seg->base, sizeof(uint32_t));
        agregar_a_paquete(paquete, &seg->tamanio, sizeof(uint32_t));
    }

    enviar_paquete(paquete, sockets->conexion_kernel_memory);
    eliminar_paquete(paquete);

    err = recibir_op_code (sockets->conexion_kernel_memory);
    if (err != OK) log_error(logger, "Error en operacion: %d", (int)err);

}

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
    if (strcmp(token, "EXIT_PROC") == 0)          return EXIT_PROC;

    // caso por defecto si no reconoce el comando
    if (token == NULL) return EXIT_FAILURE;
    return NOOP;
}

void* obtener_registro(char* nombre) {
    
    log_debug(logger, "Funcion: [Obtener_Registro]");

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

void ejecutar_noop (t_instruccion* instr){/*OK*/

    log_info (logger, "## PID:[%d] - Ejecutando [NOOP]",contexto_actual->pid);/*Logger Obligatorio*/
    //No hace nada

}

void ejecutar_set (t_instruccion* instr){/*OK*/

    char* reg_dest_nombre = instr->params[0];

    if (es_registro_32bits(reg_dest_nombre)){
        
        uint32_t valor = (uint32_t) strtoul(instr->params[1], NULL, 10);
        uint32_t* dest = (uint32_t*)obtener_registro(reg_dest_nombre);
        
        *dest = valor;

        log_info (logger, "## PID:[%d] - Ejecutando [SET] - Destino [%s] - Valor [%d]",contexto_actual->pid, reg_dest_nombre, *dest);/*Logger Obligatorio*/
        log_info(logger, "[EXEC] SET 32b: %s = %u", reg_dest_nombre, *dest);
    }
    else {
        
        uint8_t valor = (uint8_t) strtoul(instr->params[1], NULL, 10);
        uint8_t* dest = (uint8_t*)obtener_registro(reg_dest_nombre);

        *dest = valor;

        log_info (logger, "## PID:[%d] - Ejecutando [SET] - Destino [%s] - Valor [%d]",contexto_actual->pid, reg_dest_nombre, *dest);/*Logger Obligatorio*/
        log_info(logger, "[EXEC] SET 8b: %s = %u", reg_dest_nombre, *dest);
    }

}

void ejecutar_mov_in (t_instruccion* instr){

    char* reg_dest_nombre = instr->params[0];
    void* buffer;

    if (es_registro_32bits(reg_dest_nombre)){
        
        uint32_t dir_fisica;

        uint32_t* dest = (uint32_t*)obtener_registro(reg_dest_nombre);
        dir_fisica = pedir_direccion_mmu(contexto_actual->si, sizeof(uint32_t));

        if(dir_fisica == ERROR_SEGMENTATION_FAULT)
        {
            return;
        }
        // Comunicación con Memory Stick
        buffer = leer_de_memoria(dir_fisica, sizeof(uint32_t));

        if(buffer == NULL)
        {
            return;
        }
        *dest = *(uint32_t*)buffer;
        free(buffer);

        log_info (logger, "## PID:[%d] - Ejecutando [MOV IN] - Destino [%s] - Valor [%d]",contexto_actual->pid, reg_dest_nombre, *dest);/*Logger Obligatorio*/
        log_info(logger, "[EXEC] MOV_IN 32b: %s = %u", reg_dest_nombre, *dest);
    }   
    else {
        
        uint32_t dir_fisica;

        uint8_t* dest = (uint8_t*)obtener_registro(reg_dest_nombre);
        dir_fisica = pedir_direccion_mmu(contexto_actual->si, sizeof(uint8_t));
        
        if(dir_fisica == ERROR_SEGMENTATION_FAULT)
        {
            return;
        }
    
        // Comunicación con Memory Stick
        buffer = leer_de_memoria(dir_fisica, sizeof(uint8_t));

        if(buffer == NULL)
        {
            return;
        }

        *dest = *(uint8_t*)buffer;
        free(buffer);

        log_info (logger, "## PID:[%d] - Ejecutando [MOV IN] - Destino [%s] - Valor [%d]",contexto_actual->pid, reg_dest_nombre, *dest);/*Logger Obligatorio*/
        log_info(logger, "[EXEC] MOV_IN 32b: %s = %u", reg_dest_nombre, *dest);


    }
}

void ejecutar_mov_out(t_instruccion* instr){

    char* reg_valor_nombre = instr->params[0];
    char* reg_dir_nombre = instr->params[1];

    int tamanio = 0;
    void* buffer;


    uint32_t direccion_logica = obtener_direccion_del_registro(reg_dir_nombre);


    if(es_registro_32bits(reg_valor_nombre)){

        tamanio = sizeof(uint32_t);

        uint32_t* valor = obtener_registro(reg_valor_nombre);

        uint32_t dir_fisica;
        dir_fisica = pedir_direccion_mmu(direccion_logica,tamanio);

        buffer = valor;


       
            escribir_en_memoria(dir_fisica,buffer,tamanio);
        


        log_info(logger,
        "## PID:[%d] - Ejecutando [MOV OUT] - Dirección [%s] - Valor [%u]",
        contexto_actual->pid,
        reg_dir_nombre,
        *valor);

    }
    else{

        tamanio = sizeof(uint8_t);

        uint8_t* valor = obtener_registro(reg_valor_nombre);

        uint32_t dir_fisica;
        dir_fisica = pedir_direccion_mmu(direccion_logica,tamanio);
  
        buffer = valor;

        escribir_en_memoria(dir_fisica,buffer,tamanio);

    }
}

void ejecutar_sum(t_instruccion* instr) {/*OK*/
    
    char* reg_dest_nombre = instr->params[0];
    char* reg_orig_nombre = instr->params[1];

    if (es_registro_32bits(reg_dest_nombre)) {
        
        uint32_t* dest = (uint32_t*)obtener_registro(reg_dest_nombre);
        uint32_t* orig = (uint32_t*)obtener_registro(reg_orig_nombre);
        *dest += *orig;
        
        log_info (logger, "## PID:[%d] - Ejecutando [SUM] - Destino [%s] - Valor [%d]",contexto_actual->pid, reg_dest_nombre, *dest);/*Logger Obligatorio*/
        log_info(logger, "[EXEC] SUM 32b: %s = %u", reg_dest_nombre, *dest);
    } else {
        
        uint8_t* dest = (uint8_t*)obtener_registro(reg_dest_nombre);
        uint8_t* orig = (uint8_t*)obtener_registro(reg_orig_nombre);
        *dest += *orig;
        
        log_info (logger, "## PID:[%d] - Ejecutando [SUM] - Destino [%s] - Valor [%d]",contexto_actual->pid, reg_dest_nombre, *dest);/*Logger Obligatorio*/
        log_info(logger, "[EXEC] SUM 8b: %s = %u", reg_dest_nombre, *dest);
    }
}

void ejecutar_sub(t_instruccion* instr) {/*OK*/
    
    char* reg_dest_nombre = instr->params[0];
    char* reg_orig_nombre = instr->params[1];

    if (es_registro_32bits(reg_dest_nombre)) {
        
        uint32_t* dest = (uint32_t*)obtener_registro(reg_dest_nombre);
        uint32_t* orig = (uint32_t*)obtener_registro(reg_orig_nombre);
        *dest -= *orig;
        
        log_info (logger, "## PID:[%d] - Ejecutando [SUB] - Destino [%s] - Valor [%d]",contexto_actual->pid, reg_dest_nombre, *dest);/*Logger Obligatorio*/
        log_info(logger, "[EXEC] SUB 32b: %s = %u", reg_dest_nombre, *dest);
    } else {
        
        uint8_t* dest = (uint8_t*)obtener_registro(reg_dest_nombre);
        uint8_t* orig = (uint8_t*)obtener_registro(reg_orig_nombre);
        *dest -= *orig;
       
        log_info (logger, "## PID:[%d] - Ejecutando [SUB] - Destino [%s] - Valor [%d]",contexto_actual->pid, reg_dest_nombre, *dest);/*Logger Obligatorio*/
        log_info(logger, "[EXEC] SUB 8b: %s = %u", reg_dest_nombre, *dest);
    }
}

void ejecutar_jnz(t_instruccion* instr) {
    
    void* reg = obtener_registro(instr->params[0]);

    uint32_t valor;

    if(es_registro_32bits(instr->params[0]))
        valor = *(uint32_t*)reg;
    else
        valor = *(uint8_t*)reg;

    if(valor != 0)
    {
        contexto_actual->pc = atoi(instr->params[1]);
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
    uint32_t dir_fisica_origen = pedir_direccion_mmu(*dir_logica_origen, tamanio);
    uint32_t dir_fisica_destino = pedir_direccion_mmu(*dir_logica_destino, tamanio);
    
    // Comunicación con Memory Stick
    void* buffer = leer_de_memoria(dir_fisica_origen, tamanio);
    escribir_en_memoria(dir_fisica_destino, buffer, tamanio);
    
    free(buffer);
    
    log_info (logger, "## PID:[%d] - Ejecutando [COPY MEM] - Tamaño [%d] - Origen [%u] - Destino [%d]",contexto_actual->pid, tamanio,*dir_logica_origen, *dir_logica_destino);/*Logger Obligatorio*/
    log_info(logger, "[EXEC] COPY_MEM: Copiados %d bytes de SI(log:%u) a DI(log:%u)", 
             tamanio, *dir_logica_origen, *dir_logica_destino);
}

void ejecutar_mutex_create(t_instruccion* instr){/*OK*/

    char* mutex_id = instr->params[0];
    op_code err;

    log_info (logger, "## PID:[%d] - Ejecutando [MUTEX CREATE] - Valor [%s]",contexto_actual->pid, mutex_id);/*Logger Obligatorio*/
    
    enviar_op_code (gl_MUTEX_CREATE, sockets->conexion_kernel_scheduler); //Envia la señal
    
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if(err != OK) log_error(logger, "Error en operacion: %d", (int)err);

    
    enviar_pid(contexto_actual->pid,sockets->conexion_kernel_scheduler);
    enviar_mensaje (mutex_id, sockets->conexion_kernel_scheduler); // Se manda el nombre del semaforo

    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK)  log_error(logger, "Error en operacion: %d", (int)err); //MARCAR ERROR

    log_debug (logger, "Fin de Syscall"); 
}

void ejecutar_mutex_lock (t_instruccion* instr){/*OK*/

    char* mutex_id = instr->params[0];
    op_code err;

    log_info (logger, "## PID:[%d] - Ejecutando [MUTEX LOCK] - Valor [%s]",contexto_actual->pid, mutex_id);/*Logger Obligatorio*/

    enviar_op_code (gl_MUTEX_LOCK, sockets->conexion_kernel_scheduler);

    err = recibir_op_code (sockets->conexion_kernel_scheduler); 
    if(err != OK)  log_error(logger, "Error en operacion: %d", (int)err);

    
    enviar_pid(contexto_actual->pid,sockets->conexion_kernel_scheduler);
    enviar_mensaje (mutex_id, sockets->conexion_kernel_scheduler); // Se manda el nombre del semaforo

    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK) log_error(logger, "Error en operacion: %d", (int)err); //MARCAR ERROR

    log_debug (logger, "Fin de Syscall"); 
}

void ejecutar_mutex_unlock (t_instruccion* instr){/*OK*/

    char* mutex_id = instr->params[0];
    op_code err;

    log_info (logger, "## PID:[%d] - Ejecutando [MUTEX Unlock] - Valor [%s]",contexto_actual->pid, mutex_id);/*Logger Obligatorio*/

    enviar_op_code (gl_MUTEX_UNLOCK, sockets->conexion_kernel_scheduler); //Envia la señal

    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if(err != OK) log_error(logger, "Error en operacion: %d", (int)err);//MARCAR ERROR

    enviar_pid(contexto_actual->pid,sockets->conexion_kernel_scheduler);
    enviar_mensaje (mutex_id, sockets->conexion_kernel_scheduler); // Se manda el nombre del semaforo

    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK) log_error(logger, "Error en operacion: %d", (int)err); //MARCAR ERROR

    log_debug (logger, "Fin de Syscall"); 
}

void ejecutar_mem_alloc (t_instruccion* instr){

    char* id_segmento = instr->params[0];
    char* tamanio = instr->params[1];
    op_code err;

    log_info (logger, "## PID:[%d] - Ejecutando [MUTEX CREATE] - ID [%s] - tamanio [%s]",contexto_actual->pid, id_segmento, tamanio);/*Logger Obligatorio*/
    
    enviar_op_code (gl_MEM_ALLOC, sockets->conexion_kernel_scheduler); //Envia la señal
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if(err != OK)  log_error(logger, "Error en operacion: %d", (int)err); //MARCAR ERROR

    enviar_mensaje (id_segmento, sockets->conexion_kernel_scheduler); 
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK)  log_error(logger, "Error en operacion: %d", (int)err); //MARCAR ERROR


    enviar_mensaje (tamanio, sockets->conexion_kernel_scheduler); 
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK) log_error(logger, "Error en operacion: %d", (int)err); //MARCAR ERROR

    enviar_pid (contexto_actual->pid, sockets->conexion_kernel_scheduler); 
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK) log_error(logger, "Error en operacion: %d", (int)err); //MARCAR ERROR

    int base = recibir_pid(sockets->conexion_kernel_scheduler); /*Uso esta aunque no sea para esto*/

    crear_segmento(
        atoi(id_segmento),
        atoi(tamanio),
        base
    );

    log_info (logger, "ok"); //Completar LOG
            
}

void ejecutar_mem_free (t_instruccion* instr){

    char* id_segmento = instr->params[0];
    op_code err;

    log_info (logger, "## PID:[%d] - Ejecutando [MEM ALLOC] - ID [%s]",contexto_actual->pid, id_segmento);/*Logger Obligatorio*/

    enviar_op_code (gl_MEM_FREE, sockets->conexion_kernel_scheduler); //Envia la señal
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if(err != OK)  log_error(logger, "Error en operacion: %d", (int)err);

    enviar_mensaje (id_segmento, sockets->conexion_kernel_scheduler); 
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK)  log_error(logger, "Error en operacion: %d", (int)err);

    enviar_pid (contexto_actual->pid, sockets->conexion_kernel_scheduler); 
    err = recibir_op_code (sockets->conexion_kernel_scheduler); // Espera Respuesta de OK
    if (err != OK)  log_error(logger, "Error en operacion: %d", (int)err);

    eliminar_segmento(atoi(id_segmento));
}

void ejecutar_sleep(t_instruccion* instr) {
    
    char* tiempo = strdup(instr->params[0]);
    
    log_info (logger, "## PID:[%d] - Ejecutando [Sleep] - Tiempo [%s]",contexto_actual->pid, tiempo);/*Logger Obligatorio*/  

    enviar_op_code(gl_IO_SLEEP,sockets->conexion_kernel_scheduler);

    enviar_pid(contexto_actual->pid,sockets->conexion_kernel_scheduler);

    enviar_mensaje(tiempo,sockets->conexion_kernel_scheduler);

    



    if (recibir_op_code(sockets->conexion_kernel_scheduler) != OK) {
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

    log_info (logger, "## PID:[%d] - Ejecutando [STDOUT] -  Registro [%p] - tamanio [%p]",contexto_actual->pid, ptr_dir, ptr_tam);/*Logger Obligatorio*/
    log_info(logger, "PID: %u", pid_actual);

    enviar_op_code(gl_IO_STDOUT,sockets->conexion_kernel_scheduler);

    enviar_int((int)tamanio,sockets->conexion_kernel_scheduler);

    enviar_int((int)direccion_logica,sockets->conexion_kernel_scheduler);

    enviar_pid(contexto_actual->pid,sockets->conexion_kernel_scheduler);

    int err = recibir_op_code(sockets->conexion_kernel_scheduler);
    if(err != OK)
    {
        log_error(logger,"Error en Comunicacion FUNCION;[ejecutar_stdoput]");
        return;
    }   
}

void ejecutar_stdin(t_instruccion* instr) {

    log_debug(logger, "STDIN");
    
    char* reg_dir = instr->params[0];
    char* reg_tam = instr->params[1];

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

    log_debug(logger,"Obteniendo direccion y tamaño");
    
    tamanio = obtener_tamanio_del_registro(reg_tam);
    direccion_logica = obtener_direccion_del_registro(instr->params[1]);
    uint32_t pid_actual = proceso_en_ejecucion->pid; 

    log_info (logger, "## PID:[%d] - Ejecutando [STDIN] - Destino [%d] - tamanio [%d]",contexto_actual->pid, direccion_logica, tamanio);/*Logger Obligatorio*/

    enviar_op_code(gl_IO_STDIN,sockets->conexion_kernel_scheduler);


    enviar_int((int)tamanio,sockets->conexion_kernel_scheduler);

    enviar_int((int)direccion_logica,sockets->conexion_kernel_scheduler);

    enviar_pid(contexto_actual->pid,sockets->conexion_kernel_scheduler);
    

    if (recibir_op_code(sockets->conexion_kernel_scheduler) == OK) {
        log_info(logger, "Proceso creado exitosamente por el Kernel.");
    }

}

void ejecutar_init_proc(t_instruccion* instr) {
   
    char* path = instr->params[0];
    int prioridad = atoi(instr->params[1]);

    log_info (logger, "## PID:[%d] - Ejecutando [INIT PROC] - PATH [%s] - Prioridad [%d]",contexto_actual->pid, path, prioridad);/*Logger Obligatorio*/
    log_info(logger, "PID %d solicitando crear nuevo proceso: %s", contexto_actual->pid, path);

    enviar_op_code(gl_INIT_PROC, sockets->conexion_kernel_scheduler);

    t_paquete* paquete = crear_paquete(gl_INIT_PROC);
    agregar_a_paquete(paquete, path, strlen(path) + 1);
    agregar_a_paquete(paquete, &prioridad, sizeof(int));
    enviar_paquete(paquete, sockets->conexion_kernel_scheduler);
    eliminar_paquete(paquete);

    enviar_pid(contexto_actual->pid,sockets->conexion_kernel_scheduler);

    if (recibir_op_code(sockets->conexion_kernel_scheduler) == OK) {
        log_info(logger, "Proceso creado exitosamente por el Kernel.");
    }
}

void ejecutar_exit() {
    
    log_info (logger, "## PID:[%d] - Ejecutando [EXIT]",contexto_actual->pid);/*Logger Obligatorio*/

    enviar_op_code(gl_EXIT, sockets->conexion_kernel_scheduler);

    enviar_pid(contexto_actual->pid,sockets->conexion_kernel_scheduler);

    if (recibir_op_code(sockets->conexion_kernel_scheduler) == OK) {
        log_info(logger, "EXIT confirmado. Limpiando CPU.");
        exit_control = 1;
        limpiar_contexto_actual();
        control_loop = 0; //hace que se vuelva a mandar CPU_LIBRE
    }
}

/* ------------------ FUNCIONES AUXILIARES DE EXECUTE ------------------*/

void crear_segmento(int id, int tamanio, int base){
    t_segmento* nuevo_segmento = malloc(sizeof(t_segmento));

    nuevo_segmento->id_segmento = id;
    nuevo_segmento->tamanio = tamanio;
    nuevo_segmento->base = base;

    list_add(contexto_actual->tabla_segmentos, nuevo_segmento);
}

void eliminar_segmento(int id) {

    int id_buscado = id;

    t_segmento* segmento_a_eliminar = list_remove_by_condition(
        contexto_actual->tabla_segmentos,
        tiene_mismo_id
    );

    if(segmento_a_eliminar == NULL)
        return;

    free(segmento_a_eliminar);
}

/* ------------------ MMU ------------------*/

t_mem_stick* buscar_memory_stick(uint32_t direccion_fisica) {
    
    log_info(logger, "Buscando MS para direccion %u", direccion_fisica);
    log_info(logger, "Cantidad de Memory Sticks: %d",
         list_size(sockets->memory_sticks));

    for(int i = 0; i < list_size(sockets->memory_sticks); i++)
    {
        t_mem_stick* ms = list_get(sockets->memory_sticks, i);
        
        log_info(logger,
             "MS %d -> base=%u tam=%u rango=[%u,%u)",
             i,
             ms->base,
             ms->tamanio,
             ms->base,
             ms->base + ms->tamanio);

        if(direccion_fisica >= ms->base &&
           direccion_fisica < (ms->base + ms->tamanio))
        {
            return ms;
        }
    }

    return NULL;
}

uint32_t pedir_direccion_mmu(uint32_t dir_logica, uint32_t tamanio_solicitado)
{
    uint32_t id_segmento = dir_logica / config_cpu->tam_max_segmento;
    uint32_t desplazamiento = dir_logica % config_cpu->tam_max_segmento;

    int id_buscado = id_segmento;

    log_info(logger, "Cantidad de segmentos: %d",
         list_size(contexto_actual->tabla_segmentos));

    for(int i = 0; i < list_size(contexto_actual->tabla_segmentos); i++)
    {
        t_segmento* s = list_get(contexto_actual->tabla_segmentos, i);

        log_info(logger,
                "Segmento %d -> id=%d base=%u tam=%u",
                i,
                s->id_segmento,
                s->base,
                s->tamanio);
    }

    t_segmento* segmento = list_find(
        contexto_actual->tabla_segmentos,
        tiene_mismo_id
    );

    if(segmento == NULL)
    {
        log_error(logger,
                  "SEG_FAULT: Segmento %u inexistente",
                  id_segmento);

        return ERROR_SEGMENTATION_FAULT;
    }

    if(desplazamiento + tamanio_solicitado > segmento->tamanio)
    {
        log_error(logger,
                  "SEG_FAULT: Acceso fuera de límites. PID: %u",
                  contexto_actual->pid);

        enviar_op_code(ERROR_SEGMENTATION_FAULT,
                       sockets->conexion_kernel_scheduler);   // o el socket que corresponda

        return ERROR_SEGMENTATION_FAULT;
    }

    return segmento->base + desplazamiento;
}


uint32_t obtener_direccion_del_registro(char* reg) {
    uint32_t* ptr = (uint32_t*)obtener_registro(reg);
    return (ptr != NULL) ? *ptr : 0;
}

uint32_t obtener_tamanio_del_registro(char* reg)
{
    void* registro = obtener_registro(reg);

    if(registro == NULL)
    {
        log_error(logger, "Registro inexistente: %s", reg);
        return 0;
    }

    if(es_registro_32bits(reg))
    {
        return *(uint32_t*)registro;
    }
    else
    {
        return (uint32_t)(*(uint8_t*)registro);
    }
}

void* leer_de_memoria(uint32_t dir_fisica, int tamanio)
{
    void* buffer_total = malloc(tamanio);

    int bytes_restantes = tamanio;
    uint32_t direccion_actual = dir_fisica;
    int offset_buffer = 0;

    while(bytes_restantes > 0)
    {
        t_mem_stick* ms = buscar_memory_stick(direccion_actual);

        if(ms == NULL){
            log_error(logger, 
                "No existe Memory Stick para la direccion %u",
                direccion_actual
            );

            free(buffer_total);
            return NULL;
        }

        uint32_t dir_local = direccion_actual - ms->base;

        int espacio_disponible = ms->tamanio - dir_local;

        int bytes_a_leer =
            (bytes_restantes < espacio_disponible)
            ? bytes_restantes
            : espacio_disponible;


        // Avisamos la operación
        enviar_op_code(
            LEER_MEMORIA,
            ms->socket
        );


        // Enviamos dirección local
        send(
            ms->socket,
            &dir_local,
            sizeof(uint32_t),
            0
        );


        // Enviamos cantidad de bytes
        send(
            ms->socket,
            &bytes_a_leer,
            sizeof(int),
            0
        );


        // Recibimos los bytes directamente
        void* parte = malloc(bytes_a_leer);

        int recibidos = recv(
            ms->socket,
            parte,
            bytes_a_leer,
            MSG_WAITALL
        );


        if(recibidos != bytes_a_leer)
        {
            log_error(logger,
                "Error recibiendo datos del Memory Stick"
            );

            free(parte);
            free(buffer_total);
            return NULL;
        }


        memcpy(
            (char*)buffer_total + offset_buffer,
            parte,
            bytes_a_leer
        );


        free(parte);


        direccion_actual += bytes_a_leer;
        offset_buffer += bytes_a_leer;
        bytes_restantes -= bytes_a_leer;
    }


    log_info(logger,
        "## PID:[%d] - Accion: [Leer] - Direccion Fisica [%d]",
        contexto_actual->pid,
        dir_fisica
    );


    return buffer_total;
}


void escribir_en_memoria(uint32_t dir_fisica, void* buffer, int tamanio)
{
    log_info(logger,
        "ENTRO escribir_en_memoria: direccion=%u tamanio=%d",
        dir_fisica,
        tamanio
    );

    int bytes_restantes = tamanio;
    uint32_t direccion_actual = dir_fisica;
    int offset_buffer = 0;

    while(bytes_restantes > 0)
    {
        t_mem_stick* ms = buscar_memory_stick(direccion_actual);

        if(ms == NULL){
            log_error(logger,
                "No existe Memory Stick para la direccion %u",
                direccion_actual
            );
            return;
        }


        uint32_t dir_local = direccion_actual - ms->base;

        int espacio_disponible = ms->tamanio - dir_local;

        int bytes_a_escribir =
            (bytes_restantes < espacio_disponible)
            ? bytes_restantes
            : espacio_disponible;


        // Avisamos operación
        enviar_op_code(
            ESCRIBIR_MEMORIA,
            ms->socket
        );


        // Mandamos dirección local
        send(
            ms->socket,
            &dir_local,
            sizeof(uint32_t),
            0
        );


        // Mandamos cantidad de bytes
        send(
            ms->socket,
            &bytes_a_escribir,
            sizeof(uint32_t),
            0
        );


        // Mandamos los datos
        send(
            ms->socket,
            (char*)buffer + offset_buffer,
            bytes_a_escribir,
            0
        );


        // Esperamos confirmación
        int resultado = recibir_op_code(ms->socket);


        if(resultado != OK_ESCRITURA)
        {
            log_error(logger,
                "Error escribiendo en Memory Stick"
            );
            return;
        }


        direccion_actual += bytes_a_escribir;
        offset_buffer += bytes_a_escribir;
        bytes_restantes -= bytes_a_escribir;
    }


    log_info(logger,
        "## PID:[%d] - Accion: [Escribir] - Direccion Fisica [%d]",
        contexto_actual->pid,
        dir_fisica
    );
}

/* ------------------ MANEJO DE MEMORY STICK  ------------------*/

void recibir_memory_stick(int socket_ks)
{
    int size;
    void* buffer = recibir_buffer(&size,socket_ks);


    // liberar al KS primero
    enviar_op_code(OK,socket_ks);


    t_mem_stick* ms = malloc(sizeof(t_mem_stick));

    int offset=0;

    char* stream = buffer;

    ms->ip = strdup(stream + offset);
    offset += strlen(ms->ip)+1;


    ms->puerto = strdup(stream + offset);
    offset += strlen(ms->puerto)+1;


    memcpy(&ms->base,buffer+offset,sizeof(uint32_t));
    offset+=sizeof(uint32_t);


    memcpy(&ms->tamanio,buffer+offset,sizeof(uint32_t));
    offset += sizeof(uint32_t);

    ms->socket = crear_conexion_reintentando(
        ms->ip,
        ms->puerto,
        logger,
        MEMORY_STICK
    );


    if(ms->socket <0)
    {
       log_error(logger,"No conecta Memory Stick");

        free(ms->ip);
        free(ms->puerto);
        free(ms);
        free(buffer);

        return;
    }


    enviar_op_code(NUEVA_CPU,ms->socket);

    if(recibir_op_code(ms->socket)!=OK)
    {
        log_error(logger,"Handshake MS falló");
        return;
    }


   sem_wait(&mutex_memory_sticks);

    list_add_sorted(
        sockets->memory_sticks,
        ms,
        comparar_base_memory_stick
    );

    sem_post(&mutex_memory_sticks);
    free(buffer);
}

bool comparar_base_memory_stick(void* a, void* b)
{
    t_mem_stick* ms1 = a;
    t_mem_stick* ms2 = b;

    return ms1->base < ms2->base;
}

bool rango_ocupado(t_mem_stick* nuevo)
{
    for(int i=0;i<list_size(sockets->memory_sticks);i++)
    {
        t_mem_stick* actual = list_get(sockets->memory_sticks,i);

        uint32_t fin_actual = actual->base + actual->tamanio;
        uint32_t fin_nuevo = nuevo->base + nuevo->tamanio;


        if(nuevo->base < fin_actual &&
           fin_nuevo > actual->base)
        {
            return true;
        }
    }

    return false;
}

/* ------------------ AUXILIARES  ------------------*/

static int id_buscado = 0;

bool tiene_mismo_id(void* elemento) {
    t_segmento* segmento = (t_segmento*) elemento;

    return (id_buscado == segmento->id_segmento);
}

/*---- Fucnones MOCKS ----*/

char* instruccion[] = {

    "SET AX 123",
    "SET SI 0",
    "MOV_OUT AX SI",
    "MOV_IN BX SI",
    "EXIT_PROC"

};

t_contexto* recibir_contexto_mock () { /*Modiicar estos valores si se quiere cambiar algo del contexto inicial*/

    // crear una estructura para almacenar lo recibido
    t_contexto* nuevo_contexto = malloc(sizeof(t_contexto));

    nuevo_contexto->ax = 0;
    nuevo_contexto->bx = 0;
    nuevo_contexto->cx = 0;
    nuevo_contexto->di = 0;
    nuevo_contexto->dx = 0;
    nuevo_contexto->eax = 0;
    nuevo_contexto->ebx = 0;
    nuevo_contexto->ecx = 0;
    nuevo_contexto->edx = 0;
    nuevo_contexto->pc = 0;
    nuevo_contexto->pid = 0;
    nuevo_contexto->si = 0;
    
    nuevo_contexto->tabla_segmentos = list_create();

    t_segmento* seg = malloc(sizeof(t_segmento));

    seg->id_segmento = 0;
    seg->base = 42;
    seg->tamanio = 16;

    list_add(nuevo_contexto->tabla_segmentos, seg);

    log_debug(logger, "Conetexto MOCK Realizado");

    return nuevo_contexto;

}

char* fetch_mock(t_cpu_sockets* sockets){
 
    int tamanio = 0;

    log_info(logger, "[FETCH] Solicitando instruccion para PID: %d, PC: %u [MOCK]", 
             contexto_actual->pid, 
             contexto_actual->pc);


    
    static int nro_instr = 0;

    char* instruccion_raw = instruccion[nro_instr];

    nro_instr++;
    
    if (instruccion_raw == NULL) {
        log_error(logger, "Error en fetch");
        return NULL;
    }
    
    return strdup(instruccion_raw);

}

void enviar_contexto_a_kernel_memory_mock(){
    log_info(logger, "Contexto Enviado a Kernel Memory [MOCK]");
}

void* leer_de_memoria_mock(uint32_t dir_fisica, int tamanio) {/*Siempre que se quiera leer devuelve 33*/

    uint32_t* valor = malloc(sizeof(uint32_t));

    *valor = 33;

    log_info(logger, "Se leyo en memoria [%u] [MOCK]", dir_fisica);

    return valor;

}

void escribir_en_memoria_mock(uint32_t dir_fisica, void* buffer, int tamanio) {
    log_info(logger,"Se escribio en memoria dir.F [%d] [MOCK]",dir_fisica);
    return;}

uint32_t pedir_direccion_mmu_mock(uint32_t dir_logica, int tamanio_solicitado) { /*Siempre devuelve dir_log * 100*/
    log_info(logger,"Se solicito direccion a MMU dir_l [%d] [MOCK]",dir_logica);

    uint32_t dir_f = dir_logica * 100;
    return dir_f;}



