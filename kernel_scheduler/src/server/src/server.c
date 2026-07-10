
#include "server.h"
#include "utilsKS.h"
#include "../../utils/src/global_utils.h"

int main(int argc, char *argv[]) /*OK*/
{

    printf("=====     Iniciando Kernel Scheduler     =====\n");

    if(argc != 3){
        printf("Uso: ./bin/kernel_scheduler [archivo.config] [archivoProcesos]\n");
        return 1;
    }

    printf("=====     Iniciando Cliente     =====\n");

    int err = cliente_Kernel_Scheduler(argc, argv);
    if(err != 0){log_error(logger, "Error al inciar cliente");}
    


    
    if(mock)
    {
       //prueba_mediano_plazo_mock();
       //prueba_lago_plazo_mock();
       //pruebas_io();

    }
    

    int server_fd = iniciar_servidor(config_get_string_value(config, "PUERTO_ESCUCHA"),logger); 

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
            log_error(logger, "El cliente en el socket [%d] se desconectó.", cliente_fd);
            control_loop = 0;
            return NULL ;
        }

        

        switch (opcode) {
            case NUEVA_CPU:
                nueva_cpu(cliente_fd);
                break;

            case CPU_LIBRE:
                mandar_proceso_cpu(cliente_fd);
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

            case MEM_CORRUPT:
                mem_corrupt(cliente_fd);
                break;

            case COMPACTACION:
                compactacion(cliente_fd);
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

            default:
                log_warning(logger, "Operación desconocida.");
                control_loop = 0; // Si hay basura, mejor cortar
                break;
        }
    }

    close(cliente_fd);
    return NULL;
}

/*-----                     GESTION DE PCBs                     -----*/

PCB* buscar_pcb_por_pid(int pid_recibido) /*OK*/
{ 
    
    t_list* listas_a_revisar[] = { 
        listasProcesos-> new, listasProcesos->rdy, 
        listasProcesos-> rnn, listasProcesos->bck, 
        listasProcesos-> ext 
    };

    for (int i = 0; i < 5; i++) {
        t_list* lista_actual = listas_a_revisar[i];
        
        t_list_iterator* it = list_iterator_create(lista_actual);
        while (list_iterator_has_next(it)) {
            PCB* pcb = (PCB*) list_iterator_next(it);
            if (pcb->data.PID == pid_recibido) {
                list_iterator_destroy(it);
                return pcb; 
            }
        }
        list_iterator_destroy(it);
    }

    log_error(logger, "Error al identificar PCB en listas de estados (funcion buscar_pcb_por_pid)");
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
      
   
    sem_wait(&sem_hay_ready); // Verifica que no se entre si la lista esta vacia
    pthread_mutex_lock(&mutex_cpus);

    
        /* Buscamos la CPU pasándole la dirección del socket_cliente como contexto */
    t_CPU *cpu_libre = list_find_with_context(list_suplementarias->cpu, es_la_cpu_buscada, &socket_cliente);
    
    if(compactacion_value)
    {
        if(mock)
        {

            pthread_mutex_unlock(&mutex_cpus);
            log_info(logger,
                "[MOCK] No se envia ningun proceso porque Memory esta compactando");
            
        }

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
        agregar_proceso_lista(pcb_a_ejecutar); //ESTAS FUNCIONES YA TIENEN EL MUTEX DENTRO
        
        loguear_lista(listasProcesos->rnn,logger);
        

        int err = enviar_pid (pcb_a_ejecutar->data.PID, cpu_libre->fd); //Envia el PID a la CPU
        log_info(logger, "Kernel Shceduler envio PID[%d] a CPU ID: [%s] a Ejecutarse",pcb_a_ejecutar->data.PID,cpu_libre->identificador);

        if (err != 1) 
        {
            log_error (logger, "Error en conexion con la CPU (funcion: mandar_proceso_cpu)"); // Completar log de error
            cpu_libre->enUso = false; 
            
            
            cambiar_estado_pcb(pcb_a_ejecutar, RDY);
            pcb_a_ejecutar->fd_cpu = 0;
            agregar_proceso_lista(pcb_a_ejecutar); //ESTAS FUNCIONES YA TIENEN EL MUTEX DENTRO
            eliminar_proceso_Lista(pcb_a_ejecutar);
            return;
        }

        if (usa_quantum(pcb_a_ejecutar))
        {
            t_datos_quantum* datos = malloc(sizeof(t_datos_quantum));

            datos->pcb = pcb_a_ejecutar;

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


/*-----                     ALGORITMOS DE PLANIFICACION                     -----*/

void* control_hilo_quantum(void* arg)
{
    t_datos_quantum* datos = (t_datos_quantum*) arg;

    PCB* pcb = datos->pcb;

    usleep(info_config.intervalo_tarea * 1000);

    if(pcb->estado_pcb == RNN)
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

void verificar_desalojo_por_prioridad(PCB* pcb_nuevo)
{
    pthread_mutex_lock(&sem_procesos_running);

    int size = list_size(listasProcesos->rnn);

    for(int i = 0; i < size; i++)
    {
        PCB* pcb_running = list_get(listasProcesos->rnn, i);

        if(pcb_running == NULL)
            continue;

        if(pcb_nuevo->data.prioridad < pcb_running->data.prioridad)
        {
            pthread_mutex_lock(&sem_procesos_s_desalojo);

            // Evitar duplicados por PID (NO por puntero)
            bool existe = false;

            for(int j = 0; j < list_size(list_suplementarias->desalojo); j++)
            {
                PCB* pcb_aux = list_get(list_suplementarias->desalojo, j);

                if(pcb_aux != NULL &&
                   pcb_aux->data.PID == pcb_running->data.PID)
                {
                    existe = true;
                    break;
                }
            }

            if(!existe)
            {
                list_add(list_suplementarias->desalojo, pcb_running);
            }

            pthread_mutex_unlock(&sem_procesos_s_desalojo);

            log_info(logger,
                "## PID:[%d] Proprodad:[%d] Desalojado por cola mas prioritaria por el proceso PID:[%d] con Prioridad:[%d]",
                pcb_running->data.PID,
                pcb_running->data.prioridad,
                pcb_nuevo->data.PID,
                pcb_nuevo->data.prioridad
            );

            break;
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
 
    if(pcb->estado_pcb != BCK){return;}

    usleep(info_config.tiempo_suspencion * 1000);

    if (pcb->estado_pcb == BCK){ 
        
        cambiar_estado_pcb(pcb,S_BCK);
        agregar_proceso_lista(pcb);
        eliminar_proceso_Lista(pcb);

        log_info(logger, "## PID: [%d] Suspendido Block",pcb->data.PID);

        if(!mock){
            enviar_op_code(SUSPENDIDO,info_km.conexion_km);
            enviar_pid(pcb->data.PID, info_km.conexion_km);
            int err = recibir_op_code(info_km.conexion_km);
            if( err != OK){log_info (logger, "ERROR al Comunicar Suspension del PID: [%d]",pcb->data.PID);}
        }
        else{
            log_info(logger,
            "[MOCK] KM suspendió el proceso PID [%d]",
            pcb->data.PID);
        }
    }

}

void mediano_plazo_rdy (PCB* pcb){
    if(pcb->estado_pcb == BCK){

        cambiar_estado_pcb(pcb,RDY);
        agregar_proceso_lista(pcb);
        eliminar_proceso_Lista(pcb);
    }
    else if (pcb->estado_pcb == S_BCK){
        
        cambiar_estado_pcb(pcb,S_RDY);
        agregar_proceso_lista(pcb);
        eliminar_proceso_Lista(pcb);
    }  
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

        t_CPU* info_cpu = malloc(sizeof(t_CPU));

        info_cpu->fd = cliente_fd;         
        info_cpu->enUso = false;   
        info_cpu->identificador = recibir_mensaje(cliente_fd, logger);
         
                    
        pthread_mutex_lock(&mutex_cpus);
        list_add(list_suplementarias->cpu, info_cpu);
        pthread_mutex_unlock(&mutex_cpus);
        
        enviar_op_code (OK, cliente_fd);

        log_info(logger, "## CPU ID: [%s] Conectada", info_cpu->identificador); /*Logger Obligatorio*/
}

//CPU_LIBRE,
void cpu_libre (int cliente_fd)/*OK*/
{

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

    if(mem_corrupt_value == 1){
        
        log_debug(logger, "MEM CORRUPT");
        enviar_op_code(MEM_CORRUPT, socket_cliente);
        log_info(logger, "## Se solicito desalojar el PID:[%d] que se encuentra ejecutando en la CPU:[%s]",pid,cpu_id);
        desalojado = 1;

        log_info(logger,
            "## Se solicito desalojar el PID:[%d] que se encuentra ejecutando en la CPU:[%s]",
            pid,
            cpu_id);
    }
    else if (compactacion_value == 1) {
        
        log_debug(logger, "COMPACTACION");
        enviar_op_code(COMPACTACION, socket_cliente);
        log_info(logger, "## Se solicito desalojar el PID:[%d] que se encuentra ejecutando en la CPU:[%s]",pid,cpu_id);
        desalojado = 1;
              
    }
    else if (existe_pcb_con_pid(list_suplementarias->desalojo,pid)){
        
        log_debug(logger, "DESALOJO POR LISTA");
        enviar_op_code(DESALOJO, socket_cliente);
        log_info(logger, "## Se solicito desalojar el PID:[%d] que se encuentra ejecutando en la CPU:[%s]",pid,cpu_id);
        
        pthread_mutex_lock(&sem_procesos_s_desalojo);
        sacar_pcb_por_pid(list_suplementarias->desalojo,pid);
        pthread_mutex_unlock(&sem_procesos_s_desalojo);
        desalojado = 1;

        PCB* pcb = buscar_pcb_por_pid(pid);

    }
    else {

        log_info(logger, "No es Necesario Relizar Acciones");
        log_debug(logger, "NO HAY DESAOLOJO");
        enviar_op_code(OK,socket_cliente);
        desalojado = 0;
    }

    err = recibir_op_code(socket_cliente);
    if(err == OK){

        if(desalojado == 1){
            
            log_debug(logger, "Entro a IF de DESALOJADO");
            PCB* pcb = buscar_pcb_por_pid(pid);
        
            if(pcb->estado_pcb == RNN){

                cambiar_estado_pcb(pcb,RDY);

                if (compactacion_value == 1){
                    if (strcmp(info_config.planificacion_algoritmo, "CMN") == 0){
                        actualizar_prioridad_pcb(pcb,0);
                    }
                    
                    pthread_mutex_lock(&mutex_ready);
                    list_add_in_index(listasProcesos->rdy, 0, pcb);
                    pthread_mutex_lock(&mutex_ready);

                    sem_post(&sem_hay_ready);
                }
            
                else
                {
                    agregar_proceso_lista(pcb);
                }
            
                    eliminar_proceso_Lista(pcb);
                    t_CPU *cpu_libre = list_find_with_context(list_suplementarias->cpu, es_la_cpu_buscada, &socket_cliente);
                    cpu_libre->enUso = false;
            }
        

            log_info(logger,"Proceso Desalojado PID:[%d] de CPU:[%s]",pid,cpu_id);

        }
    }
    else {
            log_error(logger, "Error de condinacion en la comunicacion [desalojo]");
        }
    
    
    t_CPU *cpu_libre = list_find_with_context(list_suplementarias->cpu, es_la_cpu_buscada, &socket_cliente);

    cpu_libre->enUso = false;
    
    
    return;

    
}  

 
/*-----Con la IO-----*/

// NUEVA_IO
void nueva_io (int cliente_fd){

    t_IO* info_io = malloc(sizeof(IO));
    info_io->fd = cliente_fd;         
    info_io->enUso = false;    
    
   
    info_io->nombre = recibir_mensaje(cliente_fd,logger); 
    
                
    pthread_mutex_lock(&mutex_ios);
    list_add(list_suplementarias->io, info_io);
    pthread_mutex_unlock(&mutex_ios);

    loguear_lista_suplementaria("BCK_IO",logger);
    loguear_lista_suplementaria("IO",logger);

    enviar_op_code (OK, cliente_fd);
                
    log_info(logger, "IO '%s' registrada en el socket %d", info_io->nombre, cliente_fd);
}

// IO LIBRE
void io_libre(int io_socket){ //Copia de atender CPU

    log_debug(logger,"[IO LIBRE] -> Consultando Semaforo");

    sem_wait(&sem_io_vacio);

    log_debug(logger, "[IO LIBRE] -> Se paso el Semaforo");
    
    pthread_mutex_lock(&mutex_ios);
    
    t_IO *io_libre = list_find_with_context(list_suplementarias->io, es_la_io_buscada, &io_socket);

    if(io_libre == NULL)
    {
        log_error(logger, "No se encontro a la IO buscada");
        pthread_mutex_unlock(&mutex_ios);
        return;
    }
    pthread_mutex_unlock(&mutex_ios);

    loguear_lista_suplementaria("BCK_IO",logger);
    loguear_lista_suplementaria("IO",logger);

    /*Mandamos el PCB a la IO*/
    if ((io_libre != NULL) && (!list_is_empty(lista_bck_io)) && (!list_is_empty(list_suplementarias->io))) 
    {   
        if(!io_libre->enUso && !list_is_empty(lista_bck_io))
        {
            io_libre->enUso = true;
        }
        else return;

        espera_io* pcb_a_ejecutar = list_remove(lista_bck_io,0);
        
        if(pcb_a_ejecutar->io_op_code == gl_IO_SLEEP)
        {
            enviar_op_code(gl_IO_SLEEP, io_socket);
            
            if(recibir_op_code(io_socket) != OK) return;

            t_paquete* paquete_para_io = crear_paquete(PAQUETE); 
            agregar_a_paquete(paquete_para_io,&pcb_a_ejecutar->pid,sizeof(uint32_t));
            agregar_a_paquete(paquete_para_io,&pcb_a_ejecutar->sleep.time,sizeof(uint32_t));        
            
            log_info(logger, "Enviando IO_SLEEP PID=%d TIME=%d",
            pcb_a_ejecutar->pid,
            pcb_a_ejecutar->sleep.time);
            
            log_info(logger, "Tamaño buffer enviado: %d", paquete_para_io->buffer->size);
            printf("ENVIO BUFFER SIZE = %d\n", paquete_para_io->buffer->size);
            
            printf("sizeof(op_code) = %ld\n", sizeof(op_code));
            printf("sizeof(uint32_t) = %ld\n", sizeof(uint32_t));
            
            enviar_solo_buffer(paquete_para_io->buffer, io_socket);
            eliminar_paquete(paquete_para_io);

        }
        
        else if (pcb_a_ejecutar->io_op_code == gl_IO_STDIN)
        {
            enviar_op_code(gl_IO_STDIN, io_socket);

            if(recibir_op_code(io_socket) != OK) return;

            t_paquete* paquete_io = crear_paquete(gl_IO_STDIN);

            log_debug("PID: %u\n", pcb_a_ejecutar->pid);
            log_debug("DIRECCION: %u\n", pcb_a_ejecutar->iostdin.direc);
            log_debug("LENGTH: %u\n", pcb_a_ejecutar->iostdin.length);

            agregar_a_paquete(paquete_io, &pcb_a_ejecutar->pid, sizeof(uint32_t));
            agregar_a_paquete(paquete_io, &pcb_a_ejecutar->iostdin.direc, sizeof(uint32_t));
            agregar_a_paquete(paquete_io, &pcb_a_ejecutar->iostdin.length, sizeof(uint32_t));

            log_debug("TAMAÑO FINAL PAQUETE: %d\n", paquete_io->buffer->size);

            enviar_solo_buffer(paquete_io->buffer, io_socket);
            eliminar_paquete(paquete_io);
        }
        
        else if (pcb_a_ejecutar->io_op_code == gl_IO_STDOUT)
        {
            enviar_op_code(gl_IO_STDOUT, io_socket);
            
            if(recibir_op_code(io_socket) != OK) return;

            t_paquete* paquete_io = crear_paquete(PAQUETE);
    
            agregar_a_paquete(paquete_io, &pcb_a_ejecutar->pid, sizeof(uint32_t));
            agregar_a_paquete(paquete_io, &pcb_a_ejecutar->iostdout.length, sizeof(uint32_t));
            agregar_a_paquete(paquete_io, pcb_a_ejecutar->iostdout.info, strlen(pcb_a_ejecutar->iostdout.info)+1); // Los datos que vinieron de memoria
            
            
            enviar_solo_buffer(paquete_io->buffer, io_socket);
            eliminar_paquete(paquete_io);

        }

        else 
        {
            if (io_libre == NULL){
                log_error(logger, "PID:[%d] Error al Identificar IO (io_libre == NULL)",pcb_a_ejecutar->pid);
            }

            if (list_is_empty(lista_bck_io)){
                log_error(logger, "PID:[%d] Error lista_bck_io Esta Vacia",pcb_a_ejecutar->pid);
            }

            if (list_is_empty(list_suplementarias->io)){
                log_error(logger,"PID:[%d] Error lista_suplementarias->io Esta Vacia",pcb_a_ejecutar->pid); 
            }
            
        }


    free(pcb_a_ejecutar);

    }
}

void io_finalizada(int io_socket)
{
    pthread_mutex_lock(&mutex_ios);

    loguear_lista_suplementaria("IO", logger);

    t_IO *io = list_find_with_context(
        list_suplementarias->io,
        es_la_io_buscada,
        &io_socket
    );

    if(io != NULL){
        io->enUso = false;
        log_info(logger, "IO liberada");
    }
    else{
        log_error(logger, "No se encontró IO finalizada");
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

    if(mock)
{
    log_info(logger, "========== MOCK MEM CORRUPT ==========");

    while(!list_is_empty(listasProcesos->rnn))
    {
        sem_wait(&sem_hay_ready);
        pthread_mutex_lock(&sem_procesos_running);

        PCB* pcb = list_remove(listasProcesos->rnn, 0);

        bool rnn_quedo_vacio = list_is_empty(listasProcesos->rnn);

        pthread_mutex_unlock(&sem_procesos_running);

        if (rnn_quedo_vacio)
        {
            sem_post(&sem_rnn_vacio);
        }


        log_info(logger,
                 "[MOCK] PID [%d] desalojado",
                 pcb->data.PID);
    }

    log_info(logger, "[MOCK] Todas las CPUs fueron desalojadas");
    log_info(logger, "[MOCK] Blue Screen");

    scheduler_control_loop = 0;

    return;
}
else
{
    pthread_mutex_lock(&sem_procesos_running);
    bool rnn_vacio = list_is_empty(listasProcesos->rnn);
    pthread_mutex_unlock(&sem_procesos_running);

    if (!rnn_vacio)
    {
        sem_wait(&sem_rnn_vacio);
    }

    enviar_op_code(CPUS_DESALOJADAS_OK, info_km.conexion_km);

    log_info(logger, "Blue Screen");

    scheduler_control_loop = 0;
}
}

//COMPACTACION
void compactacion (int socket_cliente){
    int err = 0;
    compactacion_value = 1;
    sem_wait(&sem_compactacion);
    control_compac = 1;
    log_info(logger,"## Se Desalojaran todas las CPUs por Compactacion");

     if(mock)
    {
        log_info(logger, "========== MOCK COMPACTACION ==========");

        while(!list_is_empty(listasProcesos->rnn))
        {
            PCB* pcb = list_remove(listasProcesos->rnn, 0);

            cambiar_estado_pcb(pcb, RDY);

            if(strcmp(info_config.planificacion_algoritmo, "CMN") == 0)
            {
                pcb->data.prioridad = 0;

                pthread_mutex_lock(&mutex_ready);

                list_add_in_index(
                    planificador->niveles[0].cola,
                    0,
                    pcb);

                pthread_mutex_unlock(&mutex_ready);
            }
            else
            {
                pthread_mutex_lock(&mutex_ready);

                list_add_in_index(
                    listasProcesos->rdy,
                    0,
                    pcb);

                pthread_mutex_unlock(&mutex_ready);
            }
            log_info(logger,
                    "[MOCK] PID [%d] agregado al principio de READY",
                    pcb->data.PID);
        }

        log_info(logger, "[MOCK] Todas las CPUs fueron desalojadas");

        log_info(logger, "[MOCK] Memory compactando...");

        sleep(1);

        err = COMPACTACION_FINALIZADA;
    }
    else
    {
        pthread_mutex_lock(&sem_procesos_running);

        bool rnn_vacio = list_is_empty(listasProcesos->rnn);

        pthread_mutex_unlock(&sem_procesos_running);

        if (!rnn_vacio)
        {
            sem_wait(&sem_rnn_vacio);
        }

        enviar_op_code(CPUS_DESALOJADAS_OK, info_km.conexion_km);

        log_info(logger,"## Inicio de Compactacion");/*Logger Obligatorio*/

        err = recibir_op_code(info_km.conexion_km);
    }

    if (err == COMPACTACION_FINALIZADA){
        compactacion_value = 0;
        sem_post(&sem_compactacion);

        log_info(logger,"## Compactacion finalizada");/*Logger Obligatorio*/

        nuevo_espacio();
    }
    else{
        log_error(logger, "## error en resolver compactacion");
        sem_post(&sem_compactacion);
    }
}

//NUEVA_MEMORY_STICK
void recibir_nueva_memory_stick(int socket_km)
{
    int size;

    void* buffer = recibir_buffer(&size, socket_km);


    t_mem_stick* ms = malloc(sizeof(t_mem_stick));

    int offset = 0;

    if(mock)
    {
        log_info(logger,
            "========== MOCK MEMORY STICK ==========");
    }
    else{

    // IP
    ms->ip = strdup(buffer + offset);
    offset += strlen(ms->ip) + 1;


    // Puerto
    ms->puerto = strdup(buffer + offset);
    offset += strlen(ms->puerto) + 1;


    // Base
    memcpy(
        &ms->base,
        buffer + offset,
        sizeof(uint32_t)
    );

    offset += sizeof(uint32_t);


    // Tamaño
    memcpy(
        &ms->tamanio,
        buffer + offset,
        sizeof(uint32_t)
    );


    ms->socket = -1; // KS no se conecta

    list_add(list_suplementarias->ms, ms);

    free(buffer);


    log_info(logger,
        "Nueva Memory Stick recibida: IP=%s PUERTO=%s BASE=%u TAM=%u",
        ms->ip,
        ms->puerto,
        ms->base,
        ms->tamanio
    );


    enviar_memory_stick_a_cpus(ms);
    nuevo_espacio();
    
    } 
}

//NUEVO_ESPACIO
void nuevo_espacio()
{
    while (1)
    {
        PCB* pcb = NULL;

        pthread_mutex_lock(&sem_procesos_s_ready);

        if (list_is_empty(listasProcesos->s_rdy))
        {
            pthread_mutex_unlock(&sem_procesos_s_ready);
            return;
        }

        if (strcmp(info_config.planificacion_algoritmo, "CMN") == 0)
        {
            for (int prioridad = 1;
                 prioridad <= planificador->cantidad_niveles && pcb == NULL;
                 prioridad++)
            {
                for (int i = 0; i < list_size(listasProcesos->s_rdy); i++)
                {
                    PCB* aux = list_get(listasProcesos->s_rdy, i);

                    if (aux->data.prioridad == prioridad)
                    {
                        pcb = aux;
                        break;
                    }
                }
            }
        }
        else
        {
            pcb = list_get(listasProcesos->s_rdy, 0);
        }

        pthread_mutex_unlock(&sem_procesos_s_ready);

        if (pcb == NULL)
        {
            log_warning(logger,
                "No se encontró ningún proceso en SUSP_READY");
            return;
        }

        int respuesta;

        if (mock)
        {
            log_info(logger, "[MOCK] Simulando espacio disponible");
            respuesta = OK;
        }
        else
        {
            enviar_op_code(NUEVO_ESPACIO, info_km.conexion_km);
            enviar_pid(pcb->data.PID, info_km.conexion_km);

            respuesta = recibir_op_code(info_km.conexion_km);
        }

        if (respuesta != OK)
        {
            log_info(logger,
                "No hay espacio para PID [%d]. Continúa suspendido.",
                pcb->data.PID);

            return;
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

    if(prioridad_vieja == nueva_prioridad)
        return;

    pcb->data.prioridad = nueva_prioridad;

    if(pcb->estado_pcb == RDY)
    {
        pthread_mutex_lock(&mutex_ready);

        list_remove_element(
            planificador->niveles[prioridad_vieja].cola,
            pcb
        );

        list_add(
            planificador->niveles[nueva_prioridad].cola,
            pcb
        );

        pthread_mutex_unlock(&mutex_ready);

    }
    log_info(logger,"## PID:[%d] Cambio de prioridad: [%d] => [%d]",pcb->data.PID,prioridad_vieja,nueva_prioridad);
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

        t_CPU* cpu = list_get(list_suplementarias->cpu,i);


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
    
    enviar_op_code (gl_EXIT, info_km.conexion_km);
    
    t_paquete* paquete = crear_paquete(gl_EXIT);
    
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));    
    
    enviar_paquete(paquete, info_km.conexion_km);
    
    eliminar_paquete(paquete);
    
    log_info(logger, " Enviado a KM, PID: %u", pid);
}

void enviar_proceso_KM(uint32_t pid, op_code opCode) { 
  
    t_paquete* paquete = crear_paquete(opCode);
    
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));    
    
    enviar_paquete(paquete, info_km.conexion_km);
    
    eliminar_paquete(paquete);
    
    log_info(logger, " Enviado a KM, PID: %u", pid);
}

bool es_el_mutex_buscado(void* elemento, void* contexto) {
    
    mutex_cpu* un_mutex = (mutex_cpu*) elemento;
    char* id_buscado = (char*) contexto;
    
    // strcmp devuelve 0 si los strings son exactamente iguales
    return strcmp(un_mutex->mutex_id, id_buscado) == 0;
}

espera_io* encontrar_pid_io_bck (int pid) {
    
    pthread_mutex_lock(&mutex_ios); 

    espera_io* pcb_buscado = NULL;
    
    for (int i = 0; i < list_size(lista_bck_io); i++) {
        espera_io* io_pcb = list_get(lista_bck_io, i);
        if (io_pcb->pid == pid) {
            pcb_buscado = io_pcb;
            break;
        }
    }
    
    pthread_mutex_unlock(&mutex_ios);
    return pcb_buscado;
}

/* Busqueda en Listas */

int pid_buscado;

bool tiene_pid(void* elemento) {
    PCB* pcb = (PCB*) elemento;
    return pcb->data.PID == pid_buscado;
}

bool existe_pcb_con_pid(t_list* lista, int pid) {
    pid_buscado = pid;
    return list_any_satisfy(lista, tiene_pid);
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

    } else if (strcmp(tipo_lista, "IO") == 0) {

        t_list* lista = list_suplementarias->io;

        if (lista == NULL || list_is_empty(lista)) {
            log_debug(logger, "Lista IO Vacia");
            return;
        }

        for (int i = 0; i < list_size(lista); i++) {
            t_IO* io = list_get(lista, i);

            if (io == NULL) {
                log_debug(logger, "[%d] IO NULL", i);
                continue;
            }

            log_debug(
                logger,
                "[%d] FD:[%d] Nombre:[%s] EnUso:[%s]",
                i,
                io->fd,
                io->nombre != NULL ? io->nombre : "NULL",
                io->enUso ? "true" : "false"
            );
        }

    } else if (strcmp(tipo_lista, "MS") == 0) {

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

    } else if (strcmp(tipo_lista, "BCK_IO") == 0) {

        t_list* lista = lista_bck_io;

        if (lista == NULL || list_is_empty(lista)) {
            log_debug(logger, "Lista BCK_IO Vacia");
            return;
        }

        for (int i = 0; i < list_size(lista); i++) {
            espera_io* io_pcb = list_get(lista, i);

            if (io_pcb == NULL) {
                log_debug(logger, "[%d] espera_io NULL", i);
                continue;
            }

            log_debug(
                logger,
                "[%d] PID:[%d] IO_OP:[%s]",
                i,
                io_pcb->pid,
                opcode_to_string(io_pcb->io_op_code)
            );

            switch (io_pcb->io_op_code) {
                case IO_SLEEP:
                case gl_IO_SLEEP:
                    log_debug(logger, "    SLEEP Time:[%u]", io_pcb->sleep.time);
                    break;

                case IO_STDIN:
                case gl_IO_STDIN:
                    log_debug(
                        logger,
                        "    STDIN Direccion:[%u] Length:[%u]",
                        io_pcb->iostdin.direc,
                        io_pcb->iostdin.length
                    );
                    break;

                case IO_STDOUT:
                case gl_IO_STDOUT:
                    log_debug(
                        logger,
                        "    STDOUT Length:[%u] Info:[%s]",
                        io_pcb->iostdout.length,
                        io_pcb->iostdout.info != NULL ? io_pcb->iostdout.info : "NULL"
                    );
                    break;

                default:
                    log_debug(logger, "    Tipo de IO no reconocido");
                    break;
            }
        }

    } else if (strcmp(tipo_lista, "MUTEX") == 0) {

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

    agregar_proceso_lista(pcb);
    eliminar_proceso_Lista(pcb);

    pthread_t hilo;

    pthread_create(
        &hilo,
        NULL,
        (void*) mediano_plazo_bck,
        pcb);

    pthread_detach(hilo);
}

/*-----     Syscalls CPU     -----*/

//MUTEX_CREATE,
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

    /*Bloqueo y Desalojo*/
    PCB* pcb = buscar_pcb_por_pid(pid);
    cambiar_estado_pcb(pcb,BCK);
    agregar_proceso_lista(pcb);
    eliminar_proceso_Lista(pcb);

    loguear_lista(listasProcesos->bck,logger);

    pthread_mutex_lock(&sem_procesos_s_desalojo);
    list_add(list_suplementarias->desalojo, pcb);
    pthread_mutex_unlock(&sem_procesos_s_desalojo);

    log_info(logger, "## PID:[%d] Creo el Mutex [%s]", pid, mutex->mutex_id); /*Logger Obligatorio*/

    enviar_op_code(OK, socket_cliente);

    cambiar_estado_pcb(pcb,RDY);
    agregar_proceso_lista(pcb);
    eliminar_proceso_Lista(pcb);
        
}

//MUTEX_LOCK,
void mutex_lock (int socket_cliente){

        enviar_op_code(OK, socket_cliente);


        int* pid_guardado = malloc(sizeof(int));
        int pid = recibir_pid (socket_cliente);

        *pid_guardado = pid;


        char* mutex_id = recibir_mensaje (socket_cliente, logger);

        log_info(logger, "## PID:[%d] Solicito Syscall: [Mutex Lock]", pid); /*Logger Obligatorio*/

        mutex_cpu* mutex = list_find_with_context(lista_mutex, es_el_mutex_buscado, mutex_id);
        

        if (mutex == NULL) 
        {
            log_error(logger, "Mutex no encontrado");
            enviar_op_code(NOTOK,socket_cliente);    
            return;
        }

        pthread_mutex_lock(&mutex_simulados);
        list_add(mutex->cola_mutex, pid_guardado);
        if(mutex->dueño_actual != NULL)
        {
            actualizar_herencia(mutex);
        }
        pthread_mutex_unlock(&mutex_simulados);

        /*Bloqueo y Desalojo*/
        PCB* pcb = buscar_pcb_por_pid(pid);
        cambiar_estado_pcb(pcb,BCK);
        agregar_proceso_lista(pcb);
        eliminar_proceso_Lista(pcb);

        pthread_mutex_lock(&sem_procesos_s_desalojo);
        list_add(list_suplementarias->desalojo, pcb);
        pthread_mutex_unlock(&sem_procesos_s_desalojo);

        enviar_op_code(OK, socket_cliente);

        while (1)
    {
        pthread_mutex_lock(&mutex_simulados);

        int* primer_pid = list_get(mutex->cola_mutex, 0);

        pthread_mutex_unlock(&mutex_simulados);

        if(primer_pid != NULL && *primer_pid == pid)
        {
            while(1)
            {
                if(mutex->valor == 1)
                {
                    log_info(logger,
                            "## PID:[%d] Toma el mutex:[%s]",
                            pid,
                            mutex_id);
                pcb = buscar_pcb_por_pid(pid);

                mutex->dueño_actual = pcb;

                list_add(pcb->mutex_tomados, mutex);

                mutex->valor = 0;

                break;
                }

                log_info(logger,
                        "## PID:[%d] No pudo tomar el mutex:[%s]. Vuelve a intentarlo... (Proximo en la lista)",
                        pid,
                        mutex_id);

                usleep(1000);
            }

            break;
        }

        log_info(logger,
                "## PID:[%d] No pudo tomar el mutex:[%s]. Vuelve a intentarlo...",
                pid,
                mutex_id);

        usleep(1000);
    }


    cambiar_estado_pcb(pcb,RDY);
    agregar_proceso_lista(pcb);
    eliminar_proceso_Lista(pcb);

        free(mutex_id); 
}

//MUTEX_UNLOK,
void mutex_unlock (int socket_cliente)
{

        enviar_op_code(OK,socket_cliente);

        int pid = recibir_pid(socket_cliente);
        char* mutex_id = recibir_mensaje (socket_cliente, logger);
        log_info(logger, "## PID:[%d] Solicito Syscall: [Mutex Unlock]", pid); /*Logger Obligatorio*/

        /*Bloqueo y Desalojo*/
        PCB* pcb = buscar_pcb_por_pid(pid);
        cambiar_estado_pcb(pcb,BCK);
        agregar_proceso_lista(pcb);
        eliminar_proceso_Lista(pcb);

        pthread_mutex_lock(&sem_procesos_s_desalojo);
        list_add(list_suplementarias->desalojo, pcb);
        pthread_mutex_unlock(&sem_procesos_s_desalojo);

        enviar_op_code(OK, socket_cliente);

        mutex_cpu* mutex = list_find_with_context(lista_mutex, es_el_mutex_buscado, mutex_id);

        if(mutex == NULL)
        {
            log_error(logger, "Mutex no encontrado");
            free(mutex_id);
            return;
        }

        pthread_mutex_lock(&mutex_simulados);

        int* primer_pid = list_get(mutex->cola_mutex, 0);

        if(primer_pid == NULL || *primer_pid != pid)
        {
            pthread_mutex_unlock(&mutex_simulados);

            log_info(logger,
                "ERROR en sincronizacion de MUTEX mutex_id:[%s] PID:[%d]",
                mutex_id,
                pid);

            enviar_op_code(NOTOK, socket_cliente);

            free(mutex_id);
            return;
        }

        int* pid_removed = list_remove(mutex->cola_mutex, 0);
        
        if(pid_removed != NULL && *pid_removed == pid){

            mutex->valor = 1;
            PCB* pcb = buscar_pcb_por_pid(pid);

            list_remove_element(
                pcb->mutex_tomados,
                mutex
            );

            recalcular_prioridad(pcb);

            mutex->dueño_actual = NULL;
            log_info(logger,"## PID:[%d] Libera el mutex:[%s]",pid,mutex_id);/*Logger Obligatorio*/
            
            cambiar_estado_pcb(pcb,RDY);
            agregar_proceso_lista(pcb);
            eliminar_proceso_Lista(pcb);

        }
        else{
            log_info (logger, "ERROR en sincronizacion de MUTEX mutex_id:[%s] PID:[%d]",mutex_id,pid);
            enviar_op_code(NOTOK, socket_cliente);
        }
        
        pthread_mutex_unlock (&mutex_simulados);
        free(pid_removed);
        free(mutex_id);

    }

//MEM_ALLOC,k
void mem_alloc (int socket_cliente){
    
    enviar_op_code(OK,socket_cliente); //HandShake

    char* id_segmento = recibir_mensaje(socket_cliente, logger);
    enviar_op_code(OK,socket_cliente); 

    char* tamanio = recibir_mensaje(socket_cliente,logger);
    enviar_op_code(OK,socket_cliente);


    int pid = recibir_pid(socket_cliente);
    enviar_op_code(OK, socket_cliente);

    log_info(logger, "## PID:[%d] Solicito Syscall: [Mem Alloc]", pid); /*Logger Obligatorio*/

    /*Le envamos la DATA a la Kernel Memory*/

    enviar_op_code(gl_MEM_ALLOC,info_km.conexion_km);

    t_paquete* paquete = crear_paquete(gl_MEM_ALLOC);

    agregar_a_paquete(paquete, &id_segmento, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    

    enviar_paquete(paquete, info_km.conexion_km);
    eliminar_paquete(paquete);

    

    if (recibir_op_code(info_km.conexion_km) == OK) {
        log_info(logger, "Nuevo segmento ID:[%s] TAMAÑO:[%s] PID:[%d] fue enviado a KM.",id_segmento,tamanio,pid);
    }

    int base = recibir_pid(info_km.conexion_km);
    enviar_pid(base, socket_cliente);

}; 

//MEM_FREE,
void mem_free (int socket_cliente){

    enviar_op_code(OK,socket_cliente);

    char* id_segmento = recibir_mensaje(socket_cliente, logger);
    enviar_op_code(OK,socket_cliente);

    int pid = recibir_pid(socket_cliente);
    enviar_op_code(OK, socket_cliente); 

    log_info(logger, "## PID:[%d] Solicito Syscall: [Mem Free]", pid); /*Logger Obligatorio*/

    /*Le enviamos la DATA a la Kernel Memory*/

    enviar_op_code(gl_MEM_FREE,info_km.conexion_km);

    t_paquete* paquete = crear_paquete(gl_MEM_FREE);

    agregar_a_paquete(paquete, &id_segmento, sizeof(uint32_t));
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    

    enviar_paquete(paquete, info_km.conexion_km);
    eliminar_paquete(paquete);

    if (recibir_op_code(info_km.conexion_km) == OK) {
        log_info(logger, "Nuevo segmento ID:[%s] PID:[%d] fue enviado a liberarse a KM.",id_segmento,pid);
    }
    
} 

//INIT PROC
void init_proc(int socket_cliente){
    
    t_list* lista = recibir_paquete(socket_cliente);
    char* path = (char*)list_get(lista, 0);
    int prioridad = *(int*)list_get(lista, 1);

    int pid = recibir_pid(socket_cliente); //Agregar esto en CPU para poder completar el logger
    log_info(logger, "## PID:[%d] Solicito Syscall: [Init Proc]", pid); /*Logger Obligatorio*/

    /*Bloqueo y Desalojo*/
    PCB* pcb = buscar_pcb_por_pid(pid);
    cambiar_estado_pcb(pcb,BCK);
    agregar_proceso_lista(pcb);
    eliminar_proceso_Lista(pcb);

    pthread_mutex_lock(&sem_procesos_s_desalojo);
    list_add(list_suplementarias->desalojo, pcb);
    pthread_mutex_unlock(&sem_procesos_s_desalojo);

    enviar_op_code(OK,socket_cliente);

    log_info(logger, "Solicitud INIT_PROC: %s (Prioridad: %d)", path, prioridad);

    PCB* nuevo_pcb; 
    if(!mock){
        
        nuevo_pcb = crearNuevoProceso(path, prioridad, info_km.conexion_km);
        
        if (recibir_op_code(info_km.conexion_km) == OK) {
                cambiar_estado_pcb(nuevo_pcb, RDY);
                agregar_proceso_lista (nuevo_pcb);
                eliminar_proceso_Lista(nuevo_pcb);           
        }
    
    }
    else{
        
        nuevo_pcb = crearNuevoProceso_mock(path, prioridad, info_km.conexion_km);
        cambiar_estado_pcb(nuevo_pcb, RDY);
        agregar_proceso_lista (nuevo_pcb);
        eliminar_proceso_Lista(nuevo_pcb);   
    }

    cambiar_estado_pcb(pcb,RDY);
    agregar_proceso_lista(pcb);
    eliminar_proceso_Lista(pcb);

    list_destroy(lista);
    
                
}

//EXIT
void exit_proceso(int socket_cpu){ /*OK*/

    log_debug(logger, "Iniciando EXIT Proceso");
    
    int pid_a_finalizar = recibir_pid(socket_cpu);

    /*Bloqueo y Desalojo*/
    PCB* pcb = buscar_pcb_por_pid(pid_a_finalizar);

    if (pcb == NULL){
        log_error(logger, "PCB NULL en [Exit Proceso]");

    }

    log_info(logger, "## PID:[%d] Solicito Syscall: [Exit Proc]", pid_a_finalizar); /*Logger Obligatorio*/

    cambiar_estado_pcb(pcb,BCK);
    agregar_proceso_lista(pcb);
    eliminar_proceso_Lista(pcb);


    pthread_mutex_lock(&sem_procesos_s_desalojo);
    list_add(list_suplementarias->desalojo, pcb);
    pthread_mutex_unlock(&sem_procesos_s_desalojo);
    
    
    enviar_op_code(OK, socket_cpu);


    log_info(logger, "Finalizando proceso PID: %d", pid_a_finalizar);

    if(!mock){enviar_proceso_finalizar_KM(pid_a_finalizar);}
    else{enviar_proceso_finalizar_KM_mock(pid_a_finalizar);}
 
    
    if (pcb != NULL) {
        cambiar_estado_pcb(pcb, EXT);
        agregar_proceso_lista (pcb);
        eliminar_proceso_Lista(pcb);
    }
    else{
        log_error(logger, "PCB = NULL en EXIT_PROCESO");
    }

    t_CPU *cpu_libre = list_find_with_context(list_suplementarias->cpu, es_la_cpu_buscada, &socket_cpu);
    
    if(cpu_libre == NULL){
        log_error(logger,"Error al encontrar CPU en la lista");
        return;
    }

    cpu_libre->enUso = false;

    log_info (logger, "## PID:[%d] Finalizo su ejecucion con motivo de [Fin de proceso]",pid_a_finalizar);/*Logger Obligatorio*/
    
    nuevo_espacio();
}

//SLEEP
void io_sleep(int socket_cpu) { 
    
    int pid_a_bloquear = recibir_pid(socket_cpu);
    char* tiempo_str = recibir_mensaje(socket_cpu,logger);
    
    int tiempo_ms = atoi(tiempo_str);
    
    

    log_info(logger, "## PID:[%d] Solicito Syscall: [Sleep]", pid_a_bloquear); /*Logger Obligatorio*/

    enviar_op_code(OK,socket_cpu);

    PCB* pcb = buscar_pcb_por_pid(pid_a_bloquear);

    
    if (pcb != NULL) {
        
        /*Bloqueamos el Proceso*/
        cambiar_estado_pcb(pcb, BCK);
        agregar_proceso_lista(pcb);
        eliminar_proceso_Lista(pcb);

        pthread_mutex_lock(&sem_procesos_s_desalojo);
        list_add(list_suplementarias->desalojo, pcb);
        pthread_mutex_unlock(&sem_procesos_s_desalojo);
        
        /**/
        espera_io* io_pcb = malloc(sizeof(espera_io));

        io_pcb->pid = pcb->data.PID;
        io_pcb->io_op_code = gl_IO_SLEEP;
        io_pcb->sleep.time = tiempo_ms;

        pthread_mutex_lock(&mutex_ios);
        list_add(lista_bck_io, io_pcb);
        pthread_mutex_unlock(&mutex_ios);

        sem_post(&sem_io_vacio);
        
        
    } else {
        log_error(logger, "PID %d no encontrado en EXEC", pid_a_bloquear);
        enviar_op_code(NOTOK, socket_cpu);
    }

    pthread_t hilo1;
    pthread_create(&hilo1, NULL, (void*)mediano_plazo_bck, pcb);
    pthread_detach(hilo1);
}

void rta_io_sleep(int socket_io){ 

    enviar_op_code(OK, socket_io);

    int pid = recibir_pid(socket_io);



    PCB* pcb = buscar_pcb_por_pid(pid);

   mediano_plazo_rdy (pcb);

    log_info(logger,
        "## PID:[%d] Finalizo IO SLEEP y Pasa a estado Ready / Susp. Ready",
        pcb->data.PID
    );
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
        cambiar_estado_pcb(pcb, BCK);
        agregar_proceso_lista(pcb);
        eliminar_proceso_Lista(pcb);

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
        list_add(lista_bck_io, io_pcb);
        pthread_mutex_unlock(&mutex_ios);

        sem_post(&sem_io_vacio);
        
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

    t_list* lista = recibir_paquete(socket_io);

    uint32_t direccion_logica = *(uint32_t*)list_get(lista,0);
    uint32_t tam_datos = *(uint32_t*)list_get(lista,1);
    void* datos_recibidos = list_get(lista,2);
    int pid = *(int*)list_get(lista,3);

    log_debug(logger, "Texto Recibifo [%s]",(char*)datos_recibidos);

    if(!mock){

        enviar_op_code(km_IO_STDIN, info_km.conexion_km);

        if(recibir_op_code(info_km.conexion_km)!=OK){
            log_error(logger,"Error al enviar STDIN a KM");
            return;
        }


        t_paquete* paquete_mem = crear_paquete(km_IO_STDIN);

        agregar_a_paquete(paquete_mem,&direccion_logica,sizeof(uint32_t));
        agregar_a_paquete(paquete_mem,&tam_datos,sizeof(uint32_t));
        agregar_a_paquete(paquete_mem,datos_recibidos,tam_datos);
        agregar_a_paquete(paquete_mem,&pid,sizeof(int));

        enviar_paquete(paquete_mem,info_km.conexion_km);

        eliminar_paquete(paquete_mem);
    }


    PCB* pcb = buscar_pcb_por_pid(pid);

    mediano_plazo_rdy (pcb);


    log_info(logger,
        "PID:[%d] Finalizo IO STDIN",
        pcb->data.PID
    );


    list_destroy_and_destroy_elements(lista,free);
}


// STDOUT
void io_stdout(int cpu_socket) {
    
    uint32_t tam = (uint32_t) recibir_int(cpu_socket);

    uint32_t dir = (uint32_t) recibir_int(cpu_socket);

    uint32_t pid_a_bloquear = (uint32_t) recibir_pid(cpu_socket);
   
    log_info(logger, "## PID:[%d] Solicito Syscall: [Stdout]", pid_a_bloquear); /*Logger Obligatorio*/

    enviar_op_code(OK,cpu_socket);

    
    PCB* pcb = buscar_pcb_por_pid(pid_a_bloquear);
    
    /*Bloqueamos el Proceso*/
    cambiar_estado_pcb(pcb, BCK);
    agregar_proceso_lista(pcb);
    eliminar_proceso_Lista(pcb);

    
    pthread_mutex_lock(&sem_procesos_s_desalojo);
    list_add(list_suplementarias->desalojo, pcb);
    pthread_mutex_unlock(&sem_procesos_s_desalojo);


    espera_io* io_pcb = NULL;
    
    if(!mock){
        /*Le solicitamos los Datos de la KM*/
        enviar_op_code(km_IO_STDOUT, info_km.conexion_km);

        t_paquete* req_mem = crear_paquete(km_IO_STDOUT);
        agregar_a_paquete(req_mem, &pid_a_bloquear, sizeof(uint32_t));
        agregar_a_paquete(req_mem, &dir, sizeof(uint32_t));
        agregar_a_paquete(req_mem, &tam, sizeof(uint32_t));
        enviar_paquete(req_mem, info_km.conexion_km); 
        eliminar_paquete(req_mem);

        // recibir de km
        t_list* lista_mem = recibir_paquete(info_km.conexion_km);
        char* datos_leidos = (char*)list_get(lista_mem, 0);

        io_pcb = malloc(sizeof(espera_io));

        io_pcb->pid = pcb->data.PID;
        io_pcb->io_op_code = gl_IO_STDOUT;
        io_pcb->iostdout.length = tam;
        io_pcb->iostdout.info = datos_leidos;

 
    }
    else {
        
        io_pcb = malloc(sizeof(espera_io));
        data_io_stdout_mock(io_pcb, pcb, tam);
    }
        
    pthread_mutex_lock(&mutex_ios);
    list_add(lista_bck_io, io_pcb);
    log_debug(logger,"Se Agego PID:[%d] a lista de bck_io",io_pcb->pid);
    pthread_mutex_unlock(&mutex_ios);

    sem_post(&sem_io_vacio);

    pthread_t hilo1;
    pthread_create(&hilo1, NULL, (void*)mediano_plazo_bck, pcb);
    pthread_detach(hilo1);
}

void rta_io_stdout(int socket_io){

    enviar_op_code(OK, socket_io);

    int pid = recibir_pid(socket_io);

    loguear_lista(listasProcesos->bck, logger);

    PCB* pcb = buscar_pcb_por_pid(pid);

    mediano_plazo_rdy (pcb);


    log_info(logger,
        "PID: [%d] Finalizo IO STDOUT",
        pcb->data.PID
    );
}


