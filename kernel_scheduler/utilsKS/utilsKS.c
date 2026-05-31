#include "utilsKS.h"

void crearNuevoProceso(t_log* logger, char* path, int fd_km) {
    
    PCB* nuevoPcb = iniciar_pcb(contador_pid);
	
    enviarProcesoKM(nuevoPcb, path, fd_km);

	contador_pid++;
}

/*------     PCB     -----*/

PCB* iniciar_pcb (int PID){

	PCB* nuevo_pcb = malloc(sizeof(PCB));
	nuevo_pcb->data.PID = PID;
	nuevo_pcb->estado_pcb = NEW;
	nuevo_pcb->estado_anterior = NULL;

	return nuevo_pcb;
}

PCB* crearNuevoProceso(t_log* logger, char* path, int fd_km) {
    
    
    PCB* nuevoPcb = iniciar_pcb(contador_pid);
	
    enviarProcesoKM(nuevoPcb, path, fd_km);

	contador_pid++;
	
	return nuevoPcb;
}

void enviarProcesoKM(PCB* pcb, char* path, int fd_km){
	
	t_paquete* paquete = crear_paquete(ENVIAR_PROCESO);
	
	agregar_a_paquete(paquete, pcb->data.PID, sizeof(int));

	agregar_a_paquete(paquete, &path, sizeof(char*));
	

	enviar_paquete(paquete, fd_km);
    eliminar_paquete(paquete);

}

void terminar_pcb (PCB* pcb){
	free(pcb);
    //para liberar el espacio --> no es de lógica
	return 0;
}

/*-----     IO      -----*/


// Auxiliar para buscar la IO por su FILE DESCRIPTOR (Socket)
IO* buscar_io_por_fd(int fd_buscado) {
    IO* io_encontrada = NULL;

    pthread_mutex_lock(&mutex_ios);
    
    for (int i = 0; i < list_size(list_suplementarias->io); i++) {
        IO* una_io = list_get(list_suplementarias->io, i);
        
        if (una_io->fd == fd_buscado) {
            io_encontrada = una_io;
            break;
        }
    }
    
    pthread_mutex_unlock(&mutex_ios);

    if (io_encontrada == NULL) {
        printf("Kernel Error: No se encontró la interfaz de IO con FD: %d", fd_buscado);
    }

    return io_encontrada;
}