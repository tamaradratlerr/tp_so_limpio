#include "server.h"



int main(void) {
    logger = log_create("log.log", "Servidor", 1, LOG_LEVEL_DEBUG);

    int server_fd = iniciar_servidor();
    log_info(logger, "Servidor listo para recibir clientes");

    // BUCLE 1: Para aceptar nuevos clientes uno tras otro
    while (1) {
        int cliente_fd = esperar_cliente(server_fd);
        if (cliente_fd == -1) {
            log_error(logger, "Error al aceptar un cliente");
            continue; // Intentamos con el siguiente
        }
        
        log_info(logger, "Cliente conectado. Iniciando recepción...");

        // BUCLE 2: El que ya tenías, para procesar a ESTE cliente
        int control_loop = 1;
        while (control_loop) {
            int cod_op = recibir_operacion(cliente_fd);
            
            t_list* lista;
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

//Funcion que inicializa todas las listas de los Procesos//
listas_procesos* Iniciar_listas_procesos (void){
	
	listas_procesos* l_procesos;

	l_procesos->new = list_create();
	l_procesos->rnn = list_create();
	l_procesos->bck = list_create();
	l_procesos->ext = list_create();
	l_procesos->rdy = list_create();

	return l_procesos;
};


void terminar_listas_procesos (listas_procesos* listas_procesos){
	list_destroy(listas_procesos->new);
	list_destroy(listas_procesos->rnn);
	list_destroy(listas_procesos->bck);
	list_destroy(listas_procesos->ext);
	list_destroy(listas_procesos->rdy);

	return 0;
}

listas_procesos* listasProcesos;

//Funcion que a suma un PCB a su lista correspondiente segun su ESTADO ACTUAL.
void agregar_proceso_lista (PCB* pcb){

	sacar_proceso_lista();//Funcion pensada para tomar estado anterior y sacarlo de esa lista.
	switch (pcb->estado_pcb)
	{
	case 1: //NEW
		list_add(listasProcesos->new, pcb);
	case 2: //RUNNING
		list_add(listasProcesos->rnn, pcb);
	case 3: //BLOCK
		list_add(listasProcesos->bck, pcb);
	case 4: //EXIT
		list_add(listasProcesos->ext, pcb);

	case 5: //RDY
		agregar_lista_ready(pcb, "algoritmo");
		break;
	
	default:
		break;
	}

}


//Funcion que inicia un nuevo PCB en estado NEW en utilsKS


//Funcion que termina un pcb liberando su memoria en utils KS

//funcion CAmbia estado pcb
void cambiar_estado_pcb(PCB* pcb, estado nuevoEstado){
    pcb -> estado_anterior = (pcb ->estado_pcb);
    pcb ->estado_pcb = nuevoEstado;
}





//PLANIFICACION

void agregar_lista_ready(PCB* pcb, algortimoEnUso algoritmo){

    switch (algoritmo)
    {
    case FIFO:
        ready_FIFO(pcb);

    case NM:

    case RR:
        /* code */
        break;
    
    default:
        break;
    }

}





sem_t sem_procesos_ready;
sem_t sem_cpus_libres;

pthread_mutex_t mutex_ready;


//de new a ready fifo
void agregar_a_ready_FIFO(PCB* pcb_nuevo) {

    cambiar_estado_pcb(pcb_nuevo, RDY);  

    sem_wait(&sem_procesos_ready); 

    pthread_mutex_lock(&mutex_ready);
    
    list_add(listasProcesos -> rdy, pcb_nuevo); // Lo pones al final de la lista
    
    pthread_mutex_unlock(&mutex_ready);

    sem_post(&sem_procesos_ready); 
}


void agregar_a_ready_RR(PCB* pcb_nuevo) {

    //sacamos el quantum desde config (no se como)
    int quantum;

    while(timepo){
        
    }
    

    
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
    
    if (algoirtimo == rr){
        sleep(quantum);
        desalojar_proceso;
    }
    
    
    manejar_vuelta_proceso_cpu(listasProcesos -> rnn)
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

