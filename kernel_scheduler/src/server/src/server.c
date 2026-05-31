#include "server.h"
#include <utilsKS.h>
#include "../../utils/src/global_utils.h"

pthread_t hilo_quantum;
t_listas_procesos* listasProcesos = NULL;
t_listas_suplementarias* list_suplementarias = NULL;

int main(void) {
    
    pthread_mutex_init(&mutex_cpus, NULL);

    //FALTA PONER LOS MUTEX_INIT DE TODOS LOS MUTEX DE LOS ESTADOS
    //PONER EN UNA FUNCION EL DISTROY DE LA LISTA DE CPUS_COENCTADAS


    //no hay logger aca porque cliente y servidor lo comparten (esta en cliente)

    int server_fd = iniciar_servidor(info_km.puerto_km); 

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
            case DESALOJO:
                //por parte de la km
                deslojarTodasCpus();
                break;
            case gl_IO_SLEEP: 
               //agarrar io de la lista de ios y mandar
                IO* io = queue_pop(list_suplementarias -> io_ready);
                io_sleep(cliente_fd, io -> fd); 
                break;

            case gl_IO_STDIN: 
                // viene de la cpu 
                IO* io = queue_pop(list_suplementarias -> io_ready);
                io_stdin(cliente_fd, io -> fd, info_km.conexion_km); 
                break;

            case gl_IO_STDOUT: 
                IO* io = queue_pop(list_suplementarias -> io_ready);
                io_stdout(cliente_fd, io -> fd); 
                break;

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


t_listas_procesos* Iniciar_listas_procesos (void){ /*Funcion que inicializa todas las listas de los Procesos*/

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
    else if (strcmp(planificacion_algoritmo, "CMN") == 0) {
        // ready_CMN(pcb)

    }
    else {
    return -1;
    }
    return 0;
}

void ready_FIFO(PCB* pcb_nuevo) { /*Funcion que a partir del ALGORITMO FIFO agrega un PCB a LISTA DE READYS ordenando por PRIORIDAD*/
 
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

PCB* buscar_pcb_por_pid(uint32_t pid_recibido) {
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


void mandar_proceso_cpu(int socket_cliente){ /* Funcion que manda el PCB de mayor priridad a una CPU es especial */
    
    
    pthread_mutex_lock(&mutex_cpus);
    
        /* Buscamos la CPU pasándole la dirección del socket_cliente como contexto */
    t_CPU *cpu_libre = list_find_with_context(list_suplementarias->cpu, es_la_cpu_buscada, &socket_cliente);

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

            if(pcb_a_ejecutar->estado_pcb == RNN){
                enviar_desalojo(socket_cliente); //Hacer Funcion
            }
            
    
        }
    }
}
}

void* hilo_quantum(void* arg) {
    PCB* pcb = (PCB*)arg;
    
    // Dormimos el tiempo del quantum (usleep espera microsegundos)
    usleep(info_config.intervalo_tarea * 1000); 
    
    // Cuando despierta, el tiempo se terminó. 
    // Aquí notificas al Kernel que debe pedir el desalojo.
    log_info(logger, "Quantum expirado para PID: %d. Solicitando desalojo...", pcb->data.PID);
    
    return NULL;
}


bool es_la_cpu_buscada(void* elemento, void* contexto) {
    
        t_CPU* cpu = (t_CPU*) elemento;
    
        // Casteamos el contexto de vuelta a un puntero de int para sacar el socket
     int socket_buscado = *(int*) contexto; 
    
    return (cpu->fd == socket_buscado) && (cpu->enUso == false);
}

void enviar_desalojo(int socket_cliente){/* HACER  */
   
    
   log_info(logger, "Enviado Desalojo a socket %d por fin de Quantum");

}


/*-----                     GESTION DE HILOS                     -----*/





/*-----                     GESTION DE OP_CODEs                     -----*/

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
void deslojarTodasCpus() {
    
    int cantidad = list_size(list_suplementarias->cpu);
    
    for(int i = 0; i < cantidad; i++) {
        int socket_cpu = *(int*)list_get(list_suplementarias->cpu, i);
        enviar_op_code(DESALOJO, socket_cpu); 
    }
}


/*----syscalls de la CPU --- Descripcion de cada una esta en el TP-----*/
    
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

//tami
//MEM_ALLOC,
void mem_alloc (){//Hacer

}; 
//MEM_FREE,
void mem_free (){// Hacer

} 
//INIT PROC
void init_proc(int socket_cliente){
    
    t_paquete* paquete = recibir_paquete(socket_cliente);
    char* path = (char*)paquete->buffer->stream;
    int prioridad = *(int*)(paquete->buffer->stream + strlen(path) + 1);

    log_info(logger, "Solicitud INIT_PROC: %s (Prioridad: %d)", path, prioridad);

    PCB* nuevo_pcb = crearNuevoProceso(logger, path, info_km.conexion_km);

    if (recibir_op_code(info_km.conexion_km) == OK) {
                    
    pthread_mutex_lock(&mutex_ready);
    cambiar_estado_pcb(nuevo_pcb, RDY);
    agregar_lista_ready(nuevo_pcb);
    pthread_mutex_unlock(&mutex_ready);
                    
    }
                
    eliminar_paquete(paquete);
}

//EXIT
void exit_proceso(int socket_cpu){

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

/*-----Con la IO-----*/

//SLEEP
void io_sleep(int socket_cpu, int socket_io) {

    t_paquete* paquete_cpu = recibir_paquete(socket_cpu);
    int pid_a_bloquear;
    memcpy(&pid_a_bloquear, paquete_cpu->buffer->stream, sizeof(int));
    
    char* tiempo_str = (char*)(paquete_cpu->buffer->stream + sizeof(int));
    int tiempo_ms = atoi(tiempo_str); 
    
    log_info(logger, "Recibida syscall SLEEP (PID: %d, Tiempo: %d ms)", pid_a_bloquear, tiempo_ms);
    eliminar_paquete(paquete_cpu);

    PCB* pcb = encontrar_pcb_rnn_por_pid(pid_a_bloquear);
    
    if (pcb != NULL) {
        // Mover a bloqueados
        cambiar_estado_pcb(pcb, BCK);
        list_remove_element(listasProcesos->rnn, pcb);
        agregar_proceso_lista(pcb);
        
        // comunicación con la io-----
        
        t_io_sleep* datos_io = malloc(sizeof(t_io_sleep));
        datos_io->pid = (uint32_t)pid_a_bloquear;
        datos_io->time = (uint32_t)tiempo_ms;

        t_paquete* paquete_para_io = crear_paquete(gl_IO_SLEEP); 
        buffer_add(paquete_para_io->buffer, datos_io, sizeof(t_io_sleep));
        
        enviar_paquete(paquete_para_io, socket_io);
        
        // recibir el IO_LIBRE
        op_code op_libre = recibir_op_code(socket_io);
        if (op_libre == IO_LIBRE) {
            log_info(logger, "IO liberada tras SLEEP");
            IO* interfaz = buscar_io_por_fd(socket_io);
            interfaz->enUso = false;
            queue_push(list_suplementarias->io_ready, interfaz);
        }

        free(datos_io);
        eliminar_paquete(paquete_para_io);
        
        enviar_op_code(OK, socket_cpu);
        
        
    } else {
        log_error(logger, "PID %d no encontrado en EXEC", pid_a_bloquear);
        enviar_op_code(NOTOK, socket_cpu);
    }
}
//NUEVA_IO
void nueva_io (int cliente_fd){

    
    IO* info_io = malloc(sizeof(IO));
    info_io->fd = cliente_fd;         
    info_io->enUso = false;    
    
   
    info_io->nombre = recibir_string(cliente_fd); 
    
                
    pthread_mutex_lock(&mutex_ios);
    list_add(list_suplementarias->io_ready, info_io);
    list_add(list_suplementarias->io, info_io);
    pthread_mutex_unlock(&mutex_ios);

    enviar_op_code (OK, cliente_fd);
                
    log_info(logger, "IO '%s' registrada en el socket %d", info_io->nombre, cliente_fd);
}
// STDIN
void io_stdin(int socket_cpu, int socket_io, int socket_memoria) {
    t_paquete* paquete = recibir_paquete(socket_cpu);
    
    uint32_t tam, dir, pid;
    memcpy(&tam, paquete->buffer->stream, sizeof(uint32_t));
    memcpy(&dir, paquete->buffer->stream + sizeof(uint32_t), sizeof(uint32_t));
    memcpy(&pid, paquete->buffer->stream + (2 * sizeof(uint32_t)), sizeof(uint32_t));

    t_paquete* paquete_io = crear_paquete(gl_IO_STDIN);
    agregar_a_paquete(paquete_io, &pid, sizeof(uint32_t));
    agregar_a_paquete(paquete_io, &dir, sizeof(uint32_t));
    agregar_a_paquete(paquete_io, &tam, sizeof(uint32_t));
    enviar_paquete(paquete_io, socket_io);
    eliminar_paquete(paquete_io); // Limpiamos tras enviar

    op_code cod_op = recibir_op_code(socket_io);
    if (cod_op == IO_STDIN_RETORNO) {
        t_paquete* paquete_datos = recibir_paquete(socket_io);
        
        uint32_t direccion_logica, tam_datos;
        memcpy(&direccion_logica, paquete_datos->buffer->stream, sizeof(uint32_t));
        memcpy(&tam_datos, paquete_datos->buffer->stream + sizeof(uint32_t), sizeof(uint32_t));

        // Aquí debes extraer los datos reales leídos por el IO
        void* buffer_usuario = malloc(tam_datos);
        memcpy(buffer_usuario, paquete_datos->buffer->stream + (2 * sizeof(uint32_t)), tam_datos);

        t_paquete* paquete_mem = crear_paquete(km_IO_STDIN);
        agregar_a_paquete(paquete_mem, &direccion_logica, sizeof(uint32_t));
        agregar_a_paquete(paquete_mem, &tam_datos, sizeof(uint32_t));
        agregar_a_paquete(paquete_mem, buffer_usuario, tam_datos);
        
        enviar_paquete(paquete_mem, socket_memoria);

        free(buffer_usuario); // Liberamos el malloc temporal
        eliminar_paquete(paquete_datos);
        eliminar_paquete(paquete_mem);
    }

    op_code op_libre = recibir_op_code(socket_io);
    if (op_libre == IO_LIBRE) {
        IO* interfaz = buscar_io_por_fd(socket_io);
        if(interfaz != NULL) {
            interfaz->enUso = false;
            queue_push(list_suplementarias->io_ready, interfaz);
        }
    }
    eliminar_paquete(paquete);
}

// STDOUT
void io_stdout(t_list* lista, int io_socket) {

    // deserializar lo que viene de la CPU
    uint32_t pid = *(uint32_t*)list_get(lista, 0);
    uint32_t dir = *(uint32_t*)list_get(lista, 1);
    uint32_t tam = *(uint32_t*)list_get(lista, 2);
    
    // bloquear PCB
    PCB* pcb = buscar_pcb_por_pid(pid);
    cambiar_estado_pcb(pcb, BCK); // O el estado que uses
    
    // pedir datos a km (Lo pide el enunciado yo mucho no entendi para qué)
    t_paquete* req_mem = crear_paquete(km_IO_STDOUT);
    agregar_a_paquete(req_mem, &pid, sizeof(uint32_t));
    agregar_a_paquete(req_mem, &dir, sizeof(uint32_t));
    agregar_a_paquete(req_mem, &tam, sizeof(uint32_t));
    enviar_paquete(req_mem, info_km.conexion_km); 
    eliminar_paquete(req_mem);

    // recibir de km
    t_paquete* resp_mem = recibir_paquete(info_km.conexion_km);
    void* datos_leidos = resp_mem->buffer->stream; // Los bytes traídos de memoria

    // enviar a io
    t_paquete* paquete_io = crear_paquete(gl_IO_STDOUT);
    
    agregar_a_paquete(paquete_io, &pid, sizeof(uint32_t));
    agregar_a_paquete(paquete_io, &tam, sizeof(uint32_t));
    agregar_a_paquete(paquete_io, datos_leidos, tam); // Los datos que vinieron de memoria
    
    enviar_paquete(paquete_io, io_socket);
    eliminar_paquete(paquete_io);

    // El KS se queda esperando que la IO termine de imprimir
    op_code cod_op = recibir_op_code(io_socket);
    
    if (cod_op == IO_STDOUT_RETORNO) {
        t_paquete* paquete_fin = recibir_paquete(io_socket);
        eliminar_paquete(paquete_fin);
    }

    // recibir el IO_LIBRE
    op_code op_libre = recibir_op_code(io_socket);
    if (op_libre == IO_LIBRE) {
        log_info(logger, "IO liberada tras STDOUT");
        IO* interfaz = buscar_io_por_fd(io_socket);
        interfaz->enUso = false;
        queue_push(list_suplementarias->io_ready, interfaz);
    }
    eliminar_paquete(resp_mem);
}


/*-----Con el Kernel Memory-----*/

//MEM_CORRUPT
void mem_corrupt (); // Hacer


/*-----                     AUXILIARES                     -----*/

void enviar_proceso_finalizar_KM(int pid){
    t_paquete* paquete = crear_paquete(gl_EXIT);
    
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));    
    
    enviar_paquete(paquete, info_km.conexion_km);
    
    eliminar_paquete(paquete);
    
    log_info(logger, " Enviado a KM, PID: %u", pid);
}

void enviar_proceso_KM(uint32_t pid, op_code opCode) { //ver para que la hice
  
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

