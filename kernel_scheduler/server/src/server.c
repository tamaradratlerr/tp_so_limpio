#include "server.h"



int main(void) {
    pthread_mutex_init(&mutex_cpus, NULL);

    //FALTA PONER LOS MUTEX_INIT DE TODOS LOS MUTEX DE LOS ESTADOS
    //PONER EN UNA FUNCION EL DISTROY DE LA LISTA DE CPUS_COENCTADAS

    logger = log_create("log.log", "Servidor", 1, LOG_LEVEL_DEBUG);
    
    int server_fd = iniciar_servidor(); 
    log_info(logger, "Servidor listo para recibir clientes");

    while (1) {
        
        int cliente_fd = esperar_cliente(server_fd);
        
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




    return EXIT_SUCCESS;
}

t_conexion conexion;


/*----------------------------------FUNCIONES------------------------------------------*/

/*-----                     GESTION DE NUEVOS CLIENTES                     -----*/


void* atender_nuevo_cliente(void* fd) { /*Funcion que se encarga de atender los HILOS de cuando te conecta un NUEVO CLIENTE*/

    int cliente_fd = (int)(intptr_t)fd; // Recuperamos el FD del cliente
    
    pthread_detach(pthread_self()); //Esto hace que el SO limpie la memoria de este hilo cuando termine la funcion

    t_list* lista = NULL; 
    int control_loop = 1;
    while (control_loop) { //Este loop funciona de manera tal de que se mantiene CONSTANTEMENTE la comunicacion con el CLIENTE.
        
        int op_code = recibir_operacion(cliente_fd); //syscall bloqueante --> por lo que no se esta haciendo espera activa; es como que el sistema se duerme hasta que reciva 
        if(op_code == -1){
            log_info(logger, "El cliente en el socket %d se desconectó.", cliente_fd);
            control_loop = 0;
            return -1;
        }

        

        switch (op_code) {
            case NUEVA_CPU:
                nueva_cpu(cliente_fd);
                break;

            case CPU_LIBRE:
                mandar_proceso_cpu(cliente_fd);
                break;

            case NUEVA_IO:
                // Se conecta una interfaz de IO al Kernel por primera vez
                nueva_io(cliente_fd); 
                break;

            case ks_SLEEP: 
               
                sleep(cliente_fd); 
                break;

            case ks_IO_STDIN: 
                // viene de la cpu 
                lista = recibir_paquete(cliente_fd); 
                io_stdin(lista); 
                list_destroy_and_destroy_elements(lista, free);
                break;

            case ks_IO_STDOUT: 
                lista = recibir_paquete(cliente_fd); 
                atender_io_stdout(lista); 
                list_destroy_and_destroy_elements(lista, free);
                break;

            case IO_LIBRE: 
                // viene de la io
                // La interfaz de IO nos manda este opcode cuando termina de operar
                io_libre(cliente_fd); 
                break;

            case MUTEX_CREATE:
                mutex_create(cliente_fd);
                break;            
           
            case ks_INIT_PROC:
                init_proc(cliente_fd);
                break;
                        
            case ks_EXIT:
                exit_proceso(cliente_fd);
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
    list_destroy_and_destroy_elements(lista, free);
    return NULL;
}




/*-----                     CREACION Y DESTRUCCION DE LISTAS                     -----*/


listas_procesos* Iniciar_listas_procesos (void){ /*Funcion que inicializa todas las listas de los Procesos*/

	listasProcesos->new = list_create();
	listasProcesos->rnn = list_create();
	listasProcesos->bck = list_create();
	listasProcesos->ext = list_create();
	listasProcesos->rdy = list_create();

	return listasProcesos;
};

void terminar_listas_procesos (){ /*Funcion que destruye las listas de los Procesos*/

	list_destroy(listasProcesos->new);
	list_destroy(listasProcesos->rnn);
	list_destroy(listasProcesos->bck);
	list_destroy(listasProcesos->ext);
	list_destroy(listasProcesos->rdy);

	return 0;
}

void iniciar_listas_suple (){ /*Funcion que inicializa las listas de CPUs y IOs (Suplementarias)*/
    
    list_suplementarias->cpu = list_create();
    list_suplementarias->io = list_create();

    return 0;
}

void eliminar_listas_suple (){ /* Funcion que destruye las listas de CPUs y IOs (Suplmentarias)*/
    
    list_destroy(list_suplementarias->cpu);
    list_destroy(list_suplementarias->io);
    
    return 0;
}




/*-----                     GESTION DE LISTAS                     -----*/


void agregar_proceso_lista (PCB* pcb){ /*Funcion que a AGREGA un PCB a su lista correspondiente segun PCB->ESTADO_ACTUAL.*/

	sacar_proceso_lista();//Funcion pensada para tomar estado anterior y sacarlo de esa lista.
	
    switch (pcb->estado_pcb){
	case NEW: //NEW
        
        pthread_mutex_lock(&sem_procesos_new);
		list_add(listasProcesos->new, pcb);
        pthread_mutex_unlock(&sem_procesos_new);
        break;

	case RNN: //RUNNING

        pthread_mutex_lock(&sem_procesos_running);    
		list_add(listasProcesos->rnn, pcb);
        pthread_mutex_unlock(&sem_procesos_running);
        break;

	case BCK: //BLOCK

        pthread_mutex_lock(&sem_procesos_block);
		list_add(listasProcesos->bck, pcb);
        pthread_mutex_unlock(&sem_procesos_block);
        break;

	case EXT: //EXIT

        pthread_mutex_lock(&sem_procesos_exit);    
		list_add(listasProcesos->ext, pcb);
        pthread_mutex_unlock(&sem_procesos_exit);
        break;

	case RDY: //RDY

        //por ahora no agrego los semaforos pq hay que analizar si se hace dentro o fuera de la funcion
		agregar_lista_ready(pcb);
	
        break;
	
	default:
        return -1;
		break;

        return 0;
	}

}

//Esta Funcion debe ser llamada dsp de agregar_proceso_lista (PCB* pcb) 
void eliminar_proceso_Lista (PCB* pcb ){/*Funcion que ELIMINA un PCB de una lista correspondiente segun PCB->ESTADO_ANTERIOR*/

    bool removed;
    switch (pcb->estado_anterior)
	{
	
        case NEW: //NEW
        pthread_mutex_lock(&sem_procesos_new);
		removed = list_remove_element(listasProcesos->new, pcb);
        pthread_mutex_unlock(&sem_procesos_new);
        break;
        	
        case RNN: //RUNNING
        pthread_mutex_lock(&sem_procesos_running);
		removed = list_remove_element(listasProcesos->rnn, pcb);
        pthread_mutex_unlock(&sem_procesos_running);
        break;

        case BCK: //BLOCK
        pthread_mutex_lock(&sem_procesos_block);
		removed = list_remove_element(listasProcesos->bck, pcb);
        pthread_mutex_unlock(&sem_procesos_block);
        break;

        case EXT: //EXIT
        pthread_mutex_lock(&sem_procesos_exit);
		removed = list_remove_element(listasProcesos->ext, pcb);
        pthread_mutex_unlock(&sem_procesos_exit);
        break;
        
        case RDY: //RDY
        pthread_mutex_lock(&sem_procesos_ready);
		removed = list_remove_element(listasProcesos->rdy, pcb);
        pthread_mutex_unlock(&sem_procesos_ready);
		break;
	
	default:
        //Erro en valor del PCB//
		break;
	}
    if (!remove){
        return -1;
    }
    return 0;
    
    /*list_remove_element:: Devuelve TRUE si el elemento fue removido y Devuelve FALSE si no fue encontrado el elemento*/
}   

void agregar_lista_ready(PCB* pcb){ /*Funcion que AGREGA un PCB a la lista de READYS a partir de un ALGORITMO de PLANIFICACION*/

    
    if (strcmp(planificacion_algoritmo, "FIFO") == 0) {
        ready_FIFO(pcb);
    } 
    else if (strcmp(planificacion_algoritmo, "RR") == 0) {
        ready_FIFO(pcb); // ready_RR(pcb);
    } 
    else if (strcmp(planificacion_algoritmo, "CNM") == 0) {
        // ready_CNM(pcb)

    }
    else {
    return -1;
    }
    return 0;
}

void ready_FIFO(PCB* pcb_nuevo) { /*Funcion que a partir del ALGORITMO FIFO agrega un PCB a LISTA DE READYS ordenando por PRIORIDAD*/

    cambiar_estado_pcb(pcb_nuevo, RDY);  
 
    pthread_mutex_lock(&mutex_ready);
    
    list_add(listasProcesos -> rdy, pcb_nuevo); // Lo pones al final de la lista
    
    pthread_mutex_unlock(&mutex_ready); 
}






/*-----                     GESTION DE PCBs                     -----*/


void cambiar_estado_pcb(PCB* pcb, estado nuevoEstado){ /*Funcion que cambia el estado de un PCB*/
    
    //Se le podria agregar semaforos pero antes y dsp de llamar a la funcion
    
    pcb -> estado_anterior = (pcb ->estado_pcb);
    pcb ->estado_pcb = nuevoEstado;
}

int enviar_pcb(int PCB_ID, int socket_cliente){ /* Funcion que manda PCB a un cliente */
	
    t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = PCB_DATA;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = sizeof(int);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, PCB_ID, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);

    return 1;
}


/*-----                     GESTION DE CPUs                     -----*/


void mandar_proceso_cpu(int socket_cliente){ /* Funcion que manda el PCB de mayor priridad a una CPU es especial */
    
    
    pthread_mutex_lock(&mutex_cpus);
    
        /* Buscamos la CPU pasándole la dirección del socket_cliente como contexto */
    CPU *cpu_libre = list_find_with_context(list_suplementarias->cpu, es_la_cpu_buscada, &socket_cliente);

    if (cpu_libre != NULL) {
        cpu_libre->enUso = true;
    
    pthread_mutex_unlock(&mutex_cpus);

    /*Mandamos el PCB a la CPU*/
    if ((cpu_libre != NULL) && (!list_is_empty(listasProcesos->rdy)) && (!list_is_empty(list_suplementarias->io))) { /*Verifica que exista la CPU libre; Verifica que Haya algun procesos en READY; Verifica que Haya alguna IO*/
        
        PCB* pcb_a_ejecutar = list_get(listasProcesos->rdy, 0);
        
        //¿agregar mutex aca?
        cambiar_estado_pcb(pcb_a_ejecutar, RNN);
        //¿agregar mutex aca?

        agregar_proceso_lista(pcb_a_ejecutar); //ESTAS FUNCIONES YA TIENEN EL MUTEX DENTRO
        eliminar_proceso_lista(pcb_a_ejecutar);//ESTAS FUNCIONES YA TIENEN EL MUTEX DENTRO

        int err = enviar_pcb (pcb_a_ejecutar->data.PID, cpu_libre->fd); //Envia el PID a la CPU
        if (err != 1) return log_error (logger, ""); // Completar log de error        
        
        log_info(logger, "PID %d enviado a ejecutar en socket %d", pcb_a_ejecutar->data.PID, cpu_libre->fd);

        

        if (strcmp(planificacion_algoritmo, "RR") == 0) {
            // ceamos un hilo que espere el Quatum y mande la interrupción --> CHEQUEAR
            pthread_create(&hilo_timer, NULL, hilo_quantum, (void*)pcb_a_ejecutar);
            pthread_detach(hilo_timer);
            enviar_desalojo(socket_cliente); //Hacer Funcion
    
        }
    }
}
}

bool es_la_cpu_buscada(void* elemento, void* contexto) {
    
        CPU* cpu = (CPU*) elemento;
    
        // Casteamos el contexto de vuelta a un puntero de int para sacar el socket
     int socket_buscado = *(int*) contexto; 
    
    return (cpu->fd == socket_buscado) && (cpu->enUso == false);
}



void enviar_desalojo(int socket_cliente){/* HACER  */
   
    
   log_info(logger, "Enviado Desalojo a socket %d por fin de Quantum",cpu_libre->fd);

}


/*-----                     GESTION DE HILOS                     -----*/





/*-----                     GESTION DE OP_CODEs                     -----*/

    //OK, 
    //NOTOK,
    
    //MENSAJE,
	//PAQUETE, 

	//con la CPU
	
    //NUEVA_CPU,
void nueva_cpu (int cliente_fd) {

        CPU* info_cpu = malloc(sizeof(CPU));

        info_cpu->fd = cliente_fd;         
        info_cpu->enUso = false;    
                    
        pthread_mutex_lock(&mutex_cpus);
        list_add(list_suplementarias->cpu, info_cpu);
        pthread_mutex_unlock(&mutex_cpus);
        
        enviar_op_code (OK, cliente_fd);

        log_info(logger, "CPU registrada en el socket %d", cliente_fd);

        //NO PONGO mandar_proceso_cpu() porque para mi la CPU deberia comunicarse devuelta USANDO el OP_CODE CPU_LIBRE
        }
    //CPU_LIBRE,
void cpu_libre (int cliente_fd){

    mandar_proceso_cpu(cliente_fd);
}
    //FIN_PROCESO,
void fin_proceso (int cliente_fd){ /*HACER*/

    


}
    //DESALOJO,
    //PCB,

    //syscalls de la CPU --- Descripcion de cada una esta en el TP.
    
    //MUTEX_CREATE,
void mutex_create (int socket_cliente){

        enviar_op_code(OK, socket_cliente); //Segundo paso del Handshake

        char* mutex_id = recibir_mensaje (socket_cliente);

        mutex_cpu* mutex = malloc(sizeof(mutex_cpu));

        mutex->mutex_id = mutex_id;
        mutex->valor = 1;
        
        list_add(lista_mutex, mutex);

        enviar_op_code(OK, socket_cliente);
        
    }
    //MUTEX_LOCK,
void mutex_lock (int socket_cliente){

        enviar_op_code(OK, socket_cliente);

        char* mutex_id = recibir_mensaje (socket_cliente);

        mutex_cpu* mutex = list_find_with_context(lista_mutex, es_el_mutex_buscado, mutex_id);

        while (mutex->valor =! 1){

            sleep(1000); //para que la espera activa sea menos grave

            log_info (logger, "El mutex %d esta bloqueado", mutex->mutex_id);
        }
        
        pthread_mutex_lock (&mutex_simulados);
        mutex->valor = 0;    
        pthread_mutex_unlock (&mutex_simulados);

        enviar_op_code(OK, socket_cliente);
    }
    //MUTEX_UNLOK,
void mutex_unlock (int socket_cliente){

        enviar_op_code(OK, socket_cliente);

        char* mutex_id = recibir_mensaje (socket_cliente);

        mutex_cpu* mutex = list_find_with_context(lista_mutex, es_el_mutex_buscado, mutex_id);

        pthread_mutex_lock (&mutex_simulados);
        mutex->valor = 1;
        pthread_mutex_unlock (&mutex_simulados);
    }
    //MEM_ALLOC,
    //MEM_FREE,
   

    //SLEEP
sleep(int socket_cpu){

        t_paquete* paquete = recibir_paquete(socket_cpu);
                
        int pid_a_bloquear;
        memcpy(&pid_a_bloquear, paquete->buffer->stream, sizeof(int));
               
        char* tiempo_str = (char*)(paquete->buffer->stream + sizeof(int));
        int tiempo_ms = atoi(tiempo_str); 
                
        log_info(logger, "Recibida syscall SLEEP (PID: %d, Tiempo: %d ms)", pid_a_bloquear, tiempo_ms);
                
        eliminar_paquete(paquete);

        PCB* pcb = encontrar_pcb_rnn_por_pid(pid_a_bloquear);
                
        if (pcb != NULL) {
            list_remove_element(listasProcesos -> rnn, pcb);
            cambiar_estado_pcb(pcb, BCK);
            list_add(listasProcesos -> bck, pcb);
                    
            iniciar_timer_bloqueo(pcb, tiempo_ms);
                    
            enviar_op_code(OK, socket_cpu);

        } else {
            log_error(logger, "PID %d no encontrado en EXEC", pid_a_bloquear);
            enviar_op_code(ERROR, socket_cpu);
        }
    }

PCB* encontrar_pcb_rnn_por_pid(int pid) {
    pthread_mutex_lock(&sem_procesos_exit); 

    PCB* pcb_buscado = NULL;
    
    for (int i = 0; i < list_size(istasProcesos -> rnn); i++) {
        PCB* pcb_actual = list_get(istasProcesos -> rnn, i);
        if (pcb_actual->pid == pid) {
            pcb_buscado = pcb_actual;
            break;
        }
    }
    
    pthread_mutex_unlock(&sem_procesos_exit);
    return pcb_buscado;
}

void iniciar_timer_bloqueo(t_pcb* pcb, int tiempo_ms) {
    pthread_t hilo_timer;
    
    t_timer_args* args = malloc(sizeof(t_timer_args));
    args->pcb = pcb;
    args->tiempo_ms = tiempo_ms;

    pthread_create(&hilo_timer, NULL, ejecutar_sleep_asincrono, args);
    pthread_detach(hilo_timer);
}


void* ejecutar_sleep_asincrono(void* args) {
    t_timer_args* datos = (t_timer_args*)args;
    
    usleep(datos->tiempo_ms * 1000); 
    
    pthread_mutex_lock(&sem_procesos_block);
    list_remove_element(listasProcesos -> bck, datos->pcb); 
    pthread_mutex_unlock(&sem_procesos_block);

    pthread_mutex_lock(&sem_procesos_ready);
    cambiar_estado_pcb(datos -> pcb, RDY);
    agregar_lista_ready(datos -> pcb);
    pthread_mutex_unlock(&sem_procesos_ready);
    
    log_info(logger, "PID %d finalizó SLEEP, movido a READY.", datos->pcb->pid);
    
    
    free(datos); 
    return NULL;
}


    
    //MEM_CORRUPT, /*cortar todo con esta*/

	//con la IO

    //IO_LIBRE,
void io_libre (void* arg) { 
    int io_fd = (intptr_t)arg;

    pthread_mutex_lock(&mutex_ios);
    IO* interfaz = buscar_io_por_fd(io_fd);
    
    t_pedido_io* pedido_terminado = queue_pop(interfaz->cola_bloqueados);
    interfaz->enUso = false;
    pthread_mutex_unlock(&mutex_ios);

    PCB* pcb_a_ready = pedido_terminado->pcb;

    cambiar_estado_pcb(pcb_a_ready, RDY);
    
    pthread_mutex_lock(&mutex_ready);
    list_add(cola_ready, pcb_a_ready); 
    pthread_mutex_unlock(&mutex_ready);

    log_info(logger, "PID %u finalizó IO en %s y vuelve a READY", pcb_a_ready->data.PID, interfaz->nombre);

    free(pedido_terminado);

    mandar_proceso_io(interfaz);
}

void nueva_io (void* arg){

    int cliente_fd = (intptr_t)arg;

    
    IO* info_io = malloc(sizeof(IO));
    info_io->fd = cliente_fd;         
    info_io->enUso = false;    
    
   
    info_io->nombre = recibir_string(cliente_fd); 
    
    info_io->cola_bloqueados = queue_create(); 
                
    pthread_mutex_lock(&mutex_ios);
    list_add(list_suplementarias->io, info_io);
    pthread_mutex_unlock(&mutex_ios);

    enviar_op_code (OK, cliente_fd);
                
    log_info(logger, "IO '%s' registrada en el socket %d", info_io->nombre, cliente_fd);
}


// STDIN
void io_stdin(t_list* lista) {

    uint32_t pid = *(uint32_t*)list_get(lista, 0);
    PCB* pcb = buscar_pcb_por_pid(pid);

    char* nombre_io = (char*)list_get(lista, 1);

    uint32_t dir = *(uint32_t*)list_get(lista, 2);
    uint32_t tam = *(uint32_t*)list_get(lista, 3);

    IO* interfaz = buscar_io_por_nombre(nombre_io); 

    t_pedido_io* pedido = malloc(sizeof(t_pedido_io));
    pedido->pcb = pcb;
    pedido->tipo_operacion = IO_STDIN;
    pedido->dir_fisica = dir;
    pedido->tamano = tam;

    cambiar_estado_pcb(pcb, BCK);

    pthread_mutex_lock(&mutex_ios);
    queue_push(interfaz->cola_bloqueados, pedido);
    pthread_mutex_unlock(&mutex_ios);

    log_info(logger, "PID %d bloqueado en %s (STDIN - Dir: %u, Tam: %u)", pcb->data.PID, nombre_io, dir, tam);

    mandar_proceso_io(interfaz);
}


// STDOUT
void atender_io_stdout(t_list* lista) {
    uint32_t pid_recibido = *(uint32_t*)list_get(lista, 0);
    PCB* pcb = buscar_pcb_por_pid(pid_recibido); 
    char* nombre_io = (char*)list_get(lista, 1);
    uint32_t dir = *(uint32_t*)list_get(lista, 2);
    uint32_t tam = *(uint32_t*)list_get(lista, 3);

    IO* interfaz = buscar_io_por_nombre(nombre_io); 

    t_pedido_io* pedido = malloc(sizeof(t_pedido_io));
    pedido->pcb = pcb;
    pedido->tipo_operacion = IO_STDOUT;
    pedido->dir_fisica = dir;
    pedido->tamano = tam;

    cambiar_estado_pcb(pcb, BCK);

    pthread_mutex_lock(&mutex_ios);
    queue_push(interfaz->cola_bloqueados, pedido);
    pthread_mutex_unlock(&mutex_ios);

    log_info(logger, "PID %d bloqueado en %s (STDOUT - Dir: %u, Tam: %u)", pcb->data.PID, nombre_io, dir, tam);

    mandar_proceso_io(interfaz);
}


PCB* buscar_pcb_por_pid(uint32_t pid_recibido) {
    t_list* listas_a_revisar[] = { 
        ListaProcesos-> new, ListaProcesos->rdy, 
        ListaProcesos-> rnn, ListaProcesos->bck, 
        ListaProcesos-> ext 
    };

    for (int i = 0; i < 5; i++) {
        t_list* lista_actual = listas_a_revisar[i];
        
        t_list_iterator* it = list_iterator_create(lista_actual);
        while (list_iterator_has_next(it)) {
            PCB* pcb = (PCB*) list_iterator_next(it);
            if (pcb->pid == pid_recibido) {
                list_iterator_destroy(it);
                return pcb; 
            }
        }
        list_iterator_destroy(it);
    }

    return NULL; 

}

//INIT PROC
init_proc(int socket_cliente){
    t_paquete* paquete = recibir_paquete(socket_cliente);
    char* path = (char*)paquete->buffer->stream;
    int prioridad = *(int*)(paquete->buffer->stream + strlen(path) + 1);

    log_info(logger, "Solicitud INIT_PROC: %s (Prioridad: %d)", path, prioridad);

    PCB* nuevo_pcb = crear_pcb(path, prioridad);

    enviar_proceso_nuevo_KM(nuevo_pcb->pid, path);

    if (recibir_operacion(sockets.conexion_memoria) == OK) {
    log_info(logger, "Proceso %d cargado en memoria.", nuevo_pcb->pid);
                    
    pthread_mutex_lock(&mutex_ready);
    cambiar_estado_pcb(nuevo_pcb, RDY);
    agregar_lista_ready(nuevo_pcb);
    pthread_mutex_unlock(&mutex_ready);
                    
    }
                
    eliminar_paquete(paquete);
}

void enviar_proceso_nuevo_KM(int pid, char* path) {
    t_paquete* paquete = crear_paquete(PROCESO_NUEVO);

    agregar_a_paquete(paquete, &pid, sizeof(int));

    int path_len = strlen(path) + 1;
    agregar_a_paquete(paquete, &path_len, sizeof(int));
    agregar_a_paquete(paquete, path, path_len);

    enviar_paquete(paquete, conexion.km);

    eliminar_paquete(paquete);

    log_info(logger, "Se notificó a Memoria la creación del proceso PID: %d, Path: %s", pid, path);
}

void mandar_proceso_io(IO* interfaz) {
    pthread_mutex_lock(&mutex_ios);
    
    // Si la interfaz no está haciendo nada y hay alguien esperando
    if (!interfaz->enUso && !queue_is_empty(interfaz->cola_bloqueados)) {
        
        // Miramos el pedido pero NO lo sacamos de la cola todavía (lo sacamos en io_libre)
        t_pedido_io* pedido = queue_peek(interfaz->cola_bloqueados);
        interfaz->enUso = true;
        pthread_mutex_unlock(&mutex_ios);

        // Armamos el paquete que va a viajar hacia el módulo de la IO externa
        t_paquete* paquete = crear_paquete(pedido->tipo_operacion);
        
        // Dependiendo de qué operacion sea, le metemos sus datos al paquete
        switch(pedido->tipo_operacion) {
            case IO_SLEEP:
                agregar_a_paquete(paquete, &(pedido->tiempo_sleep), sizeof(int));
                break;
                
            case IO_STDIN:
            case IO_STDOUT:
                agregar_a_paquete(paquete, &(pedido->dir_fisica), sizeof(uint32_t));
                agregar_a_paquete(paquete, &(pedido->tamano), sizeof(uint32_t));
                agregar_a_paquete(paquete, &(pedido->pcb->data.PID), sizeof(int));
                break;
        }

        enviar_paquete(paquete, interfaz->fd);
        eliminar_paquete(paquete);
        
        log_info(logger, "Kernel: Orden enviada a la interfaz %s para el PID %d", interfaz->nombre, pedido->pcb->data.PID);
    } else {
        pthread_mutex_unlock(&mutex_ios);
    }
}



//EXIT
exit_proceso(int socket_cpu){

    t_paquete* paquete = recibir_paquete(socket_cpu);
    int pid_a_finalizar = *(int*)paquete->buffer->stream;
    eliminar_paquete(paquete);

    log_info(logger, "Finalizando proceso PID: %d", pid_a_finalizar);

    enviar_proceso_finalizar_KM(pid_a_finalizar);
    
    PCB* pcb = encontrar_pcb_en_running(pid_a_finalizar);
    if (pcb != NULL) {
        eliminar_proceso_Lista(pcb);
    }

    enviar_op_code(OK, socket_cpu);
    
}

void enviar_proceso_KM(uint32_t pid, op_code opCode) {
  
    t_paquete* paquete = crear_paquete(opCode);
    
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));    
    
    enviar_paquete(paquete, conexion.km);
    
    eliminar_paquete(paquete);
    
    log_info(logger, " Enviado a KM, PID: %u", pid);
}

PCB* encontrar_pcb_en_running(uint32_t pid_a_finalizar) {
    
    for (int i = 0; i < list_size(listasProcesos->rnn); i++) {
        
        PCB* pcb_en_cpu = (PCB*) list_get(listasProcesos->rnn, i);

        if (pcb_en_cpu != NULL && pcb_en_cpu->pid == pid_a_finalizar) {
            return pcb_en_cpu;
        }
    }
    
    return NULL;
}

bool es_el_mutex_buscado(void* elemento, void* contexto) {
    mutex_cpu* un_mutex = (mutex_cpu*) elemento;
    char* id_buscado = (char*) contexto;
    
    // strcmp devuelve 0 si los strings son exactamente iguales
    return strcmp(un_mutex->mutex_id, id_buscado) == 0;
}



