
#include "server.h"
#include "utilsKS.h"
#include "../../utils/src/global_utils.h"






int main(int argc, char *argv[])
{

    if(argc != 3){
        printf("Uso: ./bin/kernel_scheduler [archivo.config] [archivoProcesos]\n");
        return 1;
    }


    int err = cliente_Kernel_Scheduler (argc, argv);
    if(err != 0){log_debug(logger, "ERROR al inciar cliente");}
    
    iniciar_listas_suple();
    Iniciar_listas_procesos();

    pthread_mutex_init(&mutex_cpus, NULL);

    //FALTA PONER LOS MUTEX_INIT DE TODOS LOS MUTEX DE LOS ESTADOS
    //PONER EN UNA FUNCION EL DISTROY DE LA LISTA DE CPUS_COENCTADAS


    //no hay logger aca porque cliente y servidor lo comparten (esta en cliente)

    int server_fd = iniciar_servidor(info_km.puerto_km, logger);  /*Ese puerto KM me parece que esta mal*/

    log_info(logger, "Servidor listo para recibir clientes");

    while (scheduler_control_loop == 1) {
        
        int cliente_fd = esperar_cliente(server_fd, logger);
        
        if (cliente_fd == -1) {
            log_error(logger, "Error al aceptar un cliente");
            return -1; 
        }

        log_info(logger, "Nuevo cliente. Creamos el hilo");


        /* -------------CREACIÓN DEL HILO---------- */

        pthread_t hilo_id;
        
        
        if (pthread_create(&hilo_id, NULL, atender_nuevo_cliente, (void*)(intptr_t)cliente_fd) != 0) {
            log_error(logger, "no se pudo crear el hilo");
            close(cliente_fd);
        }
        // &hilo_id: donde se guarda el ID del hilo
        // NULL: configuración por defecto que puso el profe
        // atender_nuevo_cliente: funcion que atiende al cliente
        // el cliente_fd: Lo que le pasamos como argumento
        
    }

    terminar_programa (logger, config, info_km);
}




/*----------------------------------FUNCIONES------------------------------------------*/

/*-----                     GESTION DE NUEVOS CLIENTES                     -----*/

void* atender_nuevo_cliente(void* fd) { /*Funcion que se encarga de atender los HILOS de cuando te conecta un NUEVO CLIENTE*/

    int cliente_fd = (int)(intptr_t)fd; // Recuperamos el FD del cliente
    
    pthread_detach(pthread_self()); //Esto hace que el SO limpie la memoria de este hilo cuando termine la funcion

    int control_loop = 1;
    while (control_loop) { //Este loop funciona de manera tal de que se mantiene CONSTANTEMENTE la comunicacion con el CLIENTE.
        
        int op_code = recibir_op_code(cliente_fd); //syscall bloqueante --> por lo que no se esta haciendo espera activa; es como que el sistema se duerme hasta que reciva 
        if(op_code == -1){
            log_info(logger, "El cliente en el socket %d se desconectó.", cliente_fd);
            control_loop = 0;
            return NULL ;
        }

        

        switch (op_code) {
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

            case gl_IO_SLEEP: {
                io_sleep(cliente_fd); 
                break;}

            case gl_IO_STDIN: {
                io_stdin(cliente_fd); 
                break;}

            case gl_IO_STDOUT: {
                io_stdout(cliente_fd); 
                break;
            }
            case IO_LIBRE: 
                // viene de la io
                // La interfaz de IO nos manda este opcode cuando termina de operar
                io_libre(cliente_fd); 
                break;

            case gl_MUTEX_CREATE:
                mutex_create(cliente_fd);
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
            
            case NUEVO_ESPACIO: /*Cambiar*/
                nuevo_espacio(cliente_fd);
                break;
            
            
            case -1:
                log_info(logger, "El cliente se desconectó.");
                control_loop = 0;
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


/*-----                     CREACION Y DESTRUCCION DE LISTAS                     -----*/

t_listas_procesos* Iniciar_listas_procesos (){ /*Funcion que inicializa todas las listas de los Procesos*/

    listasProcesos = malloc(sizeof(t_listas_procesos));

	listasProcesos->new   = list_create();
	listasProcesos->rnn   = list_create();
	listasProcesos->bck   = list_create();
	listasProcesos->ext   = list_create();
	listasProcesos->rdy   = list_create();
    listasProcesos->s_bck = list_create();
    listasProcesos->s_rdy = list_create();

	return listasProcesos;
};

void terminar_listas_procesos (){ /*Funcion que destruye las listas de los Procesos*/

	list_destroy(listasProcesos->new);
	list_destroy(listasProcesos->rnn);
	list_destroy(listasProcesos->bck);
	list_destroy(listasProcesos->ext);
	list_destroy(listasProcesos->rdy);
    list_destroy(listasProcesos->s_bck);
    list_destroy(listasProcesos->s_rdy);


}

void iniciar_listas_suple()
{
    list_suplementarias = malloc(sizeof(t_listas_suplementarias));

    list_suplementarias->cpu = list_create();
    list_suplementarias->io = list_create();
    list_suplementarias->ms = list_create();
    list_suplementarias->desalojo = list_create();

    lista_bck_io = list_create();
}

void eliminar_listas_suple (){ /* Funcion que destruye las listas de CPUs y IOs (Suplmentarias)*/
    
    list_destroy(list_suplementarias->cpu);
    list_destroy(list_suplementarias->io);
    list_destroy(list_suplementarias->ms);
    list_destroy(list_suplementarias->desalojo);
    list_destroy(lista_bck_io);
    
}


/*-----                     GESTION DE LISTAS DE PCBs                    -----*/

int agregar_proceso_lista (PCB* pcb){ /*Funcion que a AGREGA un PCB a su lista correspondiente segun PCB->ESTADO_ACTUAL.*/
	
    int posicion;

    switch (pcb->estado_pcb){
	case NEW: //NEW
        
        pthread_mutex_lock(&sem_procesos_new);
		posicion = list_add(listasProcesos->new, pcb);
        pthread_mutex_unlock(&sem_procesos_new);
        return posicion; //Devuelve la posicion por si nos sirve para algo en un futuro (no lo hace en agregar lista ready, se podria hacer)

        break;

	case RNN: //RUNNING

        pthread_mutex_lock(&sem_procesos_running);    
		posicion = list_add(listasProcesos->rnn, pcb);
        pthread_mutex_unlock(&sem_procesos_running);
        return posicion; //Devuelve la posicion por si nos sirve para algo en un futuro (no lo hace en agregar lista ready, se podria hacer)

        break;

	case BCK: //BLOCK

        pthread_mutex_lock(&sem_procesos_block);
		posicion = list_add(listasProcesos->bck, pcb);
        pthread_mutex_unlock(&sem_procesos_block);
        return posicion; //Devuelve la posicion por si nos sirve para algo en un futuro (no lo hace en agregar lista ready, se podria hacer)

        break;
    
    case S_BCK: //BLOCK

        pthread_mutex_lock(&sem_procesos_s_block);
		posicion = list_add(listasProcesos->s_bck, pcb);
        pthread_mutex_unlock(&sem_procesos_s_block);
        return posicion; //Devuelve la posicion por si nos sirve para algo en un futuro (no lo hace en agregar lista ready, se podria hacer)

        break;

    case S_RDY: //BLOCK

        pthread_mutex_lock(&sem_procesos_s_ready);
		posicion = list_add(listasProcesos->s_rdy, pcb);
        pthread_mutex_unlock(&sem_procesos_s_ready);
        return posicion; //Devuelve la posicion por si nos sirve para algo en un futuro (no lo hace en agregar lista ready, se podria hacer)

        break;

	case EXT: //EXIT

        pthread_mutex_lock(&sem_procesos_exit);    
		posicion = list_add(listasProcesos->ext, pcb);
        pthread_mutex_unlock(&sem_procesos_exit);
        return posicion; //Devuelve la posicion por si nos sirve para algo en un futuro (no lo hace en agregar lista ready, se podria hacer)

        break;

	case RDY: //RDY

		posicion = agregar_lista_ready(pcb);
        return posicion; //Devuelve la posicion por si nos sirve para algo en un futuro (no lo hace en agregar lista ready, se podria hacer)

        break;
	
	default:
        log_error(logger, "Erro al identificar estado de un pcb (funcion agregar proceso a lista)");
        return -1;
		break;

	}

}
 
op_code eliminar_proceso_Lista(PCB* pcb) { /*/Esta Funcion debe ser llamada dsp de agregar_proceso_lista (PCB* pcb)*/
    bool removed = false; // Inicializamos

    switch (pcb->estado_anterior) {
        case NEW:
            pthread_mutex_lock(&sem_procesos_new);
            removed = list_remove_element(listasProcesos->new, pcb);
            pthread_mutex_unlock(&sem_procesos_new);
            break;
            
        case RNN:
            pthread_mutex_lock(&sem_procesos_running);
            removed = list_remove_element(listasProcesos->rnn, pcb);
            pthread_mutex_unlock(&sem_procesos_running);
            break;

        case BCK:
            pthread_mutex_lock(&sem_procesos_block);
            removed = list_remove_element(listasProcesos->bck, pcb); // Asegura que el nombre coincida con tu struct
            pthread_mutex_unlock(&sem_procesos_block);
            break;

        case EXT:
            pthread_mutex_lock(&sem_procesos_exit);
            removed = list_remove_element(listasProcesos->ext, pcb);
            pthread_mutex_unlock(&sem_procesos_exit);
            break;
        
        case S_BCK:
            pthread_mutex_lock(&sem_procesos_s_block);
            removed = list_remove_element(listasProcesos->s_bck, pcb);
            pthread_mutex_unlock(&sem_procesos_s_block);
            break;
        
        case S_RDY:
            pthread_mutex_lock(&sem_procesos_s_ready);
            removed = list_remove_element(listasProcesos->s_rdy, pcb);
            pthread_mutex_unlock(&sem_procesos_s_ready);
            break;

        case RDY:
            pthread_mutex_lock(&sem_procesos_ready);
            removed = list_remove_element(listasProcesos->rdy, pcb);
            pthread_mutex_unlock(&sem_procesos_ready);
            break;
    
        default:
            log_error(logger, "Error: Estado anterior desconocido.");
            return NOTOK; 
    }

    if (!removed) {
        log_error(logger, "Error al remover PCB de lista");
        return NOTOK;
    }

    return OK; 
}

int agregar_lista_ready(PCB* pcb){ /*Funcion que AGREGA un PCB a la lista de READYS a partir de un ALGORITMO de PLANIFICACION*/

    int posicion;
    
    if (strcmp(planificacion_algoritmo, "FIFO") == 0 || strcmp(planificacion_algoritmo, "RR") == 0) {
        posicion = ready_FIFO(pcb);
    } 
    else if (strcmp(planificacion_algoritmo, "CMN") == 0) {
        posicion = ready_CMN(pcb);
    }
    else {
        log_error (logger, "Error al identificar algoritmo de planificacion (funcion agregar_lista_ready)");
    return -1;
    }
    return posicion;
}


/*-----                     GESTION DE PCBs                     -----*/

void cambiar_estado_pcb(PCB* pcb, estado nuevoEstado){ /*Funcion que cambia el estado de un PCB*/
    
    //Se le podria agregar semaforos pero antes y dsp de llamar a la funcion
    
    pcb -> estado_anterior = (pcb ->estado_pcb);
    pcb ->estado_pcb = nuevoEstado;
    
    log_info (logger, "## PID:[%d] pasa del estado [%d] al estado [%d]",pcb->data.PID,pcb->estado_anterior,pcb->estado_pcb);
}

PCB* buscar_pcb_por_pid(int pid_recibido) { // (Facu): Yo remplazaria esto por la funcion de las commons que permite buscar adentro de una lista
    
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

PCB* encontrar_pcb_rnn_por_pid(int pid) {
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

void  mandar_proceso_cpu(int socket_cliente){ /* Funcion que manda el PCB de mayor priridad a una CPU es especial */
    
    
    pthread_mutex_lock(&mutex_cpus);
    
        /* Buscamos la CPU pasándole la dirección del socket_cliente como contexto */
    t_CPU *cpu_libre = list_find_with_context(list_suplementarias->cpu, es_la_cpu_buscada, &socket_cliente);

    if (cpu_libre != NULL) {
        cpu_libre->enUso = true;}
    else {
        log_error (logger, "No se encontro a la CPU buscada (funcion: mandar_proceso_cpu)");
        pthread_mutex_unlock(&mutex_cpus);
        return;
    }
        pthread_mutex_unlock(&mutex_cpus);

    /*Mandamos el PCB a la CPU*/
    if ((cpu_libre != NULL)) { /*Verifica que exista la CPU libre; Verifica que Haya algun procesos en READY; Verifica que Haya alguna IO*/
        
        PCB* pcb_a_ejecutar = obtener_siguiente_proceso();
        
        if(pcb_a_ejecutar == NULL){
            log_error(logger, "No se pudo obtener PCB READY");
            cpu_libre->enUso = false;
            return;
        }

        cambiar_estado_pcb(pcb_a_ejecutar, RNN);
        pcb_a_ejecutar->fd_cpu = cpu_libre->fd;
        agregar_proceso_lista(pcb_a_ejecutar); //ESTAS FUNCIONES YA TIENEN EL MUTEX DENTRO
        
        

        int err = enviar_pid (pcb_a_ejecutar->data.PID, cpu_libre->fd); //Envia el PID a la CPU
        if (err != 1) {
            log_error (logger, "Error al enviar pcb a cpu libre (funcion: mandar_proceso_cpu)"); // Completar log de error
            cpu_libre->enUso = false;        
            return;}

        log_info(logger, "PID %d enviado a ejecutar en socket %d", pcb_a_ejecutar->data.PID, cpu_libre->fd);

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

bool es_la_cpu_buscada (void* elemento, void* contexto) {
    
        t_CPU* cpu = (t_CPU*) elemento;
    
        // Casteamos el contexto de vuelta a un puntero de int para sacar el socket
     int socket_buscado = *(int*) contexto; 
    
    return (cpu->fd == socket_buscado) && (cpu->enUso == false);
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
            "## PID:[%d] - Desalojado por Fin de Quantum",
            pcb->data.PID
        );
    }

    free(datos);

    return NULL;
}

PCB* obtener_siguiente_proceso() {
    
    PCB* pcb = NULL;

    pthread_mutex_lock(&mutex_ready);

    if(strcmp(planificacion_algoritmo, "FIFO") == 0){
        pcb = list_remove(listasProcesos->rdy, 0);
    }
    else if(strcmp(planificacion_algoritmo, "RR") == 0){
        pcb = list_remove(listasProcesos->rdy, 0);
    }
    else if(strcmp(planificacion_algoritmo, "CMN") == 0){
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

int ready_FIFO(PCB* pcb_nuevo) { /*Funcion que a partir del ALGORITMO FIFO agrega un PCB a LISTA DE READYS ordenando por PRIORIDAD*/
    
    int posicion;

    pthread_mutex_lock(&mutex_ready);
    
    posicion = list_add(listasProcesos -> rdy, pcb_nuevo); // Lo pones al final de la lista
    
    pthread_mutex_unlock(&mutex_ready);
    
    return posicion;
}

int ready_CMN(PCB* pcb_nuevo ){
    int nivel = pcb_nuevo->data.prioridad;

    if(nivel < 0 || nivel >= planificador->cantidad_niveles)
    {
        log_error(logger, "Prioridad invalida: %d", nivel);
        return -1;
    }

    ColaPrioridad* cola_apuntada = &planificador->niveles[nivel];

    pthread_mutex_lock(&mutex_ready);
    list_add(cola_apuntada->cola, pcb_nuevo);
    pthread_mutex_unlock(&mutex_ready);



    if(planificador->preemption)
    {
        verificar_desalojo_por_prioridad(pcb_nuevo);
    }

    return nivel;
}

void verificar_desalojo_por_prioridad(PCB* pcb_nuevo)
{
    pthread_mutex_lock(&sem_procesos_running);

    for(int i = 0; i < list_size(listasProcesos->rnn); i++)
    {
        PCB* pcb_running = list_get(listasProcesos->rnn, i);

        if(pcb_nuevo->data.prioridad < pcb_running->data.prioridad)
        {
            pthread_mutex_lock(&sem_procesos_s_desalojo);
            list_add(list_suplementarias->desalojo, pcb_nuevo);
            pthread_mutex_unlock(&sem_procesos_s_desalojo);

            log_info(
                logger,
                "PID %d desalojado por ingreso de PID %d con mayor prioridad",
                pcb_running->data.PID,
                pcb_nuevo->data.PID
            );
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

void mediano_plazo_bck(PCB* pcb){
    /*----- Mediano Plazo -----*/
    if(pcb->estado_pcb != BCK){return;}

    usleep(info_config.tiempo_suspencion);

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
    
    return (io->fd == socket_buscado) && (io->enUso == false);
}

/*----                     OP_CODES                     -----*/

/*-----Con la CPU-----*/
	
//NUEVA_CPU,
void nueva_cpu (int cliente_fd) {

        t_CPU* info_cpu = malloc(sizeof(t_CPU));

        info_cpu->fd = cliente_fd;         
        info_cpu->enUso = false;    
                    
        pthread_mutex_lock(&mutex_cpus);
        list_add(list_suplementarias->cpu, info_cpu);
        pthread_mutex_unlock(&mutex_cpus);
        
        enviar_op_code (OK, cliente_fd);

        log_info(logger, "## CPU ID: [%d] Conectada", cliente_fd); /*Logger Obligatorio*/
        }

//CPU_LIBRE,
void cpu_libre (int cliente_fd){

    while (compactacion_value == 1){
        usleep(1000);
    }
    mandar_proceso_cpu(cliente_fd);
}

//DESALOJO
void desalojo (int socket_cliente){
    
    int pid = recibir_pid(socket_cliente);
    char* cpu_id = recibir_mensaje(socket_cliente, logger);
    op_code err = OK;
    if(mem_corrupt_value == 1){
        
        enviar_op_code(MEM_CORRUPT, socket_cliente);
        log_info(logger, "## Se solicito desalojar el PID:[%d] que se encuentra ejecutando en la CPU:[%s]",pid,cpu_id);

    }
    else if (compactacion_value == 1) {
        
        enviar_op_code(COMPACTACION, socket_cliente);
        log_info(logger, "## Se solicito desalojar el PID:[%d] que se encuentra ejecutando en la CPU:[%s]",pid,cpu_id);

              
    }
    else if (existe_pcb_con_pid(list_suplementarias->desalojo,pid)){
        
        enviar_op_code(DESALOJO, socket_cliente);
        log_info(logger, "## Se solicito desalojar el PID:[%d] que se encuentra ejecutando en la CPU:[%s]",pid,cpu_id);
        
        pthread_mutex_lock(&sem_procesos_s_desalojo);
        sacar_pcb_por_pid(list_suplementarias->desalojo,pid);
        pthread_mutex_unlock(&sem_procesos_s_desalojo);
    }
    else {
        enviar_op_code(OK,socket_cliente);
    }

    err = recibir_op_code(socket_cliente);
    if (err == OK){

        PCB* pcb = buscar_pcb_por_pid(pid);
        
        if(pcb->estado_pcb == RNN){

            cambiar_estado_pcb(pcb,RDY);

            if (compactacion_value == 1){
                if (strcmp(info_config.planificacion_algoritmo, "CNM") == 0){
                    actualizar_prioridad_pcb(pcb,0);
                }
                
                pthread_mutex_lock(&mutex_ready);
                list_add_in_index(listasProcesos->rdy, 0, pcb);
                pthread_mutex_lock(&mutex_ready);
            }
            else {
                agregar_proceso_lista(pcb);
            }
        
            eliminar_proceso_Lista(pcb);
        }
        

        log_info(logger,"Proceso Desalojado PID:[%d] de CPU:[%s]",pid,cpu_id);
    }

    
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

    enviar_op_code (OK, cliente_fd);
                
    log_info(logger, "IO '%s' registrada en el socket %d", info_io->nombre, cliente_fd);
}

// IO LIBRE
void io_libre(int io_socket){ //Copia de atender CPU

    pthread_mutex_lock(&mutex_ios);
        /* Buscamos la CPU pasándole la dirección del socket_cliente como contexto */
    t_IO *io_libre = list_find_with_context(list_suplementarias->io, es_la_cpu_buscada, &io_socket);

    if (io_libre != NULL) {
        io_libre->enUso = true;}
    else {
        log_error (logger, "No se encontro a la IO buscada (funcion: io_libre)");
        return;
    }
    pthread_mutex_unlock(&mutex_cpus);

    /*Mandamos el PCB a la IO*/
    if ((io_libre != NULL) && (!list_is_empty(lista_bck_io)) && (!list_is_empty(list_suplementarias->io))) { 
        
        espera_io* pcb_a_ejecutar = list_get(lista_bck_io, 0);

        if(pcb_a_ejecutar->io_op_code == IO_SLEEP){
            enviar_op_code(gl_IO_SLEEP, io_socket);
            if(recibir_op_code(io_socket) != OK) return;

                t_paquete* paquete_para_io = crear_paquete(gl_IO_SLEEP); 
                agregar_a_paquete(paquete_para_io,&pcb_a_ejecutar->pid,sizeof(uint32_t));
                agregar_a_paquete(paquete_para_io,&pcb_a_ejecutar->sleep.time,sizeof(uint32_t));        
                
                enviar_paquete(paquete_para_io, io_socket);
                free(paquete_para_io);

        }
        else if (pcb_a_ejecutar->io_op_code == gl_IO_STDIN){
            enviar_op_code(gl_IO_STDIN, io_socket);
            if(recibir_op_code(io_socket) != OK) return;

            t_paquete* paquete_io = crear_paquete(gl_IO_STDIN);

            agregar_a_paquete(paquete_io, &pcb_a_ejecutar->pid, sizeof(uint32_t));
            agregar_a_paquete(paquete_io, &pcb_a_ejecutar->iostdin.direc, sizeof(uint32_t));
            agregar_a_paquete(paquete_io, &pcb_a_ejecutar->iostdin.length, sizeof(uint32_t));
            
            enviar_paquete(paquete_io, io_socket);
            free(paquete_io);
        }
        else if (pcb_a_ejecutar->io_op_code == gl_IO_STDOUT){
            enviar_op_code(gl_IO_STDOUT, io_socket);
            if(recibir_op_code(io_socket) != OK) return;

            t_paquete* paquete_io = crear_paquete(gl_IO_STDOUT);
    
            agregar_a_paquete(paquete_io, &pcb_a_ejecutar->pid, sizeof(uint32_t));
            agregar_a_paquete(paquete_io, &pcb_a_ejecutar->iostdout.length, sizeof(uint32_t));
            agregar_a_paquete(paquete_io, &pcb_a_ejecutar->iostdout.info, sizeof(pcb_a_ejecutar->iostdout.info)); // Los datos que vinieron de memoria
            
            enviar_paquete(paquete_io, io_socket);
            eliminar_paquete(paquete_io);

        }
        else {
            log_info(logger, "Error al identificar IO de PID:[%d] (funcion: io_libre)",pcb_a_ejecutar->pid);
        }

        if(recibir_op_code(io_socket) != OK){
            log_info (logger, "Error al enviar info sobre IO PID[%d] (funcion: io_libre)", pcb_a_ejecutar->pid);
        }
    
}}


/*-----Con el Kernel Memory-----*/

//MEM_CORRUPT
void mem_corrupt (int socket_cliente){ 

    mem_corrupt_value = 1;
    log_info(logger,"## Se Desalojaran todas las CPUs por Mem Corrupt");
    //madnar a km el ok cuando se desalojó
    if (mem_corrupt_value == 1){
        while (!list_is_empty(listasProcesos->rnn)){
                usleep(1000);
            }
        
        log_info(logger, "Blue Screen");  
        scheduler_control_loop = 0; //Apaga el Kernel Scheduler
        return;
    }

} 

//COMPACTACION
void compactacion (int socket_cliente){
    int err = 0;
    compactacion_value = 1;
    control_compac = 1;
    log_info(logger,"## Se Desalojaran todas las CPUs por Compactacion");

    while (!list_is_empty(listasProcesos->rnn)){
            usleep(1000);
        }

        enviar_op_code(CPUS_DESALOJADAS_OK,info_km.conexion_km);
        err = recibir_op_code(info_km.conexion_km);
        if (err == COMPACTACION_FINALIZADA){
            compactacion_value = 0;
            nuevo_espacio();
        }
        else {
            log_info(logger, "## error en resolver compactacion");
            return;
        }  
}

//NUEVA_MEMORY_STICK
void recibir_nueva_memory_stick(int socket_km)
{
    int size;

    void* buffer = recibir_buffer(&size, socket_km);


    t_mem_stick* ms = malloc(sizeof(t_mem_stick));

    int offset = 0;


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

//NUEVO_ESPACIO
void nuevo_espacio(){/*HACER y Pensar mejor*/

    PCB* pcb = NULL;

   while(1){ 

    if(list_is_empty(listasProcesos->s_rdy)){
        
        log_info(logger, "La lista de s_rdy esta vacia");
        return;
    }

    if(strcmp(info_config.planificacion_algoritmo, "CNM") == 0){

        for(int prioridad = 1; prioridad <= planificador->cantidad_niveles && pcb == NULL; prioridad++)
    {
        for(int i = 0; i < list_size(listasProcesos->s_rdy); i++)
        {
            PCB* aux = list_get(listasProcesos->s_rdy, i);

            if(aux->data.prioridad == prioridad)
            {
                pthread_mutex_lock(&sem_procesos_s_ready);
                pcb = (PCB*) list_remove(listasProcesos->s_rdy, i);
                pthread_mutex_unlock(&sem_procesos_s_ready);
                break;
            }
        }
    }
    }
    
    else{

        pthread_mutex_lock(&sem_procesos_s_ready);
        pcb = (PCB*)list_remove(listasProcesos->s_rdy,0);
        pthread_mutex_unlock(&sem_procesos_s_ready);

    }

    if(pcb == NULL) {   
        log_info(logger,"Error a encontrar en s_rdy PID:[%d]",pcb->data.PID);
        return;
    }

    enviar_op_code(NUEVO_ESPACIO,info_km.conexion_km);
    enviar_pid(pcb->data.PID, info_km.conexion_km);

    int err = recibir_op_code(info_km.conexion_km);
    if(err != OK){
        log_info(logger,"Espacio Insuficiente para PID:[%d]",pcb->data.PID);
        return; 
    }

    log_info(logger, "Exito al Desuspeder proceso PID: [%d]", pcb->data.PID);

    cambiar_estado_pcb(pcb, RDY);
    agregar_proceso_lista(pcb);
    eliminar_proceso_Lista(pcb);
    }

    
    

    return;
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
                "CPU %d recibió nueva Memory Stick",
                cpu->identificador
            );
        }
        else
        {
            log_error(logger,
                "CPU %d no confirmó Memory Stick",
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

PCB* sacar_pcb_por_pid(t_list* lista, int pid) {
    pid_buscado = pid;
    return list_remove_by_condition(lista, tiene_pid);
}


/*-----                     MOCKs                     -----*/

void enviar_proceso_finalizar_KM_mock (int pid) {
    
    log_info(logger, " Enviado a KM, PID: %u", pid);

}

void data_io_stdout_mock(espera_io* io_pcb, PCB* pcb, uint32_t tam) { /*Siempre devuelve 5555 como DATA*/

    io_pcb->pid = pcb->data.PID;
    io_pcb->io_op_code = IO_STDOUT;
    io_pcb->iostdout.length = tam;
    io_pcb->iostdout.info = "5555";

} 


/*-----     Syscalls CPU     -----*/

//MUTEX_CREATE,
void mutex_create (int socket_cliente){

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

    list_add(list_suplementarias->desalojo, pcb);

    enviar_op_code(OK, socket_cliente);
        
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
        

        if (mutex == NULL) {
        log_error(logger, "Mutex no encontrado");
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

        list_add(list_suplementarias->desalojo, pcb);

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
void mutex_unlock (int socket_cliente){

        enviar_op_code(OK,socket_cliente);

        int pid = recibir_pid(socket_cliente);
        char* mutex_id = recibir_mensaje (socket_cliente, logger);
        log_info(logger, "## PID:[%d] Solicito Syscall: [Mutex Unlock]", pid); /*Logger Obligatorio*/

        /*Bloqueo y Desalojo*/
        PCB* pcb = buscar_pcb_por_pid(pid);
        cambiar_estado_pcb(pcb,BCK);
        agregar_proceso_lista(pcb);
        eliminar_proceso_Lista(pcb);

        list_add(list_suplementarias->desalojo, pcb);

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

//MEM_ALLOC,
void mem_alloc (int socket_cliente){
    
    enviar_op_code(OK,socket_cliente); //HandShake

    char* id_segmento = recibir_mensaje(socket_cliente, logger);
    enviar_op_code(OK,socket_cliente); 

    char* tamanio = recibir_mensaje(socket_cliente,logger);
    enviar_op_code(OK,socket_cliente);


    int pid = recibir_pid(socket_cliente);
    enviar_op_code(OK, socket_cliente);

    log_info(logger, "## PID:[%d] Solicito Syscall: [Mem Alloc", pid); /*Logger Obligatorio*/

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

    log_info(logger, "## PID:[%d] Solicito Syscall: [Mutex Free]", pid); /*Logger Obligatorio*/

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

    list_add(list_suplementarias->desalojo, pcb);

    enviar_op_code(OK,socket_cliente);

    log_info(logger, "Solicitud INIT_PROC: %s (Prioridad: %d)", path, prioridad);

    PCB* nuevo_pcb; 
    if(!mock){
        
        nuevo_pcb = crearNuevoProceso(path, prioridad, info_km.conexion_km);
        
        if (recibir_op_code(info_km.conexion_km) == OK) {
                cambiar_estado_pcb(nuevo_pcb, RDY);
                agregar_proceso_lista (nuevo_pcb);           
        }
    
    }
    else{
        
        nuevo_pcb = crearNuevoProceso_mock(path, prioridad, info_km.conexion_km);
        cambiar_estado_pcb(nuevo_pcb, RDY);
        agregar_proceso_lista (nuevo_pcb);    
    }

    cambiar_estado_pcb(pcb,RDY);
    agregar_proceso_lista(pcb);
    eliminar_proceso_Lista(pcb);

    list_destroy(lista);
    
                
}

//EXIT
void exit_proceso(int socket_cpu){

    t_list* lista = recibir_paquete(socket_cpu);
    int pid_a_finalizar = *(int*)list_get(lista, 0);
    list_destroy(lista);

    /*Bloqueo y Desalojo*/
    PCB* pcb = buscar_pcb_por_pid(pid_a_finalizar);
    cambiar_estado_pcb(pcb,BCK);
    agregar_proceso_lista(pcb);
    eliminar_proceso_Lista(pcb);

    list_add(list_suplementarias->desalojo, pcb);
    enviar_op_code(OK, socket_cpu);

    log_info(logger, "## PID:[%d] Solicito Syscall: [Exit]", pid_a_finalizar); /*Logger Obligatorio*/

    log_info(logger, "Finalizando proceso PID: %d", pid_a_finalizar);

    if(!mock){enviar_proceso_finalizar_KM(pid_a_finalizar);}
    else{enviar_proceso_finalizar_KM_mock(pid_a_finalizar);}
 
    
    if (pcb != NULL) {
        cambiar_estado_pcb(pcb, EXT);
        agregar_proceso_lista (pcb);
        eliminar_proceso_Lista(pcb);
    }

    log_info (logger, "## PID:[%d] Finalizo su ejecucion con motivo de [Fin de proceso]",pid_a_finalizar);/*Logger Obligatorio*/
    
    nuevo_espacio();
}

//SLEEP
void io_sleep(int socket_cpu) { 
    /*Recibimos la info necesaria*/
    t_list* lista = recibir_paquete(socket_cpu);
    int pid_a_bloquear = *(int*)list_get(lista, 0);
    char* tiempo_str = (char*)list_get(lista, 1);
    int tiempo_ms = atoi(tiempo_str); // Si viene como string, convertimos
    list_destroy(lista);

    log_info(logger, "## PID:[%d] Solicito Syscall: [Sleep]", pid_a_bloquear); /*Logger Obligatorio*/

    PCB* pcb = buscar_pcb_por_pid(pid_a_bloquear);

    
    if (pcb != NULL) {
        
        /*Bloqueamos el Proceso*/
        cambiar_estado_pcb(pcb, BCK);
        agregar_proceso_lista(pcb);
        eliminar_proceso_Lista(pcb);

        list_add(list_suplementarias->desalojo, pcb);
        
        /**/
        espera_io* io_pcb = malloc(sizeof(espera_io));

        io_pcb->pid = pcb->data.PID;
        io_pcb->io_op_code = IO_SLEEP;
        io_pcb->sleep.time = tiempo_ms;

        pthread_mutex_lock(&mutex_ios);
        list_add(lista_bck_io, io_pcb);
        pthread_mutex_unlock(&mutex_ios);
        
        
    } else {
        log_error(logger, "PID %d no encontrado en EXEC", pid_a_bloquear);
        enviar_op_code(NOTOK, socket_cpu);
    }

    mediano_plazo_rdy(pcb);
}

void rta_io_sleep(int socket_io){ //Funcion que recibe desde IO que finalizo un SLEEP de un proceso

    enviar_op_code(OK, socket_io);
    int pid = recibir_pid(socket_io);

    espera_io* io_pcb = encontrar_pid_io_bck(pid);

    if(!list_remove_element(lista_bck_io,io_pcb)){
        log_info(logger, "Error al encontar el io_pcb buscado e la lista de bloqueados (funcion: rta io sleep)");
        return;
    }

    free(io_pcb);

    PCB* pcb = buscar_pcb_por_pid(io_pcb->pid);

    mediano_plazo_rdy(pcb);

    log_info (logger, "PID: [%d] Finalizo IO SLEEP", pcb->data.PID);


}

// STDIN
void io_stdin(int socket_cpu) {
    
    /*Recibimos la info necesaria*/
    t_list* lista = recibir_paquete(socket_cpu);
    uint32_t tam = *(uint32_t*)list_get(lista, 0);
    uint32_t dir = *(uint32_t*)list_get(lista, 1);
    uint32_t pid_a_bloquear = *(uint32_t*)list_get(lista, 2);
    list_destroy(lista);


    PCB* pcb = buscar_pcb_por_pid(pid_a_bloquear);
    
    log_info(logger, "## PID:[%d] Solicito Syscall: [Stdin]", pid_a_bloquear); /*Logger Obligatorio*/

    if (pcb != NULL) {
        
        /*Bloqueamos el Proceso*/
        cambiar_estado_pcb(pcb, BCK);
        agregar_proceso_lista(pcb);
        eliminar_proceso_Lista(pcb);

        list_add(list_suplementarias->desalojo, pcb);
        
        /**/
        espera_io* io_pcb = malloc(sizeof(espera_io));

        io_pcb->pid = pcb->data.PID;
        io_pcb->io_op_code = IO_STDIN;
        io_pcb->iostdin.direc = dir;
        io_pcb->iostdin.length = tam;

        pthread_mutex_lock(&mutex_ios);
        list_add(lista_bck_io, io_pcb);
        pthread_mutex_unlock(&mutex_ios);
        
    } else {
        log_error(logger, "PID %d no encontrado en EXEC", pid_a_bloquear);
        enviar_op_code(NOTOK, socket_cpu);
    }

    mediano_plazo_bck(pcb);
}    

void rta_io_stdin (int socket_io){

    enviar_op_code(OK, socket_io);
    
    t_list* lista = recibir_paquete(socket_io);  
    uint32_t direccion_logica = *(uint32_t*)list_get(lista, 0);
    uint32_t tam_datos = *(uint32_t*)list_get(lista, 1);    
    void* datos_recibidos = list_get(lista, 2);
    int pid = *(int*)list_get(lista,3);

    free(lista);

    espera_io* io_pcb = encontrar_pid_io_bck(pid);

    if(!list_remove_element(lista_bck_io,io_pcb)){
        log_info(logger, "Error al encontar el io_pcb buscado e la lista de bloqueados (funcion: rta io stdin)");
        return;
    }

    free(io_pcb);

    if(!mock){
        
        enviar_op_code(km_IO_STDIN, info_km.conexion_km);

        if(recibir_op_code(info_km.conexion_km)!= OK){
        log_info(logger, "Error al enviar STDIN a KM");
        return;
        }

        t_paquete* paquete_mem = crear_paquete(km_IO_STDIN);
        agregar_a_paquete(paquete_mem, &direccion_logica, sizeof(uint32_t));
        agregar_a_paquete(paquete_mem, &tam_datos, sizeof(uint32_t));
        agregar_a_paquete(paquete_mem, &datos_recibidos, sizeof(datos_recibidos));
        agregar_a_paquete(paquete_mem, &pid, sizeof(int));

        enviar_paquete(paquete_mem, info_km.conexion_km);

        eliminar_paquete(paquete_mem);

    }

    PCB* pcb = buscar_pcb_por_pid(io_pcb->pid);

    mediano_plazo_rdy(pcb);

    log_info (logger, "PID: [%d] Finalizo IO SLEEP", pcb->data.PID);

    log_info (logger, "PID: [%d] Finalizo IO STDIN", pcb->data.PID);
    
}

// STDOUT
void io_stdout(int cpu_socket) {
    
    t_list* lista = recibir_paquete(cpu_socket);
    
    // deserializar lo que viene de la CPU
    uint32_t pid = *(uint32_t*)list_get(lista, 0);
    uint32_t dir = *(uint32_t*)list_get(lista, 1);
    uint32_t tam = *(uint32_t*)list_get(lista, 2);
   
    log_info(logger, "## PID:[%d] Solicito Syscall: [Stdout]", pid); /*Logger Obligatorio*/

    
    PCB* pcb = buscar_pcb_por_pid(pid);
    
    /*Bloqueamos el Proceso*/
    cambiar_estado_pcb(pcb, BCK);
    agregar_proceso_lista(pcb);
    eliminar_proceso_Lista(pcb);

    list_add(list_suplementarias->desalojo, pcb);

    espera_io* io_pcb = NULL;
    if(!mock){
        /*Le solicitamos los Datos de la KM*/
        enviar_op_code(km_IO_STDOUT, info_km.conexion_km);

        t_paquete* req_mem = crear_paquete(km_IO_STDOUT);
        agregar_a_paquete(req_mem, &pid, sizeof(uint32_t));
        agregar_a_paquete(req_mem, &dir, sizeof(uint32_t));
        agregar_a_paquete(req_mem, &tam, sizeof(uint32_t));
        enviar_paquete(req_mem, info_km.conexion_km); 
        eliminar_paquete(req_mem);

        // recibir de km
        t_list* lista_mem = recibir_paquete(info_km.conexion_km);
        char* datos_leidos = (char*)list_get(lista_mem, 0);

        espera_io* io_pcb = malloc(sizeof(espera_io));

        io_pcb->pid = pcb->data.PID;
        io_pcb->io_op_code = IO_STDOUT;
        io_pcb->iostdout.length = tam;
        io_pcb->iostdout.info = datos_leidos;

    }
    else{data_io_stdout_mock(io_pcb, pcb, tam);}
    
        pthread_mutex_lock(&mutex_ios);
        list_add(lista_bck_io, io_pcb);
        pthread_mutex_unlock(&mutex_ios);

    mediano_plazo_bck(pcb);
}

void rta_io_stdout(int socket_io){

    enviar_op_code(OK, socket_io);
    int pid = recibir_pid(socket_io);

    espera_io* io_pcb = encontrar_pid_io_bck(pid);

    if(!list_remove_element(lista_bck_io,io_pcb)){
        log_info(logger, "Error al encontar el io_pcb buscado e la lista de bloqueados (funcion: rta io stdout)");
        return;
    }

    free(io_pcb);

    PCB* pcb = buscar_pcb_por_pid(io_pcb->pid);

    mediano_plazo_rdy(pcb);
      
    log_info (logger, "PID: [%d] Finalizo IO SLEEP", pcb->data.PID);

    log_info (logger, "PID: [%d] Finalizo IO stdout", pcb->data.PID);
}


