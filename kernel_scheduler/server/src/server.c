#include "server.h"





int main(void) {

    logger = log_create("log.log", "Servidor", 1, LOG_LEVEL_DEBUG);

    //Esto debe suceder luego de conectarse con la KM hay que ver como...
    int server_fd = iniciar_servidor();
    log_info(logger, "Servidor listo para recibir clientes");

    // BUCLE 1: Para aceptar nuevos clientes uno tras otro
    int control_loop_2 = 1;
    while (control_loop_2) {

        /*En esta intancia estamos esperando la conexion de un cliente*/
        int cliente_fd = esperar_cliente(server_fd);

        /*Verificamos si la conexion con el nuevo cliente fue exitosa*/
        if (cliente_fd == -1) {
            log_error(logger, "Error al aceptar un cliente");
            continue; // Intentamos con el siguiente
        }
        
        log_info(logger, "Cliente conectado. Iniciando recepción...");

        pthread_t hilo_id; //Identificador del hilo

        if (pthread_create(&hilo_id, NULL))


        // BUCLE 2: El que ya tenías, para procesar a ESTE cliente
        int control_loop_1 = 1;
        while (control_loop_1) {

            //Creo que aca se deberi crear el HILO
            int cod_op = recibir_operacion(cliente_fd);
            
            t_list* lista;

            //aca deberia ver que es lo que conecto con el servidor y dsp llamar a la funcion atender y hacer ese switch
            
            switch (cod_op) {
                case MENSAJE:
                    recibir_mensaje(cliente_fd);
                    break;
                case PAQUETE:
                    lista = recibir_paquete(cliente_fd);
                    log_info(logger, "Me llegaron los siguientes valores:");
                    list_iterate(lista, (void*) iterator);
                    list_destroy_and_destroy_elements(lista, (void*)free);
                    break;
                case -1:
                    log_info(logger, "El cliente se desconectó.");
                    control_loop = 0; // Salimos del bucle interno
                    break;
                default:
                    log_warning(logger,"Operación desconocida.");
                    break;
            }
        }
        
        // Al terminar el bucle de mensajes, cerramos el socket de ese cliente
        close(cliente_fd);
        log_info(logger, "Esperando al siguiente cliente...");
    }

    return EXIT_SUCCESS;
}

void iterator(char* value) {
	log_info(logger,"%s", value);
}



/*-------- CHECKPOINT 2 --------------------------------------------------------------------*/


//PCB ---------

//Funcion que inicializa todas las listas de los Procesos
listas_procesos* Iniciar_listas_procesos (void){

	l_procesos->new = list_create();
	l_procesos->rnn = list_create();
	l_procesos->bck = list_create();
	l_procesos->ext = list_create();
	l_procesos->rdy = list_create();

	return l_procesos;
};


void terminar_listas_procesos (){

	list_destroy(listas_procesos->new);
	list_destroy(listas_procesos->rnn);
	list_destroy(listas_procesos->bck);
	list_destroy(listas_procesos->ext);
	list_destroy(listas_procesos->rdy);

	return 0;
}

//Funcion que inicializa las listas de CPUs y IOs
void iniciar_listas_suple (){
    l_suple->cpu = list_create();
    l_suple->io = list_create();

    return 0;
}

void eliminar_listas_suple (){
    list_destroy_and_destroy_elements(l_suple->cpu);
    list_destroy_and(l_suple->io);

    return 0;
}

//Funcion que a suma un PCB a su lista correspondiente segun su ESTADO ACTUAL.
void agregar_proceso_lista (PCB* pcb){

	sacar_proceso_lista();//Funcion pensada para tomar estado anterior y sacarlo de esa lista.
	
    switch (pcb->estado_pcb){
	case NEW: //NEW
        
        pthread_mutex_lock(&sem_procesos_new);

		list_add(listasProcesos->new, pcb);

        pthread_mutex_unlock(&sem_procesos_new);

	case RNN: //RUNNING

        pthread_mutex_lock(&sem_procesos_running);    

		list_add(listasProcesos->rnn, pcb);

        pthread_mutex_unlock(&sem_procesos_running);
	case BCK: //BLOCK

        pthread_mutex_lock(&sem_procesos_block);

		list_add(listasProcesos->bck, pcb);

        pthread_mutex_unlock(&sem_procesos_block);

	case EXT: //EXIT

        pthread_mutex_lock(&sem_procesos_exit);    

		list_add(listasProcesos->ext, pcb);

        pthread_mutex_unlock(&sem_procesos_exit);

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
void eliminar_proceso_Lista (PCB* pcb ){

    bool removed;
    switch (pcb->estado_anterior)
	{
	case NEW: //NEW
        pthread_mutex_lock(&sem_procesos_new);
		removed = list_remove_element(listasProcesos->new, pcb);
        pthread_mutex_unlock(&sem_procesos_new);
	case RNN: //RUNNING
        pthread_mutex_lock(&sem_procesos_running);
		removed = list_remove_element(listasProcesos->rnn, pcb);
        pthread_mutex_unlock(&sem_procesos_running);
	case BCK: //BLOCK
        pthread_mutex_lock(&sem_procesos_block);
		removed = list_remove_element(listasProcesos->bck, pcb);
        pthread_mutex_unlock(&sem_procesos_block);
	case EXT: //EXIT
        pthread_mutex_lock(&sem_procesos_exit);
		removed = list_remove_element(listasProcesos->ext, pcb);
        pthread_mutex_unlock(&sem_procesos_exit);
	case RDY: //RDY
        pthread_mutex_lock(&sem_procesos_ready);
		removed = list_remove_element(listas_procesos->rdy, pcb);
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

//funcion Cambia estado pcb //Se le podria agregar semaforos pero antes y dsp de llamar a la funcion
void cambiar_estado_pcb(PCB* pcb, estado nuevoEstado){
    pcb -> estado_anterior = (pcb ->estado_pcb);
    pcb ->estado_pcb = nuevoEstado;
}



//PLANIFICACION

void agregar_lista_ready(PCB* pcb){

    if (strcmp(planificacion_algoritmo, "FIFO") == 0) {
        ready_FIFO(pcb);
    } 
    else if (strcmp(planificacion_algoritmo, "RR") == 0) {
        ready_FIFO(pcb); // ready_RR(pcb);
    } 
    else if (strcmp(planificacion_algoritmo, "VRR") == 0) {
        // ready_VRR(pcb);
    }
    else {
    return -1;
    }
    return 0;
}





//de new a ready fifo
void ready_FIFO(PCB* pcb_nuevo) {

    cambiar_estado_pcb(pcb_nuevo, RDY);  

    sem_wait(&sem_procesos_ready); 

    pthread_mutex_lock(&mutex_ready);
    
    list_add(listasProcesos -> rdy, pcb_nuevo); // Lo pones al final de la lista
    
    pthread_mutex_unlock(&mutex_ready);

    sem_post(&sem_procesos_ready); 
}


// En la parte del KS que recibe el PCB de vuelta de la CPU
void manejar_vuelta_proceso_cpu(PCB* pcb) {

    sem_wait(&sem_cpus_libres);
    // fijarme si el pcb va a EXIT o BLOCKED 

    /*  a donde le manda la cpu los ultimos registros: dorectamedirectamente a la KM o al KS  */

    // Liberamos la CPU para el planificador
    sem_post(&sem_cpus_libres); 
}


void atender_cpu(){


    /*  recibe interrupcion o lo que sea de la cpu, identifica que tenes que hacer (si enviar otro proceso, atender interrupcion, etc)
        y resuelve  ---> con un enum gigante de todas las comunicaciones que pueden tener entre cpu y Ks */

    enviar_a_cpu_disponible(listasProcesos -> rnn);
    
    if (strcmp(planificacion_algoritmo, "RR") == 0){
        sleep(intervalo_tarea);
        desalojar_proceso_cpu();
    }
    
    
    manejar_vuelta_proceso_cpu(listasProcesos -> rnn);

    //OP_code interrupcion IO


}


//Funcion que saca al primer elemento de la lista ready y lo pone en running
void agregar_running() {

        
    pthread_mutex_lock(&mutex_ready);
        
    // sacar al primero de la fila
    PCB* pcb = list_pop_first(listasProcesos -> rdy);
        
    pthread_mutex_unlock(&mutex_ready);

    cambiar_estado_pcb(pcb, RNN);

    agregar_proceso_lista(pcb);

    
    
}




//***  IO ***//

//Mandar (cuando se recibe la peticion desde la CPU se acciona esta funcion....)
//en caso de CPU: (CPU) y agrego a listas CPU; en caso IO: (STDIN) ; (STDOUT) ; (SLEEP) y se agrega cada una a su lista.  
//vamos a hacer una estructura por cada tipo de cliene. (CPU , IO)

//t_list* l_CPU = list_create(); //CREAR DESTROY DE TODAS ESTAS LISTAS

//hacer ENUM de todas las comunicaciones posibles con la CPU.

//Hacer estado EXIT;




io_stdint(int TAMAÑO){    /*pasar por toda la lista de STDINs buscando una que no este en uso*/
    while (1)
    {
        /* code */
    }
    
    //hace la conect

    //recibe el mesaje

    //Vemos que devuelva.....


}



io_stdout();

io_sleep();
//Recibir

