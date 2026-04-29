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

//Funcion que inicializa todas las listas de los Procesos//
listas_procesos* Iniciar_listas_procesos (void){
	
	listas_procesos* l_procesos;

	l_procesos->new = list_create();
	l_procesos->rnn = list_create();
	l_procesos->bck = list_create();
	l_procesos->ext = list_create();

	return l_procesos;
};

void terminar_listas_procesos (listas_procesos* listas_procesos){
	list_destroy(listas_procesos->new);
	list_destroy(listas_procesos->rnn);
	list_destroy(listas_procesos->bck);
	list_destroy(listas_procesos->ext);

	return 0;
}

//Funcion que a suma un PCB a su lista correspondiente segun su ESTADO ACTUAL.
void agregar_proceso_lista (PCB* pcb, listas_procesos* l_procesos){

	sacar_proceso_lista();//Funcion pensada para tomar estado anterior y sacarlo de esa lista.
	switch (pcb->estado_pcb)
	{
	case 1: //NEW
		list_add(l_procesos->new, pcb);
	case 2: //RUNNING
		list_add(l_procesos->rnn, pcb);
	case 3: //BLOCK
		list_add(l_procesos->bck, pcb);
	case 4: //EXIT
		list_add(l_procesos->ext, pcb);

		break;
	
	default:
		break;
	}

}


//Funcion que inicia un nuevo PCB en estado NEW
PCB* iniciar_pcb (int PID, int PPID, int UID){

	PCB* nuevo_pcb = malloc(sizeof(PCB));
	nuevo_pcb->PID = PID;
	nuevo_pcb->PPID = PPID;
	nuevo_pcb->UID = UID;
	nuevo_pcb->estado_pcb = NEW;
	nuevo_pcb->estado_anterior = NULL;

	return nuevo_pcb;
}

//Funcion que termina un pcb liberando su memoria
void terminar_pcb (PCB* pcb){
	free(pcb);

	return 0;
}