#include "utilsKS.h"

/*------     PCB     -----*/

PCB* iniciar_pcb (int PID, int PPID, int UID){

	PCB* nuevo_pcb = malloc(sizeof(PCB));
	nuevo_pcb->data.PID = PID;
	nuevo_pcb->data.PPID = PPID;
	nuevo_pcb->data.UID = UID;
	nuevo_pcb->estado_pcb = NEW;
	nuevo_pcb->estado_anterior = NULL;

	return nuevo_pcb;
}

void terminar_pcb (PCB* pcb){
	free(pcb);

	return 0;
}

/*-----     IO      -----*/

// Auxiliar para buscar la IO por su NOMBRE texto
IO* buscar_io_por_nombre(char* nombre_buscado) {
    IO* io_encontrada = NULL;

    pthread_mutex_lock(&mutex_ios);
    
    // Recorremos la lista elemento por elemento usando las macros de las commons
    for (int i = 0; i < list_size(list_suplementarias->io); i++) {
        IO* una_io = list_get(list_suplementarias->io, i);
        
        if (strcmp(una_io->nombre, nombre_buscado) == 0) {
            io_encontrada = una_io;
            break; // Ya la encontramos, salimos del for
        }
    }
    
    pthread_mutex_unlock(&mutex_ios);

    if (io_encontrada == NULL) {
        log_error(logger, "Kernel Error: No se encontró la interfaz de IO con nombre: %s", nombre_buscado);
    }

    return io_encontrada;
}

void crearNuevoProceso(t_log* logger, char* path, int fd_km) {
    
    
    PCB* nuevoPcb = iniciar_pcb(contador_pid, 0, 0);
	
    enviarProcesoKM(nuevoPcb, path, fd_km);

	contador_pid++;
}


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
        log_error(logger, "Kernel Error: No se encontró la interfaz de IO con FD: %d", fd_buscado);
    }

    return io_encontrada;
}