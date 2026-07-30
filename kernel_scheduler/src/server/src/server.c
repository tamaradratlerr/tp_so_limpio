
#include "server.h"
#include "utilsKS.h"
#include "../../utils/src/global_utils.h"

t_list* list_ms_pendientes = NULL;
pthread_mutex_t mutex_ms_pendientes = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) /*OK*/
{
    signal(SIGPIPE, SIG_IGN);

    printf("=====     Iniciando Kernel Scheduler     =====\n");

    if(argc != 3){
        printf("Uso: ./bin/kernel_scheduler [archivo.config] [archivoProcesos]\n");
        return 1;
    }

    printf("=====     Iniciando Cliente     =====\n");

    int err = cliente_Kernel_Scheduler(argc, argv);
    if(err != 0){log_error(logger, "Error al inciar cliente");}
    

   int server_fd = iniciar_servidor(config_get_string_value(config, "PUERTO_ESCUCHA"),logger);

    pthread_t hilo_reintento;
    pthread_create(&hilo_reintento, NULL, hilo_reintentar_desuspension, NULL);
    pthread_detach(hilo_reintento);

    while (scheduler_control_loop == 1) {
        
        int cliente_fd = esperar_cliente(server_fd, logger);
        
        if (cliente_fd == -1) {
            log_error(logger, "Error al aceptar un cliente");
            return -1; 
        }

        log_info(logger, "Iniciando Hilo para Nuevo CLiente");


        /* -------------CREACIÓN DEL HILO---------- */

        pthread_t hilo_id;
        
        
        if (pthread_create(&hilo_id, NULL, atender_nuevo_cliente, (void*)(intptr_t)cliente_fd) != 0) {
            log_error(logger, "Error al crear el hilo");
            close(cliente_fd);
        }

        
    }
    

    terminar_programa (logger, config, info_km);
}




/*----------------------------------FUNCIONES------------------------------------------*/

/*-----                     GESTION DE NUEVOS CLIENTES                     -----*/

void* atender_nuevo_cliente(void* fd) { /*OK*/

    int cliente_fd = (int)(intptr_t)fd; // Recuperamos el FD del cliente
    
    pthread_detach(pthread_self()); //Esto hace que el SO limpie la memoria de este hilo cuando termine la funcion

    int control_loop = 1;
    while (control_loop) { //Este loop funciona de manera tal de que se mantiene CONSTANTEMENTE la comunicacion con el CLIENTE.
        
        log_info(logger,"[***ESPERA DE SOLICITUDES***]");

        int opcode = recibir_op_code(cliente_fd); //syscall bloqueante --> por lo que no se esta haciendo espera activa; es como que el sistema se duerme hasta que reciva 
        log_info(logger,"Fue Recibivo el %s", opcode_to_string(opcode));
        if(opcode == -1){
            log_warning(logger, "El cliente en el socket [%d] se desconectó.", cliente_fd);
            int err = rev_desconexion(cliente_fd);

        
            control_loop = 0;
            break;
        }

        

        switch (opcode) {
            
            case NUEVA_CPU:
                nueva_cpu(cliente_fd);
                break;

            case CPU_LIBRE:
                cpu_libre(cliente_fd);
                break;

            case NUEVA_MEMORY_STICK:
                recibir_nueva_memory_stick(cliente_fd);
                break;

            case NUEVA_IO:
                // Se conecta una interfaz de IO al Kernel por primera vez
                nueva_io(cliente_fd); 
                break;
            case DESALOJO: /*Consulta de la CPU por si debe desalojar*/
                desalojo(cliente_fd);
                break;

            case gl_MEM_ALLOC:
                mem_alloc(cliente_fd);
                break;

            case gl_MEM_FREE:
                mem_free(cliente_fd);
                break;

            case gl_IO_SLEEP:
                io_sleep(cliente_fd); 
                break;

            case gl_IO_STDIN: 
                io_stdin(cliente_fd); 
                break;

            case gl_IO_STDOUT:
                io_stdout(cliente_fd); 
                break;
    
            case IO_LIBRE: 
                io_libre(cliente_fd); 
                break;

            case IO_FINALIZADA:
                io_finalizada(cliente_fd);
                break;

            case gl_MUTEX_CREATE:
                mutex_create(cliente_fd);
                break;
                
            case gl_MUTEX_LOCK:
                mutex_lock(cliente_fd);
                break;

            case gl_MUTEX_UNLOCK:
                mutex_unlock(cliente_fd);
                break;
           
            case gl_INIT_PROC:
                init_proc(cliente_fd);
                break;
                        
            case gl_EXIT:
                exit_proceso(cliente_fd);
                break;
            
            case IO_SLEEP:
                rta_io_sleep(cliente_fd);
                break;

            case IO_STDIN:
                rta_io_stdin(cliente_fd);
                break;

            case IO_STDOUT:
                rta_io_stdout(cliente_fd);
                break;

            case ERROR_SEGMENTATION_FAULT:
                
                segmentation_fault(cliente_fd);
              
                break;

            default:
                log_warning(logger, "Operación desconocida.");
                control_loop = 0; // Si hay basura, mejor cortar
            break;
        }

        pthread_mutex_lock(&mutex_conexion_km);
        enviar_op_code(MEM_CORRUPT, info_km.conexion_km);
        int err = recibir_op_code(info_km.conexion_km);
        pthread_mutex_unlock(&mutex_conexion_km);

        if (err == MEM_CORRUPT)
        {
            mem_corrupt(cliente_fd);
        }

    }

    close(cliente_fd);
    return NULL;
}

/*-----                     GESTION DE PCBs                     -----*/



void segmentation_fault(int socket_cpu){

    int pid = recibir_pid(socket_cpu);

    log_error(logger, "## PID: %d - Finaliza por SEGMENTATION FAULT", pid);

    PCB* pcb = buscar_pcb_por_pid(pid);

    if (pcb == NULL) {
        log_error(logger, "PCB = NULL en SEGMENTATION_FAULT");
        return;                                   // antes seguía y hacía pcb->data.PID
    }

    cambiar_estado_pcb(pcb, EXT);
    eliminar_proceso_Lista(pcb);
    agregar_proceso_lista(pcb);
    

    // ELIMINADO: list_add(list_suplementarias->desalojo, pcb);

    enviar_proceso_finalizar_KM(pcb->data.PID);

    pthread_mutex_lock(&mutex_cpus);
    t_CPU *cpu_libre = list_find_with_context(list_suplementarias->cpu, es_la_cpu_buscada, &socket_cpu);
    if (cpu_libre == NULL) {
        pthread_mutex_unlock(&mutex_cpus);
        log_error(logger, "Error al encontrar CPU en la lista");
        return;
    }

    cpu_libre->enUso = false;
    pthread_mutex_unlock(&mutex_cpus);

    log_info(logger, "## PID:[%d] Finalizo su ejecucion con motivo de [SEG_FAULT]", pcb->data.PID);
    nuevo_espacio();
}



PCB* buscar_pcb_por_pid(int pid_recibido)
{
    log_info(logger, "===== Buscando PID %d =====", pid_recibido);

    t_list* listas_a_revisar[] = {
        listasProcesos->new,
        listasProcesos->rdy,
        listasProcesos->rnn,
        listasProcesos->bck,
        listasProcesos->ext,
        listasProcesos->s_bck,
        listasProcesos->s_rdy
    };

    // FIX A: un mutex por lista. Se toma SOLO mientras se itera esa lista,
    // para que otro hilo (mediano_plazo_bck, rta_io_*, etc.) no haga
    // list_remove y deje el iterador colgado -> segfault.
    pthread_mutex_t* mutexes[] = {
        &sem_procesos_new,
        &sem_procesos_ready,
        &sem_procesos_running,
        &sem_procesos_block,
        &sem_procesos_exit,
        &sem_procesos_s_block,
        &sem_procesos_s_ready
    };

    char* nombres[] = {
        "NEW", "READY", "RUNNING", "BLOCK", "EXIT", "S_BLOCK", "S_READY"
    };

    for (int i = 0; i < 7; i++) {

        pthread_mutex_lock(mutexes[i]);

        t_list_iterator* it = list_iterator_create(listas_a_revisar[i]);

        while (list_iterator_has_next(it)) {

            PCB* pcb = list_iterator_next(it);

            if (pcb->data.PID == pid_recibido) {
                list_iterator_destroy(it);
                pthread_mutex_unlock(mutexes[i]);
                log_info(logger, "PID %d encontrado en %s", pid_recibido, nombres[i]);
                return pcb;
            }
        }

        list_iterator_destroy(it);
        pthread_mutex_unlock(mutexes[i]);
    }

    // CMN: los READY viven en las colas por nivel del planificador (ya lockeaba mutex_ready)
    if (strcmp(info_config.planificacion_algoritmo, "CMN") == 0) {

        pthread_mutex_lock(&mutex_ready);

        for (int n = 0; n < planificador->cantidad_niveles; n++) {
            for (int j = 0; j < list_size(planificador->niveles[n].cola); j++) {
                PCB* pcb = list_get(planificador->niveles[n].cola, j);
                if (pcb->data.PID == pid_recibido) {
                    pthread_mutex_unlock(&mutex_ready);
                    log_info(logger, "PID %d encontrado en cola CMN nivel %d", pid_recibido, n);
                    return pcb;
                }
            }
        }

        pthread_mutex_unlock(&mutex_ready);
    }

    log_error(logger, "PID %d NO encontrado en ninguna lista", pid_recibido);
    return NULL;
}

PCB* encontrar_pcb_rnn_por_pid(int pid) /*OK*/
{
    pthread_mutex_lock(&sem_procesos_exit); 

    PCB* pcb_buscado = NULL;
    
    for (int i = 0; i < list_size(listasProcesos -> rnn); i++) {
        PCB* pcb_actual = list_get(listasProcesos -> rnn, i);
        if (pcb_actual->data.PID == pid) {
            pcb_buscado = pcb_actual;
            break;
        }
    }
    
    pthread_mutex_unlock(&sem_procesos_exit);
    return pcb_buscado;
}


/*-----                     GESTION DE CPUs                     -----*/

void  mandar_proceso_cpu(int socket_cliente)/*OK*/
{ 
    
    log_opcode(logger, CPU_LIBRE);

    /* ===== RETORNO A LA MISMA CPU (Enunciado pag. 11) =====
       Si esta CPU tiene un proceso reservado por MEM_ALLOC / MEM_FREE / MUTEX_CREATE /
       MUTEX_UNLOCK, se le devuelve ESE proceso y no se planifica nada de READY. */
    if (compactacion_value == 0)
    {
        int pid_reservado = tomar_retorno_cpu(socket_cliente);

        if (pid_reservado != -1)
        {
            PCB* pcb_reservado = buscar_pcb_por_pid(pid_reservado);

            pthread_mutex_lock(&mutex_cpus);
            t_CPU* cpu_duenia = list_find_with_context(list_suplementarias->cpu,
                                                       es_la_cpu_buscada, &socket_cliente);
            pthread_mutex_unlock(&mutex_cpus);

            if (pcb_reservado != NULL && cpu_duenia != NULL && pcb_reservado->estado_pcb == RNN)
            {
                cpu_duenia->enUso = true;             /* nunca se libero, se reafirma */
                cpu_duenia->pid_ejecutando = pid_reservado;

                if (enviar_pid(pid_reservado, cpu_duenia->fd) == 1)
                {
                    /* No se reinicia el quantum: es la misma rafaga de ejecucion,
                       solo hubo un viaje de ida y vuelta al Kernel Scheduler. */
                    log_info(logger,
                             "## PID:[%d] vuelve a la CPU ID:[%s], la que realizo la Syscall",
                             pid_reservado, cpu_duenia->identificador);
                    return;
                }

                log_error(logger, "Fallo el retorno del PID:[%d] a su CPU. Se replanifica por READY",
                          pid_reservado);

                cpu_duenia->enUso = false;
                cambiar_estado_pcb(pcb_reservado, RDY);
                eliminar_proceso_Lista(pcb_reservado);
                agregar_proceso_lista(pcb_reservado);
                return;
            }

            log_warning(logger,
                        "PID:[%d] tenia reserva de CPU pero ya no esta en EXEC. Se descarta la reserva",
                        pid_reservado);
        }
    }

    sem_wait(&sem_hay_ready); // Verifica que no se entre si la lista esta vacia
    pthread_mutex_lock(&mutex_cpus);

    
        /* Buscamos la CPU pasándole la dirección del socket_cliente como contexto */
    t_CPU *cpu_libre = list_find_with_context(list_suplementarias->cpu, es_la_cpu_buscada, &socket_cliente);
    
    if (compactacion_value)
    {   
        pthread_mutex_unlock(&mutex_cpus);
        sem_post(&sem_hay_ready);   // devolvemos el token: el proceso sigue en READY

        log_info(logger, "No se despacha: Memory esta compactando");
    return;
    }

    if (cpu_libre != NULL && cpu_libre->enUso == false) 
    {
        
        cpu_libre->enUso = true;
    }
    else 
    {
        log_warning(logger, "No se encontro a la CPU Buscada. Posible Desconexion");
        pthread_mutex_unlock(&mutex_cpus);
        sem_post(&sem_hay_ready);
        return;
    }
       
        pthread_mutex_unlock(&mutex_cpus);

    /*Mandamos el PCB a la CPU*/
    if ((cpu_libre != NULL)) { /*Verifica que exista la CPU libre; Verifica que Haya algun procesos en READY; Verifica que Haya alguna IO*/
        
        
        
        PCB* pcb_a_ejecutar = obtener_siguiente_proceso();
        
        if(mock && pcb_a_ejecutar)
        {
            log_info(logger,
                "[MOCK] Scheduler selecciono PID %d",
                pcb_a_ejecutar->data.PID);
        }

        if(pcb_a_ejecutar == NULL){
            log_error(logger, "No se pudo obtener PCB READY");
            cpu_libre->enUso = false;
            return;
        }

        cambiar_estado_pcb(pcb_a_ejecutar, RNN);
        pcb_a_ejecutar->fd_cpu = cpu_libre->fd;
        agregar_proceso_lista(pcb_a_ejecutar); 
        
        loguear_lista(listasProcesos->rnn,logger);
        

        int err = enviar_pid (pcb_a_ejecutar->data.PID, cpu_libre->fd); //Envia el PID a la CPU
        log_info(logger, "Kernel Shceduler envio PID[%d] a CPU ID: [%s] a Ejecutarse",pcb_a_ejecutar->data.PID,cpu_libre->identificador);

        if (err != 1) 
        {
            log_error (logger, "Error en conexion con la CPU (funcion: mandar_proceso_cpu)"); // Completar log de error
            cpu_libre->enUso = false; 
            
            
            cambiar_estado_pcb(pcb_a_ejecutar, RDY);
            pcb_a_ejecutar->fd_cpu = 0;
            eliminar_proceso_Lista(pcb_a_ejecutar);
            agregar_proceso_lista(pcb_a_ejecutar); 
            
            return;
        }

        cpu_libre->pid_ejecutando = pcb_a_ejecutar->data.PID;

        if (usa_quantum(pcb_a_ejecutar))
        {
            t_datos_quantum* datos = malloc(sizeof(t_datos_quantum));

            // Invalida cualquier timer de quantum anterior de este PCB:
            // a partir de acá, solo el timer con ESTA version podra desalojar.
            pcb_a_ejecutar->quantum_version++;

            datos->pcb     = pcb_a_ejecutar;
            datos->version = pcb_a_ejecutar->quantum_version;

            pthread_create(
                &hilo_timer,
                NULL,
                control_hilo_quantum,
                datos);

            pthread_detach(hilo_timer);
        }
    }
    else {

        cpu_libre->enUso = false;

        log_error( logger, "No hay procesos READY para ejecutar");

        return;
    }
}

bool es_la_cpu_buscada (void* elemento, void* contexto)/*OK*/
{
    
        t_CPU* cpu = (t_CPU*) elemento;
    
        // Casteamos el contexto de vuelta a un puntero de int para sacar el socket
     int socket_buscado = *(int*) contexto; 
    
    return (cpu->fd == socket_buscado);
}








bool es_el_retorno_de_la_cpu (void* elemento, void* contexto)
{
    t_retorno_cpu* retorno = (t_retorno_cpu*) elemento;
    int socket_buscado = *(int*) contexto;

    return (retorno->fd_cpu == socket_buscado);
}

void pinear_retorno_cpu (int pid, int fd_cpu)
{
    t_retorno_cpu* retorno = malloc(sizeof(t_retorno_cpu));

    retorno->pid    = pid;
    retorno->fd_cpu = fd_cpu;

    pthread_mutex_lock(&sem_retorno_cpu);
    list_add(list_suplementarias->retorno_cpu, retorno);
    pthread_mutex_unlock(&sem_retorno_cpu);
}

/* Saca y devuelve el PID reservado para esa CPU. -1 si no hay ninguno. */
int tomar_retorno_cpu (int fd_cpu)
{
    int pid = -1;

    pthread_mutex_lock(&sem_retorno_cpu);

    t_retorno_cpu* retorno = list_find_with_context(list_suplementarias->retorno_cpu,
                                                    es_el_retorno_de_la_cpu, &fd_cpu);
    if (retorno != NULL) {
        pid = retorno->pid;
        list_remove_element(list_suplementarias->retorno_cpu, retorno);
        free(retorno);
    }

    pthread_mutex_unlock(&sem_retorno_cpu);

    return pid;
}

/* Consulta sin sacar de la lista. La usa desalojo(). */
bool existe_retorno_cpu (int pid, int fd_cpu)
{
    bool existe = false;

    pthread_mutex_lock(&sem_retorno_cpu);

    for (int i = 0; i < list_size(list_suplementarias->retorno_cpu); i++) {

        t_retorno_cpu* retorno = list_get(list_suplementarias->retorno_cpu, i);

        if (retorno->pid == pid && retorno->fd_cpu == fd_cpu) {
            existe = true;
            break;
        }
    }

    pthread_mutex_unlock(&sem_retorno_cpu);

    return existe;
}

/* Cancela la reserva (compactacion, BSOD, desconexion de la CPU). */
void quitar_retorno_cpu (int pid)
{
    pthread_mutex_lock(&sem_retorno_cpu);

    for (int i = 0; i < list_size(list_suplementarias->retorno_cpu); i++) {

        t_retorno_cpu* retorno = list_get(list_suplementarias->retorno_cpu, i);

        if (retorno->pid == pid) {
            list_remove(list_suplementarias->retorno_cpu, i);
            free(retorno);
            break;
        }
    }

    pthread_mutex_unlock(&sem_retorno_cpu);
}

/* Cierre comun de MEM_ALLOC / MEM_FREE / MUTEX_CREATE / MUTEX_UNLOCK */
void desalojar_por_syscall_mismo_cpu (PCB* pcb, int socket_cpu, char* nombre_syscall)
{
    if (pcb == NULL) {
        log_error(logger, "PCB = NULL en desalojar_por_syscall_mismo_cpu [%s]", nombre_syscall);
        return;
    }

    /* 1. Dispara el desalojo: el proximo Check Interrupt de la CPU va a responder
          DESALOJO. Es el mismo mecanismo que ya usabas en mutex_create.

          OJO: NO se puede usar existe_pcb_con_pid() aca adentro porque esa funcion
          toma sem_procesos_s_desalojo por su cuenta y el mutex no es recursivo. */
    pthread_mutex_lock(&sem_procesos_s_desalojo);

    bool ya_estaba = false;

    for (int i = 0; i < list_size(list_suplementarias->desalojo); i++) {

        PCB* pcb_en_lista = list_get(list_suplementarias->desalojo, i);

        if (pcb_en_lista->data.PID == pcb->data.PID) {
            ya_estaba = true;
            break;
        }
    }

    if (!ya_estaba)
        list_add(list_suplementarias->desalojo, pcb);

    pthread_mutex_unlock(&sem_procesos_s_desalojo);

    /* 2. Reserva la CPU para ese mismo PID */
    pinear_retorno_cpu(pcb->data.PID, socket_cpu);

    log_info(logger,
             "## PID:[%d] libera la CPU por Syscall [%s] y queda reservado para volver a la MISMA CPU",
             pcb->data.PID, nombre_syscall);
}


/*-----                     ALGORITMOS DE PLANIFICACION                     -----*/

void* control_hilo_quantum(void* arg)
{
    t_datos_quantum* datos = (t_datos_quantum*) arg;

    PCB* pcb    = datos->pcb;
    int  version = datos->version;

    usleep(info_config.intervalo_tarea * 1000);

    // Solo desaloja si el proceso sigue en RUNNING Y este timer es el del
    // despacho vigente. Un timer de un despacho anterior trae una version
    // desactualizada -> se ignora (era el bug de los desalojos espurios).
    if(pcb->estado_pcb == RNN && version == pcb->quantum_version)
    {
        pthread_mutex_lock(&sem_procesos_s_desalojo);
        list_add(list_suplementarias->desalojo, pcb);
        pthread_mutex_unlock(&sem_procesos_s_desalojo);

        log_info(
            logger,
            "## PID:[%d] - Desalojado por Fin de Quantum",/*Logger Obligatorio*/
            pcb->data.PID
        );
    }

    free(datos);

    return NULL;
}

PCB* obtener_siguiente_proceso() {
    
    PCB* pcb = NULL;

    pthread_mutex_lock(&mutex_ready);

    if(strcmp(info_config.planificacion_algoritmo, "FIFO") == 0){
        pcb = list_remove(listasProcesos->rdy, 0);
    }
    else if(strcmp(info_config.planificacion_algoritmo, "RR") == 0){
        pcb = list_remove(listasProcesos->rdy, 0);
    }
    else if(strcmp(info_config.planificacion_algoritmo, "CMN") == 0){
        for(int i = 0; i < planificador->cantidad_niveles; i++)
        {
            if(!list_is_empty(planificador->niveles[i].cola))
            {
                pcb = list_remove(planificador->niveles[i].cola, 0);
                break;
            }
        }
    }

    pthread_mutex_unlock(&mutex_ready);


    return pcb;
}

void verificar_desalojo_por_prioridad(PCB* pcb)
{
    pthread_mutex_lock(&sem_procesos_running);
    int size = list_size(listasProcesos->rnn);   // FIX: antes comparaba i < listasProcesos->rnn (puntero)

    for (int i = 0; i < size; i++)
    {
        PCB* pcb_rnn = list_get(listasProcesos->rnn, i);

        if (pcb_rnn == pcb) continue;   // el pcb que estoy encolando puede seguir en RNN

        if (pcb->data.prioridad_original < pcb_rnn->data.prioridad_original){
            pthread_mutex_unlock(&sem_procesos_running);   // soltar antes de tocar otras listas

            pthread_mutex_lock(&sem_procesos_s_desalojo);
            list_add(list_suplementarias->desalojo, pcb_rnn);
            pthread_mutex_unlock(&sem_procesos_s_desalojo);

            log_info(logger,"## PID:[%d] Prioridad: [%d] - Desalojado por cola más prioritaria por el proceso PID:[%d] con prioridad [%d]",pcb_rnn->data.PID,pcb_rnn->data.prioridad,pcb->data.PID,pcb->data.prioridad);
            return;
        }
    }
    pthread_mutex_unlock(&sem_procesos_running);
}

bool usa_quantum (PCB* pcb)
{
    if(strcmp(planificacion_algoritmo, "RR") == 0)
        return true;

    if(strcmp(planificacion_algoritmo, "FIFO") == 0)
        return false;

    if(strcmp(planificacion_algoritmo, "CMN") == 0)
    {
        int nivel = pcb->data.prioridad;

        if(planificador->niveles[nivel].tipo == RR)
            return true;
    }

    return false;
}


   /*----- Mediano Plazo -----*/

void mediano_plazo_bck(PCB* pcb){

    if (pcb == NULL || pcb->estado_pcb != BCK) { return; }

    int mi_version = pcb->block_version;

    usleep(info_config.tiempo_suspencion * 1000);

    // // Chequeo barato: evita el viaje a KM si el proceso ya se desbloqueó, o si
    // // este timer quedó viejo (block_version cambia al re-entrar a BLOCK).
    // if (pcb->estado_pcb != BCK || pcb->block_version != mi_version) { return; }

    /* Se marca SUSP. BLOCK ANTES del viaje a KM. Si la IO termina mientras KM
       escribe en SWAP, mediano_plazo_rdy() encuentra el PCB en S_BCK y hace la
       transicion legal SUSP. BLOCK -> SUSP. READY, en vez de arrastrarlo desde
       RUNNING o READY. El chequeo va adentro del mutex para cerrar la carrera. */
    pthread_mutex_lock(&mutex_transiciones);

    if (pcb->estado_pcb != BCK || pcb->block_version != mi_version) {
        pthread_mutex_unlock(&mutex_transiciones);
        return;
    }

    cambiar_estado_pcb(pcb, S_BCK);
    eliminar_proceso_Lista(pcb);
    agregar_proceso_lista(pcb);

    pthread_mutex_unlock(&mutex_transiciones);

    log_info(logger, "## PID: [%d] Suspendido Block", pcb->data.PID);
    
    bool suspension_ok = true;

    if (!mock) {
        pthread_mutex_lock(&mutex_conexion_km);
        enviar_op_code(SUSPENDIDO, info_km.conexion_km);
        enviar_pid(pcb->data.PID, info_km.conexion_km);
        int err = recibir_op_code(info_km.conexion_km);
        pthread_mutex_unlock(&mutex_conexion_km);

        if (err != OK) {
            suspension_ok = false;
            log_error(logger,
                      "ERROR al Comunicar Suspension del PID: [%d] - Se reintentará más adelante",
                      pcb->data.PID);
        }
    }
    else {
        log_info(logger, "[MOCK] KM suspendió el proceso PID [%d]", pcb->data.PID);
    }

    if (!suspension_ok) {
        /* Rollback: KM no pudo suspenderlo, hay que devolverlo a donde estaba. */
        pthread_mutex_lock(&mutex_transiciones);

        if (pcb->estado_pcb == S_BCK)       cambiar_estado_pcb(pcb, BCK);
        else if (pcb->estado_pcb == S_RDY)  cambiar_estado_pcb(pcb, RDY);
        else {
            pthread_mutex_unlock(&mutex_transiciones);
            return;
        }
        
        eliminar_proceso_Lista(pcb);
        agregar_proceso_lista(pcb);

        pthread_mutex_unlock(&mutex_transiciones);
        return;
    }
}


void mediano_plazo_rdy (PCB* pcb){

    if (pcb == NULL) { return; }

    pthread_mutex_lock(&mutex_transiciones);

    if (pcb->estado_pcb == BCK) {
        cambiar_estado_pcb(pcb, RDY);
        eliminar_proceso_Lista(pcb);
        agregar_proceso_lista(pcb);
    }
    else if (pcb->estado_pcb == S_BCK) {
        cambiar_estado_pcb(pcb, S_RDY);
        eliminar_proceso_Lista(pcb);
        agregar_proceso_lista(pcb);
    }

    pthread_mutex_unlock(&mutex_transiciones);
}


/*-----                     GESTION DE IOs                     -----*/

bool es_la_io_buscada (void* elemento, void* contexto) {
    
        t_IO* io = (t_IO*) elemento;
    
        // Casteamos el contexto de vuelta a un puntero de int para sacar el socket
     int socket_buscado = *(int*) contexto; 
    
    return (io->fd == socket_buscado);
}

/*----                     OP_CODES                     -----*/

/*-----Con la CPU-----*/
	
//NUEVA_CPU,
void nueva_cpu (int cliente_fd)/*OK*/ 
{

        int err;

        t_CPU* info_cpu = malloc(sizeof(t_CPU));

        info_cpu->fd = cliente_fd;         
        info_cpu->enUso = false;   
        info_cpu->identificador = recibir_mensaje(cliente_fd, logger);
        info_cpu->pid_ejecutando = -1;
         
                    
        pthread_mutex_lock(&mutex_cpus);
        list_add(list_suplementarias->cpu, info_cpu);
        pthread_mutex_unlock(&mutex_cpus);
        
        enviar_op_code (OK, cliente_fd);

        log_info(logger, "## CPU ID: [%s] Conectada", info_cpu->identificador); /*Logger Obligatorio*/

        log_debug(logger, "Enviando info sobre Memorys Sticks");

        int cantidad = list_size(list_suplementarias->ms);
        enviar_int(cantidad, cliente_fd);

        for (int i = 0; i < cantidad; i++) 
        {
            t_mem_stick* ms = list_get(list_suplementarias->ms, i);

            enviar_int(ms->base, cliente_fd);
            enviar_int(ms->tamanio, cliente_fd);
            enviar_mensaje(ms->ip, cliente_fd);
            enviar_mensaje(ms->puerto, cliente_fd);

            err = recibir_op_code(cliente_fd);

            if (err != OK)
            {
                log_error(logger, "Error al coordinar la recepcion incial de Sticks");
            }
        }
}

//CPU_LIBRE,
void cpu_libre (int cliente_fd)/*OK*/
{
    log_info(logger, "Revision de IOs minimas para ejecucion");
    sem_wait(&init_sem_sleep);
    sem_post(&init_sem_sleep);

    log_info(logger, "1/3 OK");
    sem_wait(&init_sem_stdin);
    sem_post(&init_sem_stdin);

    log_info(logger, "2/3 OK");
    sem_wait(&init_sem_stdout);
    sem_post(&init_sem_stdout);

    log_info(logger, "3/3 OK");
    sem_wait(&sem_compactacion);

    mandar_proceso_cpu(cliente_fd);

    sem_post(&sem_compactacion);
}

//DESALOJO
void desalojo(int socket_cliente)
{
    int pid = recibir_pid(socket_cliente);
    char* cpu_id = recibir_mensaje(socket_cliente, logger);
    op_code err = OK;
    int desalojado = 0;
    bool retorna_misma_cpu = existe_retorno_cpu(pid, socket_cliente);

    if (mem_corrupt_value == 1) {

        log_debug(logger, "MEM CORRUPT");
        
        if (retorna_misma_cpu) {
            quitar_retorno_cpu(pid); retorna_misma_cpu = false;
        }   

        enviar_op_code(MEM_CORRUPT, socket_cliente);
        log_info(logger, "## Se solicito desalojar el PID:[%d] que se encuentra ejecutando en la CPU:[%s]", pid, cpu_id);
        desalojado = 1;
        // FIX: había un segundo log_info idéntico acá, eliminado
    }
    else if (compactacion_value == 1) {

        log_debug(logger, "COMPACTACION");
        
        if (retorna_misma_cpu) {
            quitar_retorno_cpu(pid); retorna_misma_cpu = false;
        }

        enviar_op_code(COMPACTACION, socket_cliente);
        log_info(logger, "## Se solicito desalojar el PID:[%d] que se encuentra ejecutando en la CPU:[%s]", pid, cpu_id);
        desalojado = 1;
    }
    else if (existe_pcb_con_pid(list_suplementarias->desalojo, pid)) {

        PCB* pcb_chequeo = buscar_pcb_por_pid(pid);

        if (pcb_chequeo == NULL || pcb_chequeo->estado_pcb == EXT) {

            log_debug(logger, "DESALOJO POR LISTA ignorado: PID %d ya finalizado", pid);

            pthread_mutex_lock(&sem_procesos_s_desalojo);
            sacar_pcb_por_pid(list_suplementarias->desalojo, pid);
            pthread_mutex_unlock(&sem_procesos_s_desalojo);

            enviar_op_code(OK, socket_cliente);
            desalojado = 0;
        }
        else {

            log_debug(logger, "DESALOJO POR LISTA");
            enviar_op_code(DESALOJO, socket_cliente);
            log_info(logger, "## Se solicito desalojar el PID:[%d] que se encuentra ejecutando en la CPU:[%s]", pid, cpu_id);

            pthread_mutex_lock(&sem_procesos_s_desalojo);
            sacar_pcb_por_pid(list_suplementarias->desalojo, pid);
            pthread_mutex_unlock(&sem_procesos_s_desalojo);
            desalojado = 1;
            // FIX: eliminado "PCB* pcb = buscar_pcb_por_pid(pid);" que estaba
            // acá: era una variable local que no se usaba para nada
        }
    }
        else {

        // ¿Hay un Memory Stick nuevo (en caliente) pendiente de avisar?
        t_mem_stick* pendiente = NULL;
        pthread_mutex_lock(&mutex_ms_pendientes);
        if (list_ms_pendientes != NULL && list_size(list_ms_pendientes) > 0)
            pendiente = list_remove(list_ms_pendientes, 0);
        pthread_mutex_unlock(&mutex_ms_pendientes);

                if (pendiente != NULL) {

            enviar_op_code(NUEVA_MEMORY_STICK, socket_cliente);

            // La CPU espera un buffer PLANO: ip\0 puerto\0 base tamanio.
            int len_ip = strlen(pendiente->ip) + 1;
            int len_puerto = strlen(pendiente->puerto) + 1;
            int size = len_ip + len_puerto + sizeof(uint32_t) * 2;

            void* buffer = malloc(size);
            int off = 0;
            memcpy(buffer + off, pendiente->ip, len_ip);           off += len_ip;
            memcpy(buffer + off, pendiente->puerto, len_puerto);   off += len_puerto;
            memcpy(buffer + off, &pendiente->base, sizeof(uint32_t));    off += sizeof(uint32_t);
            memcpy(buffer + off, &pendiente->tamanio, sizeof(uint32_t)); off += sizeof(uint32_t);

            enviar_buffer(buffer, size, socket_cliente);
            free(buffer);

            log_info(logger, "CPU %s recibió nueva Memory Stick ", cpu_id);
            desalojado = 0;
        }
        else {

            log_info(logger, "No es Necesario Relizar Acciones");
            log_debug(logger, "NO HAY DESAOLOJO");
            enviar_op_code(OK, socket_cliente);
            desalojado = 0;
        }
    }


    err = recibir_op_code(socket_cliente);

    if (err == OK) {

        if (desalojado == 1) {

            log_debug(logger, "Entro a IF de DESALOJADO");

            PCB* pcb = buscar_pcb_por_pid(pid);

            if (pcb == NULL) {

                log_error(logger, "PID %d no encontrado", pid);

                log_debug(logger, "NEW: %d", list_size(listasProcesos->new));
                log_debug(logger, "READY: %d", list_size(listasProcesos->rdy));
                log_debug(logger, "RUNNING: %d", list_size(listasProcesos->rnn));
                log_debug(logger, "BLOCK: %d", list_size(listasProcesos->bck));
                log_debug(logger, "EXIT: %d", list_size(listasProcesos->ext));
                log_debug(logger, "S_READY: %d", list_size(listasProcesos->s_rdy));
                log_debug(logger, "S_BLOCK: %d", list_size(listasProcesos->s_bck));
            }
            else if (retorna_misma_cpu) {

                log_info(logger,
                         "PID:[%d] queda reservado para volver a la CPU:[%s] que hizo la Syscall",
                         pid, cpu_id);
            }
            else if (pcb->estado_pcb == BCK) {

                if (pcb->esperando_io) {

                    /* NUEVO: bloqueado por IO (sleep/stdin/stdout).
                       La CPU ya guardó el contexto; el proceso queda en
                       BLOCK y lo despierta rta_io_* cuando termine la IO. */
                    log_info(logger,
                             "PID:[%d] sigue bloqueado esperando IO",
                             pid);
                }
                else {

                    cambiar_estado_pcb(pcb, RDY);

                    if (compactacion_value == 1) {

                        if (strcmp(info_config.planificacion_algoritmo, "CMN") == 0) {
                            actualizar_prioridad_pcb(pcb, 0);
                        }

                        pthread_mutex_lock(&mutex_ready);
                        list_add_in_index(listasProcesos->rdy, 0, pcb);
                        pthread_mutex_unlock(&mutex_ready);

                        sem_post(&sem_hay_ready);

                    } else {

                        agregar_proceso_lista(pcb);
                    }

                    eliminar_proceso_Lista(pcb);

                    log_info(logger,
                             "Proceso Desalojado PID:[%d] de CPU:[%s]",
                             pid, cpu_id);
                }
            }
            else if (pcb->estado_pcb == RNN) {

                cambiar_estado_pcb(pcb, RDY);

                if (compactacion_value == 1) 
                {
                    // Desalojado por compactación → excepcionalmente al PRINCIPIO de READY
                    if (strcmp(info_config.planificacion_algoritmo, "CMN") == 0) 
                    {
                        actualizar_prioridad_pcb(pcb, 0);
                    }

                    pthread_mutex_lock(&mutex_ready);
                    list_add_in_index(listasProcesos->rdy, 0, pcb);
                    pthread_mutex_unlock(&mutex_ready);
                    sem_post(&sem_hay_ready);
                } 
                else 
                {
                    agregar_proceso_lista(pcb);
                }

                eliminar_proceso_Lista(pcb);

                log_info(logger,
                         "Proceso Desalojado PID:[%d] de CPU:[%s]",
                         pid, cpu_id);
            }
        }

    } else {

        log_error(logger, "Error de coordinacion en la comunicacion [desalojo]");
    }

    pthread_mutex_lock(&mutex_cpus);
    t_CPU *cpu_libre = list_find_with_context(list_suplementarias->cpu, es_la_cpu_buscada, &socket_cliente);

    if (cpu_libre != NULL && !retorna_misma_cpu && desalojado == 1)          // FIX: antes se desreferenciaba sin chequear NULL
        cpu_libre->enUso = false;

    pthread_mutex_unlock(&mutex_cpus);
    free(cpu_id);                   // FIX: recibir_mensaje hace malloc, esto se perdía en cada llamada

    return;
}
 
/*-----Con la IO-----*/

// NUEVA_IO
void nueva_io (int cliente_fd){

    t_IO* info_io = malloc(sizeof(t_IO));   // FIX 6a: era sizeof(IO) — la constante
                                            // del enum module_name (4 bytes), no el struct
    info_io->fd = cliente_fd;
    info_io->enUso = false;
    info_io->nombre = recibir_mensaje(cliente_fd, logger);

    t_list* lista_destino;

    pthread_mutex_lock(&mutex_ios);
        if(strcmp(info_io->nombre, "SLEEP") == 0) 
        {
            list_add(list_suplementarias->io_sleep, info_io);
            sem_post(&init_sem_sleep);
        }
        if(strcmp(info_io->nombre, "STDIN") == 0) 
        {
            list_add(list_suplementarias->io_stdin, info_io);
            sem_post(&init_sem_stdin);
        }
        if(strcmp(info_io->nombre, "STDOUT") == 0) 
        {
            list_add(list_suplementarias->io_stdout, info_io);
            sem_post(&init_sem_stdout);
        }
    pthread_mutex_unlock(&mutex_ios);

    enviar_op_code(OK, cliente_fd);

    log_info(logger, "IO '%s' registrada en el socket %d", info_io->nombre, cliente_fd);
}

// IO LIBRE
void io_libre(int io_socket){

    op_code io_type;
    t_IO* io_encontrada        = NULL;
    espera_io* pcb_a_ejecutar  = NULL;
    sem_t* sem_del_tipo        = NULL;
    t_list* lista_bck_del_tipo = NULL;

    pthread_mutex_lock(&mutex_ios);

        io_encontrada = list_find_with_context(list_suplementarias->io_sleep, es_la_io_buscada, &io_socket);

        if (io_encontrada != NULL) {
            io_type            = gl_IO_SLEEP;
            sem_del_tipo       = &sem_io_sleep_vacio;
            lista_bck_del_tipo = lista_bck_io->io_sleep;
        }
        else if ((io_encontrada = list_find_with_context(list_suplementarias->io_stdin, es_la_io_buscada, &io_socket)) != NULL) {
            io_type            = gl_IO_STDIN;
            sem_del_tipo       = &sem_io_stdin_vacio;
            lista_bck_del_tipo = lista_bck_io->io_stdin;
        }
        else if ((io_encontrada = list_find_with_context(list_suplementarias->io_stdout, es_la_io_buscada, &io_socket)) != NULL) {
            io_type            = gl_IO_STDOUT;
            sem_del_tipo       = &sem_io_stdout_vacio;
            lista_bck_del_tipo = lista_bck_io->io_stdout;
        }

    pthread_mutex_unlock(&mutex_ios);

    if (io_encontrada == NULL) {
        log_error(logger, "No se encontro a la IO buscada (socket %d)", io_socket);
        return;
    }

    sem_wait(sem_del_tipo);

    pthread_mutex_lock(&mutex_ios);

        if (io_encontrada->enUso || list_is_empty(lista_bck_del_tipo)) {
            pthread_mutex_unlock(&mutex_ios);
            sem_post(sem_del_tipo);          /* se devuelve el token consumido */
            return;
        }

        io_encontrada->enUso = true;
        pcb_a_ejecutar = list_remove(lista_bck_del_tipo, 0);

    pthread_mutex_unlock(&mutex_ios);


    if (pcb_a_ejecutar->io_op_code != io_type) {
        log_error(logger, "Error en Sincronizacion de Syscalls e IOs [ERROR EN TIPOS]");
        pthread_mutex_lock(&mutex_ios);
            io_encontrada->enUso = false;
        pthread_mutex_unlock(&mutex_ios);
        free(pcb_a_ejecutar);
        return;
    }


    enviar_pid(pcb_a_ejecutar->pid, io_socket);
    enviar_op_code(io_type, io_socket);

    if (recibir_op_code(io_socket) != OK) {
        /* La IO murio (tipico: se la mato estando ociosa y este hilo se
           desperto del sem_wait para tomar un trabajo que ya no puede
           entregar). ANTES aca se hacia free(pcb_a_ejecutar) y el proceso
           quedaba en BLOCK para siempre porque su trabajo se perdia y nadie
           lo volvia a despertar (bug reproducido: PID 0 colgado al final de
           la prueba de desconexion de IO).
           AHORA se REENCOLA el trabajo al frente de la cola y se hace
           sem_post, para que otra IO viva del mismo tipo lo tome. */
        log_error(logger, "La IO del socket %d no confirmo la operacion. Se reencola el trabajo del PID %d",
                  io_socket, pcb_a_ejecutar->pid);

        pthread_mutex_lock(&mutex_ios);
            io_encontrada->enUso = false;
            list_add_in_index(lista_bck_del_tipo, 0, pcb_a_ejecutar);  /* al frente: preserva orden */
        pthread_mutex_unlock(&mutex_ios);

        sem_post(sem_del_tipo);   /* despierta a otra IO del tipo para que lo tome */

        /* NO se libera pcb_a_ejecutar: sigue vivo en la cola.
           NO se setea io_encontrada->pid_ejec: este hilo ya no maneja ese trabajo. */
        return;
    }

    io_encontrada->pid_ejec = pcb_a_ejecutar;

    switch (io_type)
    {
        case gl_IO_SLEEP: {
            t_paquete* paquete = crear_paquete(PAQUETE);
            agregar_a_paquete(paquete, &pcb_a_ejecutar->pid, sizeof(uint32_t));
            agregar_a_paquete(paquete, &pcb_a_ejecutar->sleep.time, sizeof(uint32_t));

            log_info(logger, "Enviando IO_SLEEP PID=%d TIME=%d",
                     pcb_a_ejecutar->pid, pcb_a_ejecutar->sleep.time);

            enviar_solo_buffer(paquete->buffer, io_socket);
            eliminar_paquete(paquete);
            break;
        }

        case gl_IO_STDIN: {
            t_paquete* paquete = crear_paquete(gl_IO_STDIN);
            agregar_a_paquete(paquete, &pcb_a_ejecutar->pid, sizeof(uint32_t));
            agregar_a_paquete(paquete, &pcb_a_ejecutar->iostdin.direc, sizeof(uint32_t));
            agregar_a_paquete(paquete, &pcb_a_ejecutar->iostdin.length, sizeof(uint32_t));

            enviar_solo_buffer(paquete->buffer, io_socket);
            eliminar_paquete(paquete);
            break;
        }

        case gl_IO_STDOUT: {
            t_paquete* paquete = crear_paquete(PAQUETE);
            agregar_a_paquete(paquete, &pcb_a_ejecutar->pid, sizeof(uint32_t));
            agregar_a_paquete(paquete, &pcb_a_ejecutar->iostdout.length, sizeof(uint32_t));
            agregar_a_paquete(paquete, pcb_a_ejecutar->iostdout.info, pcb_a_ejecutar->iostdout.length);

            enviar_solo_buffer(paquete->buffer, io_socket);
            eliminar_paquete(paquete);   /* buffer malloc'd en io_stdout */
            break;
        }

        default:
            log_error(logger, "Tipo de IO no contemplado: %d", io_type);
            break;
    }

}


void io_finalizada(int io_socket){
    pthread_mutex_lock(&mutex_ios);

    t_IO *io = list_find_with_context(list_suplementarias->io_sleep,  es_la_io_buscada, &io_socket);
    if (io == NULL) io = list_find_with_context(list_suplementarias->io_stdin,  es_la_io_buscada, &io_socket);
    if (io == NULL) io = list_find_with_context(list_suplementarias->io_stdout, es_la_io_buscada, &io_socket);

    if (io != NULL) {
        io->enUso = false;

        if (io->pid_ejec != NULL) {
            if (io->pid_ejec->io_op_code == gl_IO_STDOUT)
                free(io->pid_ejec->iostdout.info);

            free(io->pid_ejec);
            io->pid_ejec = NULL;
        }

        log_info(logger, "IO liberada");
    }
    else {
        log_error(logger, "No se encontró la IO del socket %d al finalizar", io_socket);
    }

    pthread_mutex_unlock(&mutex_ios);

    enviar_op_code(OK, io_socket);
}

/*-----Con el Kernel Memory-----*/

//MEM_CORRUPT
void mem_corrupt(int socket_cliente)
{
    mem_corrupt_value = 1;

    log_info(logger,
            "## Se Desalojaran todas las CPUs por Mem Corrupt");

    pthread_mutex_lock(&sem_procesos_running);
    
    while (!list_is_empty(listasProcesos->rnn))
        pthread_cond_wait(&cond_rnn_vacio, &sem_procesos_running);
    
    pthread_mutex_unlock(&sem_procesos_running);

    pthread_mutex_lock(&mutex_conexion_km);
    enviar_op_code(CPUS_DESALOJADAS_OK, info_km.conexion_km);
    pthread_mutex_unlock(&mutex_conexion_km);

    log_info(logger, "Blue Screen");
    scheduler_control_loop = 0;
}

//COMPACTACION
void compactacion (int socket_cliente, int pid_trigger)
{
    int err = 0;
    compactacion_value = 1;
    sem_wait(&sem_compactacion);
    control_compac = 1;
    log_info(logger,"## Se Desalojaran todas las CPUs por Compactacion");

        pthread_mutex_lock(&sem_procesos_running);

        while (hay_otro_proceso_ejecutando(pid_trigger))
            pthread_cond_wait(&cond_rnn_vacio, &sem_procesos_running);

        pthread_mutex_unlock(&sem_procesos_running);

        /* mutex_conexion_km YA lo tiene mem_alloc: la compactación es parte
           de la misma transacción. NO se toma ni se suelta acá. */
        enviar_op_code(CPUS_DESALOJADAS_OK, info_km.conexion_km);

        log_info(logger, "## Inicio de compactación");

        err = recibir_op_code(info_km.conexion_km);

    if (err == COMPACTACION_FINALIZADA){
        compactacion_value = 0;
        sem_post(&sem_compactacion);

        log_info(logger,"## Fin de compactación");/*Logger Obligatorio*/

        /* nuevo_espacio() se movió a mem_alloc: toma mutex_conexion_km y acá
           todavía lo tenemos tomado. */
    }
    else{
        log_error(logger, "## error en resolver compactacion");
        compactacion_value = 0;
        sem_post(&sem_compactacion);
    }
}

// Se llama SIEMPRE con sem_procesos_running tomado.
bool hay_otro_proceso_ejecutando(int pid_trigger)
{
    for (int i = 0; i < list_size(listasProcesos->rnn); i++) {
        PCB* pcb = list_get(listasProcesos->rnn, i);
        if (pcb->data.PID != pid_trigger)
            return true;
    }
    return false;
}

//NUEVA_MEMORY_STICK
void recibir_nueva_memory_stick(int socket_km)
{

    t_mem_stick* ms = malloc(sizeof(t_mem_stick));


    if(mock)
    {
        log_info(logger,
            "========== MOCK MEMORY STICK ==========");
    }
    else
    {

        ms->ip = recibir_mensaje(socket_km,logger);
        ms->puerto = recibir_mensaje(socket_km,logger);
        ms->base = recibir_int(socket_km);
        ms->tamanio = recibir_int(socket_km);


        list_add(list_suplementarias->ms, ms);



        log_info(logger,
            "Nueva Memory Stick recibida: IP=%s PUERTO=%s BASE=%u TAM=%u",
            ms->ip,
            ms->puerto,
            ms->base,
            ms->tamanio
        );


                // No escribimos directo a la CPU (rompía el protocolo). Encolamos el
        // aviso y la CPU lo recibe cuando pregunta por desalojo (canal seguro).
        if (list_ms_pendientes == NULL) list_ms_pendientes = list_create();
        pthread_mutex_lock(&mutex_ms_pendientes);
        list_add(list_ms_pendientes, ms);
        pthread_mutex_unlock(&mutex_ms_pendientes);

        nuevo_espacio();

    }
}
    
    
static bool ya_fue_intentado (t_list* intentados, PCB* pcb)
{
    for (int i = 0; i < list_size(intentados); i++)
        if (list_get(intentados, i) == pcb) return true;

    return false;
}

void nuevo_espacio()
{
    /* Procesos que ya se evaluaron y no entraron en esta ronda. Sin esto, el
       while volveria a elegir siempre al mismo y giraria infinito. */
    t_list* ya_intentados = list_create();

    while (1)
    {
        PCB* pcb = NULL;

        pthread_mutex_lock(&sem_procesos_s_ready);

        if (list_is_empty(listasProcesos->s_rdy))
        {
            pthread_mutex_unlock(&sem_procesos_s_ready);
            break;
        }

        if (strcmp(info_config.planificacion_algoritmo, "CMN") == 0)
        {
            for (int prioridad = 0; prioridad < planificador->cantidad_niveles && pcb == NULL; prioridad++)
            {
                for (int i = 0; i < list_size(listasProcesos->s_rdy); i++)
                {
                    PCB* aux = list_get(listasProcesos->s_rdy, i);

                    if (aux->data.prioridad == prioridad && !ya_fue_intentado(ya_intentados, aux))
                    {
                        pcb = aux;
                        break;
                    }
                }
            }
        }
        else
        {
            for (int i = 0; i < list_size(listasProcesos->s_rdy) && pcb == NULL; i++)
            {
                PCB* aux = list_get(listasProcesos->s_rdy, i);

                if (!ya_fue_intentado(ya_intentados, aux)) pcb = aux;
            }
        }

        pthread_mutex_unlock(&sem_procesos_s_ready);

        if (pcb == NULL) break;   /* no quedan candidatos sin evaluar */

        int respuesta;

        if (mock)
        {
            log_info(logger, "[MOCK] Simulando espacio disponible");
            respuesta = OK;
        }
        else
        {
            pthread_mutex_lock(&mutex_conexion_km);
            enviar_op_code(NUEVO_ESPACIO, info_km.conexion_km);
            enviar_pid(pcb->data.PID, info_km.conexion_km);

            respuesta = recibir_op_code(info_km.conexion_km);
            pthread_mutex_unlock(&mutex_conexion_km);
        }

        if (respuesta != OK)
        {
            log_info(logger,
                "No hay espacio para PID [%d]. Continúa suspendido.",
                pcb->data.PID);

            list_add(ya_intentados, pcb);
            continue;                    /* sigue con el proximo, ya no corta */
        }

        pthread_mutex_lock(&sem_procesos_s_ready);

        bool removido = list_remove_element(listasProcesos->s_rdy, pcb);

        pthread_mutex_unlock(&sem_procesos_s_ready);

        if (!removido)
        {
            log_warning(logger,
                "PID [%d] ya no estaba en SUSP_READY al intentar desuspender",
                pcb->data.PID);
            continue;
        }

        cambiar_estado_pcb(pcb, RDY);
        agregar_proceso_lista(pcb);

        log_info(logger,
            "## PID [%d] desuspendido correctamente",
            pcb->data.PID);
    }

    list_destroy(ya_intentados);
}
// Reintenta la desuspensión periódicamente. Reemplaza al aviso roto que
// mandaba Kernel Memory por el socket compartido (nadie lo leía del lado
// de KS). Cubre los 3 casos del enunciado: se liberó memoria, se conectó
// un memory stick nuevo, o terminó una compactación.
void* hilo_reintentar_desuspension(void* arg) {
    (void) arg;

    while (scheduler_control_loop == 1) {
        sleep(2);

        if (!mock) {
            nuevo_espacio();
        }
    }

    return NULL;
}



// -------------- HERENCIA -----------------

void actualizar_herencia(mutex_cpu* mutex)
{
    
    if(mutex->dueño_actual == NULL)
        return;

    int mejor_prioridad =
        mutex->dueño_actual->data.prioridad_original;

    for(int i = 0; i < list_size(mutex->cola_mutex); i++)
    {
        int* pid_bloqueado =
            list_get(mutex->cola_mutex, i);

        PCB* pcb_bloqueado =
            buscar_pcb_por_pid(*pid_bloqueado);

        if(pcb_bloqueado == NULL)
            continue;

        if(pcb_bloqueado->data.prioridad < mejor_prioridad)
        {
            mejor_prioridad =
                pcb_bloqueado->data.prioridad;
        }
    }

    PCB* duenio = mutex->dueño_actual;

    int prioridad_anterior =
        duenio->data.prioridad;

    actualizar_prioridad_pcb( duenio, mejor_prioridad);

    if(prioridad_anterior != mejor_prioridad)
    {
        verificar_desalojo_por_prioridad(duenio);
    }
}

void actualizar_prioridad_pcb(PCB* pcb, int nueva_prioridad)
{
    int prioridad_vieja = pcb->data.prioridad;
    if (prioridad_vieja == nueva_prioridad) return;

    log_info(logger, "## %d Cambio de prioridad: %d - %d",
             pcb->data.PID, prioridad_vieja, nueva_prioridad);  

    pcb->data.prioridad = nueva_prioridad;

    if (pcb->estado_pcb == RDY) {
        pthread_mutex_lock(&mutex_ready);
        bool estaba = list_remove_element(planificador->niveles[prioridad_vieja].cola, pcb);
        if (estaba)
            list_add(planificador->niveles[nueva_prioridad].cola, pcb);
        pthread_mutex_unlock(&mutex_ready);
    }
}

void recalcular_prioridad(PCB* pcb)
{
    int nueva_prioridad =
        pcb->data.prioridad_original;

    for(int i = 0; i < list_size(pcb->mutex_tomados); i++)
    {
        mutex_cpu* mutex =
            list_get(pcb->mutex_tomados, i);

        for(int j = 0; j < list_size(mutex->cola_mutex); j++)
        {
            int* pid_esperando =
                list_get(mutex->cola_mutex, j);

            PCB* esperando =
                buscar_pcb_por_pid(*pid_esperando);

            if(esperando == NULL)
                continue;

            if(esperando->data.prioridad <
               nueva_prioridad)
            {
                nueva_prioridad =
                    esperando->data.prioridad;
            }
        }
    }

    actualizar_prioridad_pcb( pcb, nueva_prioridad );
}


/*-----                     AUXILIARES                     -----*/

void enviar_memory_stick_a_cpus(t_mem_stick* ms)
{

    for(int i = 0; i < list_size(list_suplementarias->cpu); i++)
    {
        pthread_mutex_lock(&mutex_cpus);
        t_CPU* cpu = list_get(list_suplementarias->cpu,i);
        pthread_mutex_unlock(&mutex_cpus);


        enviar_op_code(
            NUEVA_MEMORY_STICK,
            cpu->fd
        );


        t_paquete* paquete =
            crear_paquete(NUEVA_MEMORY_STICK);



        agregar_a_paquete(
            paquete,
            ms->ip,
            strlen(ms->ip)+1
        );


        agregar_a_paquete(
            paquete,
            ms->puerto,
            strlen(ms->puerto)+1
        );


        agregar_a_paquete(
            paquete,
            &ms->base,
            sizeof(uint32_t)
        );


        agregar_a_paquete(
            paquete,
            &ms->tamanio,
            sizeof(uint32_t)
        );


        enviar_paquete(
            paquete,
            cpu->fd
        );


        eliminar_paquete(paquete);


        // CPU confirma recepción
        op_code respuesta =
            recibir_op_code(cpu->fd);


        if(respuesta == OK)
        {
            log_info(logger,
                "CPU %s recibió nueva Memory Stick",
                cpu->identificador
            );
        }
        else
        {
            log_error(logger,
                "CPU %s no confirmó Memory Stick",
                cpu->identificador
            );
        }
    }
}

void enviar_proceso_finalizar_KM(int pid){ 
    
    pthread_mutex_lock(&mutex_conexion_km);

    enviar_op_code(gl_EXIT, info_km.conexion_km);
    
    enviar_pid(pid, info_km.conexion_km);
    
    pthread_mutex_unlock(&mutex_conexion_km);
    
    log_info(logger, " Enviado a KM, PID: %u", pid);
}


void enviar_proceso_KM(uint32_t pid, op_code opCode) { 
  
    t_paquete* paquete = crear_paquete(opCode);
    
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));    
    
    pthread_mutex_lock(&mutex_conexion_km);
    enviar_paquete(paquete, info_km.conexion_km);
    pthread_mutex_unlock(&mutex_conexion_km);
    
    eliminar_paquete(paquete);
    
    log_info(logger, " Enviado a KM, PID: %u", pid);
}

bool es_el_mutex_buscado(void* elemento, void* contexto) {
    
    mutex_cpu* un_mutex = (mutex_cpu*) elemento;
    char* id_buscado = (char*) contexto;
    
    // strcmp devuelve 0 si los strings son exactamente iguales
    return strcmp(un_mutex->mutex_id, id_buscado) == 0;
}


/* Busqueda en Listas */

int pid_buscado;

bool tiene_pid(void* elemento) {
    PCB* pcb = (PCB*) elemento;
    return pcb->data.PID == pid_buscado;
}

// FIX B: reemplaza el global pid_buscado + list_any_satisfy (no thread-safe)
bool _es_pid(void* elem, void* ctx) {
    return ((PCB*)elem)->data.PID == *(int*)ctx;
}

bool existe_pcb_con_pid(t_list* lista, int pid) {
    pthread_mutex_lock(&sem_procesos_s_desalojo);
    bool encontrado = list_find_with_context(lista, _es_pid, &pid) != NULL;
    pthread_mutex_unlock(&sem_procesos_s_desalojo);
    return encontrado;
}

PCB* sacar_pcb_por_pid(t_list* lista, int pid)
{
    for(int i = 0; i < list_size(lista); i++)
    {
        PCB* pcb = list_get(lista, i);

        if(pcb->data.PID == pid)
        {
            return list_remove(lista, i);
        }
    }
    return NULL;
}


/*-----                     MOCKs                     -----*/

void enviar_proceso_finalizar_KM_mock (int pid) {
    
    log_info(logger, " Enviado a KM, PID: %u", pid);

}

void data_io_stdout_mock(espera_io* io_pcb, PCB* pcb, uint32_t tam) { /*Siempre devuelve 5555 como DATA*/

    log_debug(logger,"cargando DATA para STDOUT MOCK");

    io_pcb->pid = pcb->data.PID;
    io_pcb->io_op_code = gl_IO_STDOUT;
    io_pcb->iostdout.length = tam;
    io_pcb->iostdout.info = "5555";

    return;

} 

void pruebas_cpu_ms(){
    PCB* pcb_mock = crearNuevoProceso_mock("proceso1", 1, 0);

    cambiar_estado_pcb(pcb_mock, RDY);
    eliminar_proceso_Lista(pcb_mock);
    agregar_proceso_lista(pcb_mock);
    
}


void pruebas_io(){

    espera_io* prueba = malloc(sizeof(espera_io));

    prueba->pid = 1;
    prueba->io_op_code = gl_IO_SLEEP;

    prueba->sleep.time = 5000;

    list_add(lista_bck_io, prueba);

    loguear_lista_suplementaria("BCK_IO", logger);

    log_info(logger, "Prueba IO STDIN agregada.");
}

void prueba_mediano_plazo_mock()
{
    log_info(logger, "===== PRUEBA MEDIANO PLAZO =====");

    PCB* pcb = iniciar_pcb(0, 1);

    log_info(logger, "PCB creado. PID: %d", pcb->data.PID);

    // Simulamos que el proceso entra en BLOCK
    cambiar_estado_pcb(pcb, BCK);
    agregar_proceso_lista(pcb);

    log_info(logger, "Proceso agregado a BLOCK");

    pthread_t hilo;

    if (pthread_create(&hilo, NULL, (void*)mediano_plazo_bck, pcb) != 0)
    {
        log_error(logger, "Error al crear hilo de mediano plazo");
        return;
    }

    log_info(logger, "Hilo de mediano plazo creado");

    pthread_join(hilo, NULL);

    log_info(logger, "Hilo finalizado");

    // Simulamos que terminó la IO
    pthread_t hilo1;
    pthread_create(&hilo1, NULL, (void*)mediano_plazo_bck, pcb);
    pthread_detach(hilo1);

    log_info(logger, "Proceso pasó a SUSP_READY");

    // Simulamos que apareció memoria disponible
    nuevo_espacio();

    log_info(logger, "Fin prueba mediano plazo");
}

void loguear_lista(t_list* lista, t_log* logger)
{
    log_debug(logger, "Chequeo de contenido de lista [loguear_lista]");

    log_debug(logger, "Iniciando Chequeo");

    if(list_is_empty(lista)){
        log_debug(logger, "Lista Vacia");
        return;
    }
    
    for (int i = 0; i < list_size(lista); i++){
        PCB* pcb = list_get(lista, i);
        log_debug(logger, "PID: [%d] Estado: [%s]", pcb->data.PID, nombre_estado(pcb->estado_pcb));
    }

    log_debug(logger, "Fin de Chequeo");
}

void loguear_lista_suplementaria(char* tipo_lista, t_log* logger)
{
    log_debug(logger, "==============================");
    log_debug(logger, "Chequeo Lista Suplementaria [%s]", tipo_lista);
    log_debug(logger, "==============================");

    if (list_suplementarias == NULL) {
        log_debug(logger, "list_suplementarias NULL");
        return;
    }

    if (strcmp(tipo_lista, "CPU") == 0) {

        t_list* lista = list_suplementarias->cpu;

        if (lista == NULL || list_is_empty(lista)) {
            log_debug(logger, "Lista CPU Vacia");
            return;
        }

        for (int i = 0; i < list_size(lista); i++) {
            t_CPU* cpu = list_get(lista, i);

            if (cpu == NULL) {
                log_debug(logger, "[%d] CPU NULL", i);
                continue;
            }

            log_debug(
                logger,
                "[%d] FD:[%d] ID:[%s] EnUso:[%s]",
                i,
                cpu->fd,
                cpu->identificador != NULL ? cpu->identificador : "NULL",
                cpu->enUso ? "true" : "false"
            );
        }

    }  else if (strcmp(tipo_lista, "MS") == 0) {

        t_list* lista = list_suplementarias->ms;

        if (lista == NULL || list_is_empty(lista)) {
            log_debug(logger, "Lista Memory Stick Vacia");
            return;
        }

        for (int i = 0; i < list_size(lista); i++) {
            t_mem_stick* ms = list_get(lista, i);

            if (ms == NULL) {
                log_debug(logger, "[%d] Memory Stick NULL", i);
                continue;
            }

            log_debug(
                logger,
                "[%d] Socket:[%d] IP:[%s] Puerto:[%s] Base:[%u] Tamanio:[%u]",
                i,
                ms->socket,
                ms->ip != NULL ? ms->ip : "NULL",
                ms->puerto != NULL ? ms->puerto : "NULL",
                ms->base,
                ms->tamanio
            );
        }

    } else if (strcmp(tipo_lista, "DESALOJO") == 0) {

        t_list* lista = list_suplementarias->desalojo;

        if (lista == NULL || list_is_empty(lista)) {
            log_debug(logger, "Lista Desalojo Vacia");
            return;
        }

        for (int i = 0; i < list_size(lista); i++) {
            PCB* pcb = list_get(lista, i);

            if (pcb == NULL) {
                log_debug(logger, "[%d] PCB NULL", i);
                continue;
            }

            log_debug(
                logger,
                "[%d] PID:[%d] Estado:[%s] Prioridad:[%d]",
                i,
                pcb->data.PID,
                nombre_estado(pcb->estado_pcb),
                pcb->data.prioridad
            );
        }

    } 
    else if (strcmp(tipo_lista, "MUTEX") == 0) {

        t_list* lista = lista_mutex;

        if (lista == NULL || list_is_empty(lista)) {
            log_debug(logger, "Lista Mutex Vacia");
            return;
        }

        for (int i = 0; i < list_size(lista); i++) {
            mutex_cpu* mutex = list_get(lista, i);

            if (mutex == NULL) {
                log_debug(logger, "[%d] Mutex NULL", i);
                continue;
            }

            log_debug(
                logger,
                "[%d] Mutex:[%s] Valor:[%d] DuenioPID:[%d] Cola:[%d]",
                i,
                mutex->mutex_id != NULL ? mutex->mutex_id : "NULL",
                mutex->valor,
                mutex->dueño_actual != NULL ? mutex->dueño_actual->data.PID : -1,
                mutex->cola_mutex != NULL ? list_size(mutex->cola_mutex) : -1
            );

            if (mutex->cola_mutex != NULL) {
                for (int j = 0; j < list_size(mutex->cola_mutex); j++) {
                    int* pid = list_get(mutex->cola_mutex, j);

                    log_debug(
                        logger,
                        "    Cola[%d] PID:[%d]",
                        j,
                        pid != NULL ? *pid : -1
                    );
                }
            }
        }

    } else {
        log_debug(logger, "Tipo de lista suplementaria desconocido: [%s]", tipo_lista);
    }

    log_debug(logger, "Fin Chequeo Lista Suplementaria [%s]", tipo_lista);
}

void prueba_lago_plazo_mock() {

    log_debug(logger, "=== Iniciando MOCK largo Plazo ===");

    log_debug(logger, "Procesos Actuales y su Estado => Deberia ser Solo el PID 0");

    log_debug(logger, "=== Lista NEW ===");    
    loguear_lista(listasProcesos->new, logger);
    log_debug(logger, "=== Lista RDY ==="); 
    loguear_lista(listasProcesos->rdy, logger);
    log_debug(logger, "=== Lista RNN ==="); 
    loguear_lista(listasProcesos->rnn, logger);
    log_debug(logger, "=== Lista EXT ==="); 
    loguear_lista(listasProcesos->ext, logger);
    log_debug(logger, "=== Lista BCK ==="); 
    loguear_lista(listasProcesos->bck, logger);
    log_debug(logger, "=== Lista S RDY ==="); 
    loguear_lista(listasProcesos->s_rdy, logger);
    log_debug(logger, "=== Lista S BCK ==="); 
    loguear_lista(listasProcesos->s_bck, logger);

    init_proc(1);

    loguear_lista(listasProcesos->new, logger);
    loguear_lista(listasProcesos->rdy, logger);

    exit_proceso(1);

    loguear_lista(listasProcesos->rdy, logger);
    loguear_lista(listasProcesos->rnn, logger);
    loguear_lista(listasProcesos->ext, logger);
   
}

void mock_cpu(PCB* pcb)
{
    log_info(logger, "========== CPU MOCK ==========");

    // Simulamos que la CPU ejecutó y se bloqueó por IO
    cambiar_estado_pcb(pcb, BCK);
    eliminar_proceso_Lista(pcb);
    agregar_proceso_lista(pcb);
    

    pthread_t hilo;

    pthread_create(
        &hilo,
        NULL,
        (void*) mediano_plazo_bck,
        pcb);

    pthread_detach(hilo);
}

/*-----     Syscalls CPU     -----*/

void mutex_create (int socket_cliente){ /*OK*/

    enviar_op_code(OK, socket_cliente); //Segundo paso del Handshake

    int pid = recibir_pid(socket_cliente); //Agregar esto en CPU para poder completar el logger
    log_info(logger, "## PID:[%d] Solicito Syscall: [Mutex Create]", pid); /*Logger Obligatorio*/


    char* mutex_id = recibir_mensaje (socket_cliente, logger);
    mutex_cpu* mutex = malloc(sizeof(mutex_cpu));

    mutex->mutex_id = mutex_id;
    mutex->valor = 1;
    mutex->cola_mutex = list_create();
    mutex->dueño_actual = NULL;
    
    list_add(lista_mutex, mutex);

    PCB* pcb = buscar_pcb_por_pid(pid);


            if (pcb == NULL) {
                log_error(logger, "finalizado para PID %d pero no se encontró su PCB", pid);
                return;
            }

    /* MUTEX_CREATE no bloquea: el PCB queda en RNN y vuelve a la misma CPU,
       igual que MUTEX_UNLOCK. desalojar_por_syscall_mismo_cpu() ya se encarga
       de encolarlo en la lista de desalojo y de pinear la CPU. */

    log_info(logger, "## PID:[%d] Creo el Mutex [%s]", pid, mutex->mutex_id); /*Logger Obligatorio*/

    enviar_op_code(OK, socket_cliente);

    desalojar_por_syscall_mismo_cpu(pcb, socket_cliente, "MUTEX_CREATE");

}

//MUTEX_LOCK,
void mutex_lock (int socket_cliente){

    enviar_op_code(OK, socket_cliente);

    int* pid_guardado = malloc(sizeof(int));
    int pid = recibir_pid(socket_cliente);
    *pid_guardado = pid;

    char* mutex_id = recibir_mensaje(socket_cliente, logger);

    log_info(logger, "## PID:[%d] Solicito Syscall: [Mutex Lock]", pid); /*Logger Obligatorio*/

    mutex_cpu* mutex = list_find_with_context(lista_mutex, es_el_mutex_buscado, mutex_id);

    if (mutex == NULL)
    {
        log_error(logger, "Mutex no encontrado");
        enviar_op_code(NOTOK, socket_cliente);
        free(pid_guardado);
        free(mutex_id);
        return;
    }

    PCB* pcb = buscar_pcb_por_pid(pid);

    if (pcb == NULL) {
        log_error(logger, "MUTEX_LOCK para PID %d pero no se encontró su PCB", pid);
        free(pid_guardado);
        free(mutex_id);
        return;
    }

    pthread_mutex_lock(&mutex_simulados);

    if (mutex->valor == 1)
    {
        /* Mutex libre: lo toma y SIGUE EJECUTANDO en la CPU (syscall no bloqueante) */
        mutex->valor = 0;
        mutex->dueño_actual = pcb;
        list_add(pcb->mutex_tomados, mutex);

        pthread_mutex_unlock(&mutex_simulados);

        log_info(logger, "## PID:[%d] Toma el mutex:[%s]", pid, mutex_id);

        enviar_op_code(OK, socket_cliente);

        free(pid_guardado);
    }
    else
    {
        /* Mutex ocupado: se encola, hereda prioridad, y se bloquea/desaloja */
        list_add(mutex->cola_mutex, pid_guardado);
        pcb->esperando_io = true;   /* para que desalojo() no lo pase a READY */
        actualizar_herencia(mutex);

        pthread_mutex_unlock(&mutex_simulados);

        cambiar_estado_pcb(pcb, BCK);
        agregar_proceso_lista(pcb);
        eliminar_proceso_Lista(pcb);

        pthread_mutex_lock(&sem_procesos_s_desalojo);
        list_add(list_suplementarias->desalojo, pcb);
        pthread_mutex_unlock(&sem_procesos_s_desalojo);

        enviar_op_code(OK, socket_cliente);

        log_info(logger, "## PID:[%d] No pudo tomar el mutex:[%s] y queda bloqueado", pid, mutex_id);
    }

    free(mutex_id);
}

//MUTEX_UNLOCK,
void mutex_unlock (int socket_cliente)
{
    enviar_op_code(OK, socket_cliente);

    int pid = recibir_pid(socket_cliente);
    char* mutex_id = recibir_mensaje(socket_cliente, logger);

    log_info(logger, "## PID:[%d] Solicito Syscall: [Mutex Unlock]", pid); /*Logger Obligatorio*/

    PCB* pcb = buscar_pcb_por_pid(pid);

    if (pcb == NULL) {
        log_error(logger, "MUTEX_UNLOCK para PID %d pero no se encontró su PCB", pid);
        free(mutex_id);
        return;
    }

    mutex_cpu* mutex = list_find_with_context(lista_mutex, es_el_mutex_buscado, mutex_id);

    if (mutex == NULL)
    {
        log_error(logger, "Mutex no encontrado");
        enviar_op_code(NOTOK, socket_cliente);
        free(mutex_id);
        return;
    }

    pthread_mutex_lock(&mutex_simulados);

    if (mutex->dueño_actual != pcb)
    {
        pthread_mutex_unlock(&mutex_simulados);
        log_info(logger, "ERROR en sincronizacion de MUTEX mutex_id:[%s] PID:[%d]", mutex_id, pid);
    }
    else
    {
        list_remove_element(pcb->mutex_tomados, mutex);
        recalcular_prioridad(pcb);   /* vuelve a su prioridad original si corresponde */
        mutex->dueño_actual = NULL;

        log_info(logger, "## PID:[%d] Libera el mutex:[%s]", pid, mutex_id); /*Logger Obligatorio*/

        if (!list_is_empty(mutex->cola_mutex))
        {
            /* Hand-off: el primero de la cola pasa a ser el nuevo dueño y se despierta */
            int* pid_siguiente = list_remove(mutex->cola_mutex, 0);
            PCB* pcb_sig = buscar_pcb_por_pid(*pid_siguiente);

            if (pcb_sig != NULL)
            {
                mutex->dueño_actual = pcb_sig;   /* valor queda en 0: pasa directo */
                list_add(pcb_sig->mutex_tomados, mutex);

                actualizar_herencia(mutex);

                pcb_sig->esperando_io = false;   /* ya no espera el mutex */

                pthread_mutex_unlock(&mutex_simulados);

                log_info(logger, "## PID:[%d] Toma el mutex:[%s]", pcb_sig->data.PID, mutex_id);

                /* Despertar respetando suspension: BCK->RDY o S_BCK->S_RDY */
                mediano_plazo_rdy(pcb_sig);
            }
            else
            {
                mutex->valor = 1;
                pthread_mutex_unlock(&mutex_simulados);
                log_error(logger, "PID %d esperaba el mutex pero ya no existe su PCB", *pid_siguiente);
            }

            free(pid_siguiente);
        }
        else
        {
            mutex->valor = 1;
            pthread_mutex_unlock(&mutex_simulados);
        }
    }

    /* El que libera NO se bloquea ni se desaloja: sigue ejecutando en la CPU */
    enviar_op_code(OK, socket_cliente);

    desalojar_por_syscall_mismo_cpu(pcb, socket_cliente, "MUTEX_UNLOCK");

    free(mutex_id);
}

//MEM_ALLOC
void mem_alloc (int socket_cliente){

    enviar_op_code(OK,socket_cliente); //HandShake

    char* id_segmento = recibir_mensaje(socket_cliente, logger);
    enviar_op_code(OK,socket_cliente);

    char* tamanio = recibir_mensaje(socket_cliente,logger);
    enviar_op_code(OK,socket_cliente);

    int pid = recibir_pid(socket_cliente);
    enviar_op_code(OK, socket_cliente);

    log_info(logger, "## PID:[%d] Solicito Syscall: [Mem Alloc]", pid); /*Logger Obligatorio*/

    int  base              = -1;
    bool alloc_ok          = false;
    bool hubo_compactacion = false;

    /* ===== TRANSACCIÓN ATÓMICA sobre el socket KS <-> KM =====
       El mutex se sostiene hasta el final, INCLUIDA la compactación.
       Soltarlo en el medio permitía que otro hilo colara su propio
       intercambio y se llevara una respuesta ajena. */
    pthread_mutex_lock(&mutex_conexion_km);

    enviar_op_code(gl_MEM_ALLOC, info_km.conexion_km);
    enviar_pid(pid, info_km.conexion_km);
    enviar_int(atoi(tamanio), info_km.conexion_km);
    enviar_int(atoi(id_segmento), info_km.conexion_km);

    int err = recibir_op_code(info_km.conexion_km);

    if (err == COMPACTACION) {
        hubo_compactacion = true;

        compactacion(socket_cliente, pid);            // ya no toca el mutex

        err = recibir_op_code(info_km.conexion_km);   // respuesta del alloc reintentado
    }

    if (err == OK) {
        base     = recibir_int(info_km.conexion_km);
        alloc_ok = true;
    }

    pthread_mutex_unlock(&mutex_conexion_km);
    /* ===== fin de la transacción ===== */

    if (alloc_ok) {

        log_info(logger, "Nuevo segmento ID:[%s] TAMAÑO:[%s] PID:[%d] creado en KM.",
                 id_segmento, tamanio, pid);

        enviar_int(base, socket_cliente);
        desalojar_por_syscall_mismo_cpu(buscar_pcb_por_pid(pid), socket_cliente, "MEM_ALLOC");

    }
    else {

        log_info(logger,
                 "## PID:[%d] sin espacio para el segmento. Vuelve a READY para reintentar",
                 pid);

        PCB* pcb = buscar_pcb_por_pid(pid);

        if (pcb != NULL) {
            /* Mismo mecanismo que desalojar_por_syscall_mismo_cpu, pero SIN pinear
               la CPU: asi desalojo() cae en la rama estado_pcb == RNN y hace
               RUNNING -> READY liberando la CPU. Es imprescindible, porque los
               procesos que tienen que liberar memoria necesitan ejecutar para
               llegar a su punto de bloqueo. La CPU la libera desalojo(). */
            pthread_mutex_lock(&sem_procesos_s_desalojo);
            list_add(list_suplementarias->desalojo, pcb);
            pthread_mutex_unlock(&sem_procesos_s_desalojo);
        }
        else {
            log_error(logger, "PCB = NULL en mem_alloc (sin espacio)");
        }

        enviar_int(-1, socket_cliente);
    }

    /* Va DESPUÉS del unlock: nuevo_espacio() toma mutex_conexion_km. */
    if (hubo_compactacion || !alloc_ok) {
        nuevo_espacio();
    }

    free(id_segmento);
    free(tamanio);
}

//MEM_FREE,
void mem_free (int socket_cliente){

    enviar_op_code(OK,socket_cliente);

    char* id_segmento = recibir_mensaje(socket_cliente, logger);
    enviar_op_code(OK,socket_cliente);

    int pid = recibir_pid(socket_cliente);
    enviar_op_code(OK, socket_cliente); 

    log_info(logger, "## PID:[%d] Solicito Syscall: [Mem Free]", pid); /*Logger Obligatorio*/

    /*Le enviamos la DATA a la Kernel Memory*/

    pthread_mutex_lock(&mutex_conexion_km);

    enviar_op_code(gl_MEM_FREE,info_km.conexion_km);

    enviar_pid(pid,info_km.conexion_km);
    enviar_int(atoi(id_segmento), info_km.conexion_km);

    if (recibir_op_code(info_km.conexion_km) == OK) {
        log_info(logger, "Nuevo segmento ID:[%s] PID:[%d] fue enviado a liberarse a KM.",id_segmento,pid);
    }

    pthread_mutex_unlock(&mutex_conexion_km);
    
    desalojar_por_syscall_mismo_cpu(buscar_pcb_por_pid(pid), socket_cliente, "MEM_FREE");

    free(id_segmento); 

    /* Se libero memoria -> puede haber lugar para des-suspender procesos (pag. 10) */
    nuevo_espacio();
    
} 

//INIT PROC

void init_proc(int socket_cliente) {

    enviar_op_code(OK, socket_cliente);

    char* path = recibir_mensaje(socket_cliente, logger);

    enviar_op_code(OK, socket_cliente);

    int prioridad = recibir_int(socket_cliente);

    enviar_op_code(OK, socket_cliente);

    int pid = recibir_pid(socket_cliente);

    log_info(logger,
        "## PID:[%d] Solicito Syscall: [Init Proc]",
        pid);

    PCB* pcb = buscar_pcb_por_pid(pid);

    if (pcb == NULL) {
        log_error(logger, "Pinalizado por PID %d ,  no se encontró su PCB", pid);
        return;
    }

    pthread_mutex_lock(&sem_procesos_s_desalojo);
    list_add(list_suplementarias->desalojo, pcb);
    pthread_mutex_unlock(&sem_procesos_s_desalojo);

    log_info(logger,
        "Solicitud INIT_PROC: %s (Prioridad: %d)",
        path, prioridad);

    PCB* nuevo_pcb;

    if (!mock) {

        pthread_mutex_lock(&mutex_conexion_km);

        nuevo_pcb = crearNuevoProceso(path, prioridad, info_km.conexion_km);

        int resp_init_proc = recibir_op_code(info_km.conexion_km);

        pthread_mutex_unlock(&mutex_conexion_km);

        if (resp_init_proc == OK) {
            cambiar_estado_pcb(nuevo_pcb, RDY);
            eliminar_proceso_Lista(nuevo_pcb);
            agregar_proceso_lista(nuevo_pcb);
            
        }

    } else {

        nuevo_pcb = crearNuevoProceso_mock(path, prioridad, info_km.conexion_km);

        cambiar_estado_pcb(nuevo_pcb, RDY);
        eliminar_proceso_Lista(nuevo_pcb);
        agregar_proceso_lista(nuevo_pcb);
        
    }


    enviar_op_code(OK, socket_cliente);

    free(path);
}

//EXIT
void exit_proceso(int socket_cpu){ /*OK*/

    log_debug(logger, "Iniciando EXIT Proceso");
    
    int pid_a_finalizar = recibir_pid(socket_cpu);

    /*Bloqueo y Desalojo*/
    PCB* pcb = buscar_pcb_por_pid(pid_a_finalizar);

    if (pcb == NULL){
        log_error(logger, "PCB NULL en [Exit Proceso]");
        return;
    }

    log_info(logger, "## PID:[%d] Solicito Syscall: [Exit Proc]", pid_a_finalizar); /*Logger Obligatorio*/

   
    
    enviar_op_code(OK, socket_cpu);


    log_info(logger, "Finalizando proceso PID: %d", pid_a_finalizar);

    if(!mock){enviar_proceso_finalizar_KM(pid_a_finalizar);}
    else{enviar_proceso_finalizar_KM_mock(pid_a_finalizar);}
 
    
    if (pcb != NULL) {
        cambiar_estado_pcb(pcb, EXT);
        eliminar_proceso_Lista(pcb);
        agregar_proceso_lista (pcb);
        
    }
    else{
        log_error(logger, "PCB = NULL en EXIT_PROCESO");
    }

    pthread_mutex_lock(&mutex_cpus);
    t_CPU *cpu_libre = list_find_with_context(list_suplementarias->cpu, es_la_cpu_buscada, &socket_cpu);
    

    if(cpu_libre == NULL){
        pthread_mutex_unlock(&mutex_cpus);
        log_error(logger,"Error al encontrar CPU en la lista");
        return;
    }

    cpu_libre->enUso = false;
    pthread_mutex_unlock(&mutex_cpus);

    log_info (logger, "## PID:[%d] Finalizo su ejecucion con motivo de [Fin de proceso]",pid_a_finalizar);/*Logger Obligatorio*/
    
    nuevo_espacio();
}

//SLEEP
void io_sleep(int socket_cpu) {

    int pid_a_bloquear = recibir_pid(socket_cpu);
    char* tiempo_str   = recibir_mensaje(socket_cpu, logger);

    int tiempo_ms = atoi(tiempo_str);
    free(tiempo_str);

    log_info(logger, "## PID:[%d] Solicito Syscall: [Sleep]", pid_a_bloquear);

    enviar_op_code(OK, socket_cpu);

    PCB* pcb = buscar_pcb_por_pid(pid_a_bloquear);

    if (pcb != NULL) {

        pcb->esperando_io = true;
        cambiar_estado_pcb(pcb, BCK);
        eliminar_proceso_Lista(pcb);
        agregar_proceso_lista(pcb);

        pthread_mutex_lock(&sem_procesos_s_desalojo);
        list_add(list_suplementarias->desalojo, pcb);
        pthread_mutex_unlock(&sem_procesos_s_desalojo);

        espera_io* io_pcb = malloc(sizeof(espera_io));
        io_pcb->pid        = pcb->data.PID;
        io_pcb->io_op_code = gl_IO_SLEEP;
        io_pcb->sleep.time = tiempo_ms;

        pthread_mutex_lock(&mutex_ios);
        list_add(lista_bck_io->io_sleep, io_pcb);
        pthread_mutex_unlock(&mutex_ios);

        sem_post(&sem_io_sleep_vacio);

        pthread_t hilo1;
        pthread_create(&hilo1, NULL, (void*)mediano_plazo_bck, pcb);
        pthread_detach(hilo1);

    } else {
        log_error(logger, "PID %d no encontrado en EXEC", pid_a_bloquear);
        enviar_op_code(NOTOK, socket_cpu);
    }

} 

void rta_io_sleep(int socket_io){ 

    enviar_op_code(OK, socket_io);

    int pid = recibir_pid(socket_io);



    PCB* pcb = buscar_pcb_por_pid(pid);

    if (pcb == NULL) {
        log_error(logger, "SLEEP finalizado para PID %d pero no se encontró su PCB", pid);
        return;
    }

    pcb->esperando_io = false; 
    mediano_plazo_rdy (pcb);

    log_info(logger,
        "## PID:[%d] Finalizo IO SLEEP y Pasa a estado Ready / Susp. Ready",
        pcb->data.PID
    );

    
    pthread_mutex_lock(&mutex_ios);


    t_IO *io = list_find_with_context(
        list_suplementarias->io_sleep,
        es_la_io_buscada,
        &socket_io
    );

    if(io != NULL){
        
        io->enUso = false;
        
        if (io->pid_ejec != NULL) {
            free(io->pid_ejec);
            io->pid_ejec = NULL;
        }

        log_info(logger, "IO liberada");

    }
    else{
        log_error(logger, "No se encontró IO finalizada");
    }

    pthread_mutex_unlock(&mutex_ios);
    
}

// STDIN
void io_stdin(int socket_cpu) {
    
    uint32_t tam = (uint32_t) recibir_int(socket_cpu);

    uint32_t dir = (uint32_t) recibir_int(socket_cpu);

    uint32_t pid_a_bloquear = (uint32_t) recibir_pid(socket_cpu);


    PCB* pcb = buscar_pcb_por_pid(pid_a_bloquear);
    
    log_info(logger, "## PID:[%d] Solicito Syscall: [Stdin]", pid_a_bloquear); /*Logger Obligatorio*/

    enviar_op_code(OK,socket_cpu);

    if (pcb != NULL) {
        
        /*Bloqueamos el Proceso*/
        pcb->esperando_io = true;
        cambiar_estado_pcb(pcb, BCK);
        eliminar_proceso_Lista(pcb);
        agregar_proceso_lista(pcb);
        

        pthread_mutex_lock(&sem_procesos_s_desalojo);
        list_add(list_suplementarias->desalojo, pcb);
        pthread_mutex_unlock(&sem_procesos_s_desalojo);
        
        /**/
        espera_io* io_pcb = malloc(sizeof(espera_io));

        io_pcb->pid = pcb->data.PID;
        io_pcb->io_op_code = gl_IO_STDIN;
        io_pcb->iostdin.direc = dir;
        io_pcb->iostdin.length = tam;

        pthread_mutex_lock(&mutex_ios);
        list_add(lista_bck_io->io_stdin, io_pcb);
        pthread_mutex_unlock(&mutex_ios);

        sem_post(&sem_io_stdin_vacio);
        
    } else {
        log_error(logger, "PID %d no encontrado en EXEC", pid_a_bloquear);
        enviar_op_code(NOTOK, socket_cpu);
    }

    pthread_t hilo1;
    pthread_create(&hilo1, NULL, (void*)mediano_plazo_bck, pcb);
    pthread_detach(hilo1);

}    

void rta_io_stdin(int socket_io){

    enviar_op_code(OK, socket_io);


    uint32_t direccion_logica = recibir_int(socket_io);
    uint32_t tam_datos = recibir_int(socket_io);
    char* datos_recibidos = recibir_mensaje(socket_io, logger);
    int pid = recibir_int(socket_io);

    log_debug(logger, "Texto Recibifo [%s]",(char*)datos_recibidos);


        pthread_mutex_lock(&mutex_conexion_km);

        enviar_op_code(km_IO_STDIN, info_km.conexion_km);

        enviar_int(direccion_logica, info_km.conexion_km);

        enviar_int(tam_datos, info_km.conexion_km);

        enviar_buffer(datos_recibidos, tam_datos, info_km.conexion_km);

        enviar_int(pid, info_km.conexion_km);

        int confirmacion_km = recibir_int(info_km.conexion_km);

        if (confirmacion_km != 1) {
            log_error(logger, "Error al escribir STDIN en Kernel Memory");
        }

        pthread_mutex_unlock(&mutex_conexion_km);


        PCB* pcb = buscar_pcb_por_pid(pid);

    if (pcb == NULL) {
        log_error(logger, "IO finalizado para PID %d pero no se encontró su PCB", pid);
        free(datos_recibidos);   /* aun sin PCB, el texto recibido hay que liberarlo */
        return;
    }

    free(datos_recibidos);       /* recibir_mensaje hace malloc; ya lo mandamos a KM */

    pcb->esperando_io = false;

    mediano_plazo_rdy (pcb);

    log_info(logger, "## PID:[%d] Finalizo IO STDIN", pcb->data.PID);

    pthread_mutex_lock(&mutex_ios);
    t_IO *io = list_find_with_context(
        list_suplementarias->io_stdin,
        es_la_io_buscada,
        &socket_io
    );
    if (io != NULL) {
        io->enUso = false;

        /* STDIN tampoco pasa por io_finalizada: se libera el trabajo aca. */
        if (io->pid_ejec != NULL) {
            free(io->pid_ejec);
            io->pid_ejec = NULL;
        }

        log_info(logger, "IO liberada");
    }
    pthread_mutex_unlock(&mutex_ios);

    enviar_op_code(OK, socket_io);
}


// STDOUT
// STDOUT
void io_stdout(int cpu_socket) {

    uint32_t tam = (uint32_t) recibir_int(cpu_socket);

    uint32_t dir = (uint32_t) recibir_int(cpu_socket);

    uint32_t pid_a_bloquear = (uint32_t) recibir_pid(cpu_socket);

    log_info(logger, "## PID:[%d] Solicito Syscall: [Stdout]", pid_a_bloquear); /*Logger Obligatorio*/

    PCB* pcb = buscar_pcb_por_pid(pid_a_bloquear);

    if (pcb == NULL) {
        log_error(logger, "STDOUT solicitado para PID %d pero no se encontró su PCB", pid_a_bloquear);
        enviar_op_code(NOTOK, cpu_socket);   /* la CPU no queda esperando para siempre */
        return;
    }

    pcb->esperando_io = true;
    cambiar_estado_pcb(pcb, BCK);
    eliminar_proceso_Lista(pcb);
    agregar_proceso_lista(pcb);

    pthread_mutex_lock(&sem_procesos_s_desalojo);
    list_add(list_suplementarias->desalojo, pcb);
    pthread_mutex_unlock(&sem_procesos_s_desalojo);

    enviar_op_code(OK, cpu_socket);

    espera_io* io_pcb = NULL;


    if (!mock) {
        /* Le solicitamos los datos a Kernel Memory */
        pthread_mutex_lock(&mutex_conexion_km);

        enviar_op_code(km_IO_STDOUT, info_km.conexion_km);
        recibir_op_code(info_km.conexion_km);   // OK

        enviar_pid(pid_a_bloquear, info_km.conexion_km);
        recibir_op_code(info_km.conexion_km);   // OK

        enviar_int(dir, info_km.conexion_km);
        recibir_op_code(info_km.conexion_km);   // OK

        enviar_int(tam, info_km.conexion_km);

        char* datos_leidos = malloc(tam + 1);

        int recv_ok = (recv(info_km.conexion_km, datos_leidos, tam, MSG_WAITALL) == (int)tam);

        pthread_mutex_unlock(&mutex_conexion_km);

        if (!recv_ok) {
            log_error(logger, "Error recibiendo datos desde Kernel Memory");
            free(datos_leidos);
            datos_leidos = strdup("");
        } else {
            datos_leidos[tam] = '\0';
            log_debug(logger, "KS recibió: %.*s", (int)tam, datos_leidos);
        }

        io_pcb = malloc(sizeof(espera_io));
        io_pcb->pid             = pcb->data.PID;
        io_pcb->io_op_code      = gl_IO_STDOUT;
        io_pcb->iostdout.length = tam;
        io_pcb->iostdout.info   = datos_leidos;
    }
    else {
        io_pcb = malloc(sizeof(espera_io));
        data_io_stdout_mock(io_pcb, pcb, tam);
    }

    pthread_mutex_lock(&mutex_ios);
    list_add(lista_bck_io->io_stdout, io_pcb);
    log_debug(logger, "Se agregó PID:[%d] a lista de bck_io", io_pcb->pid);
    pthread_mutex_unlock(&mutex_ios);

    sem_post(&sem_io_stdout_vacio);

    pthread_t hilo1;
    pthread_create(&hilo1, NULL, (void*)mediano_plazo_bck, pcb);
    pthread_detach(hilo1);
}

void rta_io_stdout(int socket_io){

    enviar_op_code(OK, socket_io);

    int pid = recibir_pid(socket_io);


    PCB* pcb = buscar_pcb_por_pid(pid);

    if (pcb == NULL) {
        log_error(logger, "IO finalizada para PID %d pero no se encontró su PCB", pid);
        /* Aun sin PCB hay que soltar la IO, si no queda ocupada para siempre. */
        pthread_mutex_lock(&mutex_ios);
            t_IO *io_huerfana = list_find_with_context(
                list_suplementarias->io_stdout, es_la_io_buscada, &socket_io);
            if (io_huerfana != NULL) io_huerfana->enUso = false;
        pthread_mutex_unlock(&mutex_ios);
        return;
    }

    pcb->esperando_io = false;

    mediano_plazo_rdy(pcb);

    log_info(logger, "## PID: [%d] Finalizo IO STDOUT", pcb->data.PID);

}


/* CONTROL DE CONEXIONES */

int rev_desconexion (int cliente_fd){

    log_info(logger, "Atendiendo desconexion de CLiente");


    bool encontrado = false;

    /*Revisamos si era una CPU*/
    t_CPU *cpu_libre = list_find_with_context(list_suplementarias->cpu, es_la_cpu_buscada, &cliente_fd);
    if (cpu_libre != NULL) {
        gestionar_desconexion_cpu(cpu_libre);
        encontrado = true;
    }

    t_IO* io_encontrada = list_find_with_context(list_suplementarias->io_sleep, es_la_io_buscada, &cliente_fd);
    if (io_encontrada != NULL) {
        gestionar_desconexion_io(io_encontrada);
        encontrado = true;
    }

    io_encontrada = list_find_with_context(list_suplementarias->io_stdin, es_la_io_buscada, &cliente_fd);
    if (io_encontrada != NULL) {
        gestionar_desconexion_io(io_encontrada);
        encontrado = true;
    }

    io_encontrada = list_find_with_context(list_suplementarias->io_stdout, es_la_io_buscada, &cliente_fd);
    if (io_encontrada != NULL) {
        gestionar_desconexion_io(io_encontrada);
        encontrado = true;
    }

    if (!encontrado)
    {
        log_error(logger, "No se pudo identificar al cliente desconectado");
        return -1;
    }

    return 0;
}

void gestionar_desconexion_cpu(t_CPU* cpu) {

    
    pthread_mutex_lock(&mutex_cpus);
    list_remove_element(list_suplementarias->cpu, cpu);
    pthread_mutex_unlock(&mutex_cpus);

    quitar_retorno_cpu(cpu->pid_ejecutando);

    PCB* pcb_ejec = (cpu->pid_ejecutando < 0)
                        ? NULL
                        : buscar_pcb_por_pid(cpu->pid_ejecutando);

    if (pcb_ejec == NULL) {
        log_info(logger, "La CPU desconectada no tenia proceso asignado");
        free(cpu->identificador);
        free(cpu);
        return;
    }

    bool sigue_en_esta_cpu = (pcb_ejec->fd_cpu == cpu->fd);

    if (sigue_en_esta_cpu && pcb_ejec->estado_pcb == RNN) {

        pthread_mutex_lock(&sem_procesos_s_desalojo);
        sacar_pcb_por_pid(list_suplementarias->desalojo, pcb_ejec->data.PID);
        pthread_mutex_unlock(&sem_procesos_s_desalojo);

        cambiar_estado_pcb(pcb_ejec, RDY);
        eliminar_proceso_Lista(pcb_ejec);
        agregar_proceso_lista(pcb_ejec);

        log_info(logger, "## PID:[%d] Replanificado a READY por desconexion de su CPU",
                 pcb_ejec->data.PID);
    }
    else if (sigue_en_esta_cpu && pcb_ejec->estado_pcb == BCK && pcb_ejec->esperando_io) {

        pthread_mutex_lock(&sem_procesos_s_desalojo);
        sacar_pcb_por_pid(list_suplementarias->desalojo, pcb_ejec->data.PID);
        pthread_mutex_unlock(&sem_procesos_s_desalojo);

        log_info(logger, "El PID:[%d] queda en BLOCK esperando IO pese a la desconexion de su CPU",
                 pcb_ejec->data.PID);
    }
    else if (sigue_en_esta_cpu && pcb_ejec->estado_pcb == BCK) {

        pthread_mutex_lock(&sem_procesos_s_desalojo);
        sacar_pcb_por_pid(list_suplementarias->desalojo, pcb_ejec->data.PID);
        pthread_mutex_unlock(&sem_procesos_s_desalojo);

        cambiar_estado_pcb(pcb_ejec, RDY);
        eliminar_proceso_Lista(pcb_ejec);
        agregar_proceso_lista(pcb_ejec);

        log_info(logger, "## PID:[%d] Replanificado a READY por desconexion de su CPU (estaba en BLOCK)",
                 pcb_ejec->data.PID);
    }
    else {
        log_info(logger, "La CPU desconectada no estaba ejecutando activamente (PID %d en estado %d)",
                 cpu->pid_ejecutando, pcb_ejec->estado_pcb);
    }

    free(cpu->identificador);
    free(cpu);
}

void gestionar_desconexion_io(t_IO* io) {

    sem_t*  sem_disponibilidad = NULL;   /* init_sem_*  : "hay una IO de este tipo" */
    sem_t*  sem_trabajo        = NULL;   /* sem_io_*_vacio : "hay trabajo pendiente" */
    t_list* lista_ios          = NULL;   /* lista de IOs registradas de este tipo */
    t_list* lista_trabajo      = NULL;   /* cola de trabajo pendiente de este tipo */

    if (strcmp(io->nombre, "SLEEP") == 0) {
        sem_disponibilidad = &init_sem_sleep;
        sem_trabajo        = &sem_io_sleep_vacio;
        lista_ios          = list_suplementarias->io_sleep;
        lista_trabajo      = lista_bck_io->io_sleep;
    }
    else if (strcmp(io->nombre, "STDIN") == 0) {
        sem_disponibilidad = &init_sem_stdin;
        sem_trabajo        = &sem_io_stdin_vacio;
        lista_ios          = list_suplementarias->io_stdin;
        lista_trabajo      = lista_bck_io->io_stdin;
    }
    else if (strcmp(io->nombre, "STDOUT") == 0) {
        sem_disponibilidad = &init_sem_stdout;
        sem_trabajo        = &sem_io_stdout_vacio;
        lista_ios          = list_suplementarias->io_stdout;
        lista_trabajo      = lista_bck_io->io_stdout;
    }
    else {
        log_error(logger, "IO desconectada con tipo desconocido: [%s]", io->nombre);
        return;   /* no se libera: no sabemos de qué lista sacarla */
    }

    /* --- Reencolado del trabajo en curso, si habia --- */
    espera_io* trabajo_pendiente = NULL;

    if (io->enUso && io->pid_ejec != NULL) {

        PCB* pcb_ejec = buscar_pcb_por_pid(io->pid_ejec->pid);

        if (pcb_ejec == NULL) {
            log_error(logger, "La IO [%s] tenia trabajo del PID %d pero no se encontro su PCB",
                      io->nombre, io->pid_ejec->pid);
        }
        else if (pcb_ejec->estado_pcb == BCK || pcb_ejec->estado_pcb == S_BCK) {
            log_info(logger, "Se Repite la Señal de IO para el PID:[%d] IO:[%s]",
                     pcb_ejec->data.PID, io->nombre);
            trabajo_pendiente = io->pid_ejec;   /* se reencola abajo */
        }
        else {
            log_error(logger, "El PID:[%d] de la IO [%s] estaba en estado inesperado: %d",
                      pcb_ejec->data.PID, io->nombre, pcb_ejec->estado_pcb);
        }
    }
    else if (io->enUso) {
        log_error(logger, "La IO [%s] figuraba en uso pero sin trabajo asignado", io->nombre);
    }
    else {
        log_info(logger, "La IO desconectada no estaba siendo utilizada");
    }

    pthread_mutex_lock(&mutex_ios);

        list_remove_element(lista_ios, io);

        if (trabajo_pendiente != NULL)
            list_add(lista_trabajo, trabajo_pendiente);

    pthread_mutex_unlock(&mutex_ios);


    sem_wait(sem_disponibilidad);     /* cierra el torniquete: una IO menos de este tipo */

    free(io->nombre);                 /* lo aloco recibir_mensaje en nueva_io */
    free(io);

    /* trabajo_pendiente NO se libera: sigue vivo en lista_trabajo */
}