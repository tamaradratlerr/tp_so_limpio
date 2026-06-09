#include "utilsKS.h"

char* planificacion_algoritmo = NULL;
char* listas_algortimo = NULL;
t_config* config;

int intervalo_tarea = 0;
int tiempo_suspencion = 0;
int inicio_todo = false;

pthread_t hilo_timer;

int contador_pid = 0;
t_log* logger = NULL;

t_listas_procesos* listasProcesos= NULL; //Lista de PCBs segun estado (GLOBAL)
t_listas_suplementarias* list_suplementarias= NULL; //Lista de CPUs y IOs (GLOBAL)
t_list* lista_bck_io = NULL;
t_list* lista_mutex= NULL;
t_info_km info_km;
t_info_config info_config;
Planificador_Colas_Multinivel* planificador;
t_datos_quantum* datos_quantum;


/* Semaforos tipo Mutex*/
pthread_mutex_t sem_procesos_new = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sem_procesos_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sem_procesos_running = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sem_procesos_block = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sem_procesos_exit = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_cpus = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ios = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_simulados = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_exec = PTHREAD_MUTEX_INITIALIZER;
//FALTA PONER LAS DEMAS MUTEX?




/*------     PCB     -----*/

PCB* iniciar_pcb (int PID){

	PCB* nuevo_pcb = malloc(sizeof(PCB));
	nuevo_pcb->data.PID = PID;
	nuevo_pcb->estado_pcb = NEW;
	nuevo_pcb->estado_anterior = NO_ESTADO;

	return nuevo_pcb;
}

PCB* crearNuevoProceso(t_log* logger, char* path, int fd_km) {
    
    PCB* nuevoPcb = iniciar_pcb(contador_pid);
    log_info (logger, "## PID [%d] Se crea el proceso - Estado NEW", contador_pid);

    enviarProcesoKM(nuevoPcb, path, fd_km);
	contador_pid++;

	return nuevoPcb;
}
void* list_find_with_context(
        t_list* lista,
        bool (*condicion)(void*, void*),
        void* contexto)
{
    for(int i = 0; i < list_size(lista); i++)
    {
        void* elemento = list_get(lista, i);

        if(condicion(elemento, contexto))
        {
            return elemento;
        }
    }

    return NULL;
}

void enviarProcesoKM(PCB* pcb, char* path, int fd_km){
	
	t_paquete* paquete = crear_paquete(ENVIAR_PROCESO);
	
    agregar_a_paquete(paquete, &(pcb->data.PID), sizeof(int));
    agregar_a_paquete(paquete, path, strlen(path) + 1);	

	enviar_paquete(paquete, fd_km);
    eliminar_paquete(paquete);

}

void terminar_pcb (PCB* pcb){
	free(pcb);
    //para liberar el espacio --> no es de lógica
	
}

/*-----     IO      -----*/


// Auxiliar para buscar la IO por su FILE DESCRIPTOR (Socket)
t_IO* buscar_io_por_fd(int fd_buscado) {
    t_IO* io_encontrada = NULL;

    pthread_mutex_lock(&mutex_ios);
    
    for (int i = 0; i < list_size(list_suplementarias->io); i++) {
        t_IO* una_io = list_get(list_suplementarias->io, i);
        
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

/*----- Auxiliares -----*/

void terminar_programa( t_log* logger, t_config* config, t_info_km info_km)
{
	log_destroy(logger);
	config_destroy(config);

    liberar_conexion (info_km.conexion_km);
	
}

/*   PLANIFICACIÓN     */

void iniciar_planificador_CMN(char** algoritmos_array, int total_colas, int quantum_default) {
    planificador = malloc(sizeof(Planificador_Colas_Multinivel));
    planificador->cantidad_niveles = total_colas;
    planificador->preemption = info_config.preemption;
    planificador->niveles = malloc(sizeof(ColaPrioridad) * total_colas);

    for (int i = 0; i < total_colas; i++) {
        planificador->niveles[i].cola = list_create(); 
        
        if (strcmp(algoritmos_array[i], "FIFO") == 0) {
            planificador->niveles[i].tipo = FIFO;
            planificador->niveles[i].quantum = 0; // en este caso no importa el quanrtum
        } else {
            planificador->niveles[i].tipo = RR;
            planificador->niveles[i].quantum = quantum_default;
        }
    }
}