#include "server.h"



int main(void) {
    pthread_mutex_init(&mutex_cpus, NULL);

    //FALTA PONER LOS MUTEX_INIT DE TODOS LOS MUTEX DE LOS ESTADOS
    //PONER EN UNA FUNCION EL DISTROY DE LA LISTA DE CPUS_COENCTADAS

    logger = log_create("log.log", "Servidor", 1, LOG_LEVEL_DEBUG);
   
    //mutex que trae la señal de la parte client
    //Esto debe suceder luego de conectarse con la KM hay que ver como...
        while(!g_server){
            
            /*g_servel va a ser una variable INT global que desde el cliente va a indicar cuando se va a poder levantar el servidor*/
            /*Deberia ser luego que conectarse con el KM por primera vez segun el enunciado del TP*/
            /*g_server inicia con un valor de FALSE*/
        } 
    
    int server_fd = iniciar_servidor(); 
    log_info(logger, "Servidor listo para recibir clientes");

    while (1) {
        // no es espera activa xq el SO se queda frenado hasta que reciba
        int cliente_fd = esperar_cliente(server_fd);
        
        if (cliente_fd == -1) {
            log_error(logger, "Error al aceptar un cliente");
            return -1; 
        }

        log_info(logger, "Nuevo cliente. Creamos el hilo");


        // -------------CREACIÓN DEL HILO----------

        pthread_t hilo_id;
        

        // esto lo saque del ejemplo que uso el profe
        
        // &hilo_id: donde se guarda el ID del hilo
        // NULL: configuración por defecto que puso el profe
        // atender_nuevo_cliente: funcion que atiende al cliente
        // el cliente_fd: Lo que le pasamos como argumento
        if (pthread_create(&hilo_id, NULL, atender_nuevo_cliente, (void*)(intptr_t)cliente_fd) != 0) {
            log_error(logger, "no se pudo crear el hilo");
            close(cliente_fd);
        }
        
    }




    return EXIT_SUCCESS;
}


void iterator(char* value) {
	log_info(logger,"%s", value);
}



/*----------------------------------------------------------------------------*/



//INICIALIZAR Y FINALIZAR LISTAS--------------------------------------

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
    list_suplementarias->cpu = list_create();
    list_suplementarias->stdint = list_create();
    list_suplementarias-> stdou= list_create();
    list_suplementarias-> sleep= list_create();

    return 0;
}

void eliminar_listas_suple (){
    list_destroy_and_destroy_elements(list_suplementarias->cpu);
    list_destroy_and(list_suplementarias->stdint);
    list_destroy_and(list_suplementarias->stdou);
    list_destroy_and(list_suplementarias->sleep);
    
    return 0;
}















//GESTIÓN DE LISTAS--------------------------------------

//Funcion que a suma un PCB a su lista correspondiente segun su ESTADO ACTUAL.
void agregar_proceso_lista (PCB* pcb){

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
void eliminar_proceso_Lista (PCB* pcb ){

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


void agregar_lista_ready(PCB* pcb){

    
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


//de new a ready fifo
void ready_FIFO(PCB* pcb_nuevo) {

    cambiar_estado_pcb(pcb_nuevo, RDY);  

    sem_wait(&sem_procesos_ready); 
    pthread_mutex_lock(&mutex_ready);
    
    list_add(listasProcesos -> rdy, pcb_nuevo); // Lo pones al final de la lista
    
    pthread_mutex_unlock(&mutex_ready);
    sem_post(&sem_procesos_ready); 
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












//GESTIÓN DE PCB-----------------------------------------------------------------

//funcion Cambia estado pcb //Se le podria agregar semaforos pero antes y dsp de llamar a la funcion
void cambiar_estado_pcb(PCB* pcb, estado nuevoEstado){
    pcb -> estado_anterior = (pcb ->estado_pcb);
    pcb ->estado_pcb = nuevoEstado;
}













//GESTIÓN DE LAS CPUS-----------------------------------------------------------------


//hacer ENUM de todas las comunicaciones posibles con la CPU.

void* atender_nuevo_cliente(void* fd) {
    

    // recuperamos el FD del cliente
    int cliente_fd = (int)(intptr_t)fd;
    
    
    pthread_detach(pthread_self()); //esto hace que el SO limpie la memoria de este hilo cuando termine la funcion

    int control_loop = 1;
    while (control_loop) {
        int op_code = recibir_operacion(cliente_fd); //syscall bloqueante --> por lo que no se esta haciendo espera activa; es como que el sistema se duerme hasta que reciva 
        t_list* lista;

        switch (op_code) {
            case NUEVA_CPU:

            //funcion que englobe esto: void nueva_cpu()

                CPU* info_cpu = malloc(sizeof(CPU));
                info_cpu->fd = cliente_fd;
                info_cpu->enUso = false;
                
                pthread_mutex_lock(&mutex_cpus);
                list_add(list_suplementarias->cpu, info_cpu);
                pthread_mutex_unlock(&mutex_cpus);
                log_info(logger, "CPU registrada en el socket %d", cliente_fd);


                break;

            case CPU_LIBRE:
                mandar_proceso_cpu();
                break;

            case NUEVA_IO:
                
            //buscar tipo
            //incializar
            //agregar a la lista correpsondiente
                
                
                log_info(logger, "IO registrada en el socket %d", cliente_fd);
                break;

            //mas cases con los opcodes entre la cpu

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




void atender_cpu() {


/*MANDAR PROCESO A LA CPU*/
mandar_proceso_cpu();
    
}


void mandar_proceso_cpu(){
    //nos fijamos si hay cpu libre
    pthread_mutex_lock(&mutex_cpus);
    CPU* cpu_libre = buscar_cpu_libre(list_suplementarias->cpu); //crear funcion: recorer y fijarse si estan en uso
    pthread_mutex_unlock(&mutex_cpus);

    //mandamos pcb
    if (cpu_libre != NULL && !list_is_empty(listasProcesos->rdy)) {
        
        PCB* pcb_a_ejecutar = list_remove(listasProcesos->rdy, 0);
        cambiar_estado_pcb(pcb_a_ejecutar, RNN);
        cpu_libre->enUso = true; // La marcamos como ocupada

        enviar_pcb_cpu(cpu_libre->fd, pcb_a_ejecutar); //HACER FUNCION
        log_info(logger, "PID %d enviado a ejecutar en socket %d", pcb_a_ejecutar->pid, cpu_libre->fd);

        //rr
        if (strcmp(planificacion_algoritmo, "RR") == 0) {
            // ceamos un hilo que espere el Quatum y mande la interrupción --> CHEQUEAR
            pthread_create(&hilo_timer, NULL, hilo_quantum, (void*)pcb_a_ejecutar);
            pthread_detach(hilo_timer);
        }
    }
}










//GESTIÓN DE HILOS -------------------------------------------------------------

void* hilo_quantum(void* arg) {
    t_pcb* pcb = (t_pcb*) arg;
    
    usleep(config_get_int_value(config, "QUANTUM") * 1000); 

    return NULL;
}
















//GESTIÓN DE LAS IO-----------------------------------------------------------

//en caso de CPU: (CPU) y agrego a listas CPU; en caso IO: (STDIN) ; (STDOUT) ; (SLEEP) y se agrega cada una a su lista.  
//vamos a hacer una estructura por cada tipo de cliene. (CPU , IO)



//Hacer estado EXIT;


/*--------------------------LEER IO Y PENSAR QUE HAY QUE HACER--------------------------*/

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

