#include "utilsKS.h"

char* planificacion_algoritmo = NULL;
char* listas_algortimo = NULL;
t_config* config;

int intervalo_tarea = 0;
int tiempo_suspencion = 0;
int inicio_todo = false;
int mem_corrupt_value = 0;
int compactacion_value = 0;
int control_compac = 0;
int scheduler_control_loop = 1;


//ACA PUSE EL MOCK km
bool mock = false;

/*FALSE => Se ejecuta Normalmente
  TRUE => Se ejecuta sin KM (para realizar pruebas)
*/


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
pthread_mutex_t sem_procesos_s_block = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sem_procesos_s_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sem_procesos_s_desalojo = PTHREAD_MUTEX_INITIALIZER;


pthread_mutex_t mutex_cpus = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ios = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_simulados = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_conexion_km = PTHREAD_MUTEX_INITIALIZER;

sem_t sem_hay_ready;
sem_t sem_hay_s_ready;
sem_t sem_compactacion;
sem_t sem_rnn_vacio;
sem_t sem_io_vacio;


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

    sem_init(&sem_hay_ready, 0, 0);
    sem_init(&sem_hay_s_ready, 0, 0);
    sem_init(&sem_compactacion, 0, 1);
    sem_init(&sem_rnn_vacio, 0, 0);
    sem_init(&sem_io_vacio, 0, 0);


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

    sem_destroy(&sem_hay_ready);
    sem_destroy(&sem_hay_s_ready);
    sem_destroy(&sem_compactacion);
    sem_destroy(&sem_rnn_vacio);
    sem_destroy(&sem_io_vacio);
}

void iniciar_listas_suple()
{
    list_suplementarias = malloc(sizeof(t_listas_suplementarias));

    list_suplementarias->cpu = list_create();
    list_suplementarias->io = list_create();
    list_suplementarias->ms = list_create();
    list_suplementarias->desalojo = list_create();

    lista_mutex = list_create();

    lista_bck_io = list_create();
}

void eliminar_listas_suple (){ /* Funcion que destruye las listas de CPUs y IOs (Suplmentarias)*/
    
    list_destroy(list_suplementarias->cpu);
    list_destroy(list_suplementarias->io);
    list_destroy(list_suplementarias->ms);
    list_destroy(list_suplementarias->desalojo);
    
    list_destroy(lista_mutex);
    
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

        sem_post(&sem_hay_s_ready);

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
        sem_post(&sem_hay_ready);

        return posicion; //Devuelve la posicion por si nos sirve para algo en un futuro (no lo hace en agregar lista ready, se podria hacer)

        break;
	
	default:
        log_error(logger, "Erro al identificar estado de un pcb (funcion agregar proceso a lista)");
        return -1;
		break;

	}

}
 op_code eliminar_proceso_Lista(PCB* pcb)
{
    bool removed = false;

    log_info(logger, "Intentando eliminar PID <%d> del estado <%d>", pcb->data.PID, pcb->estado_anterior);

    switch (pcb->estado_anterior) {

        case NEW:
            pthread_mutex_lock(&sem_procesos_new);
            removed = list_remove_element(listasProcesos->new, pcb);
            pthread_mutex_unlock(&sem_procesos_new);

            log_info(logger, "PID <%d> eliminado de NEW: %s",
                     pcb->data.PID, removed ? "SI" : "NO");
            break;

        case RNN:
            pthread_mutex_lock(&sem_procesos_running);
            removed = list_remove_element(listasProcesos->rnn, pcb);

            bool rnn_quedo_vacio = list_is_empty(listasProcesos->rnn);
            pthread_mutex_unlock(&sem_procesos_running);

            log_info(logger, "PID <%d> eliminado de RUNNING: %s",
                     pcb->data.PID, removed ? "SI" : "NO");

            if (rnn_quedo_vacio) {
                log_info(logger, "RUNNING quedó vacío");
                sem_post(&sem_rnn_vacio);
            }

            break;

        case BCK:
            pthread_mutex_lock(&sem_procesos_block);
            removed = list_remove_element(listasProcesos->bck, pcb);
            pthread_mutex_unlock(&sem_procesos_block);

            log_info(logger, "PID <%d> eliminado de BLOCK: %s",
                     pcb->data.PID, removed ? "SI" : "NO");
            break;

        case EXT:
            pthread_mutex_lock(&sem_procesos_exit);
            removed = list_remove_element(listasProcesos->ext, pcb);
            pthread_mutex_unlock(&sem_procesos_exit);

            log_info(logger, "PID <%d> eliminado de EXIT: %s",
                     pcb->data.PID, removed ? "SI" : "NO");
            break;

        case S_BCK:
            pthread_mutex_lock(&sem_procesos_s_block);
            removed = list_remove_element(listasProcesos->s_bck, pcb);
            pthread_mutex_unlock(&sem_procesos_s_block);

            log_info(logger, "PID <%d> eliminado de S_BLOCK: %s",
                     pcb->data.PID, removed ? "SI" : "NO");
            break;

        case S_RDY:
            pthread_mutex_lock(&sem_procesos_s_ready);
            removed = list_remove_element(listasProcesos->s_rdy, pcb);
            pthread_mutex_unlock(&sem_procesos_s_ready);

            log_info(logger, "PID <%d> eliminado de S_READY: %s",
                     pcb->data.PID, removed ? "SI" : "NO");

            if (removed)
                sem_wait(&sem_hay_s_ready);

            break;

        case RDY:
            pthread_mutex_lock(&sem_procesos_ready);
            removed = list_remove_element(listasProcesos->rdy, pcb);
            pthread_mutex_unlock(&sem_procesos_ready);

            log_info(logger, "PID <%d> eliminado de READY: %s",
                     pcb->data.PID, removed ? "SI" : "NO");

            if (removed)
                sem_wait(&sem_hay_ready);

            break;

        default:
            log_error(logger, "Estado anterior desconocido");
            return NOTOK;
    }

    if (!removed) {
        log_error(logger,
                  "No se pudo eliminar el PID <%d> del estado <%d>",
                  pcb->data.PID,
                  pcb->estado_anterior);
        return NOTOK;
    }

    log_info(logger, "PID <%d> eliminado correctamente", pcb->data.PID);

    return OK;
}

int agregar_lista_ready(PCB* pcb)
{
    int posicion;

    if (strcmp(info_config.planificacion_algoritmo, "FIFO") == 0 ||
        strcmp(info_config.planificacion_algoritmo, "RR") == 0)
    {
        posicion = ready_FIFO(pcb);
    }
    else if (strcmp(info_config.planificacion_algoritmo, "CMN") == 0)
    {
        posicion = ready_CMN(pcb);
    }
    else
    {
        log_error(logger,
                  "Error al identificar algoritmo de planificación");
        return -1;
    }

    return posicion;
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


/*------     PCB     -----*/

PCB* iniciar_pcb (int PID, int prioridad){

	PCB* nuevo_pcb = malloc(sizeof(PCB));
	nuevo_pcb->data.PID = PID;
	nuevo_pcb->estado_pcb = NEW;
	nuevo_pcb->estado_anterior = NO_ESTADO;
    nuevo_pcb->data.prioridad_original = prioridad;
    nuevo_pcb->data.prioridad = prioridad;
    nuevo_pcb -> mutex_tomados = list_create();
    nuevo_pcb->esperando_io = false;
    
    agregar_proceso_lista(nuevo_pcb);

	return nuevo_pcb;
}

PCB*   crearNuevoProceso(char* path, int prioridad, int fd_km) {
    
    PCB* nuevoPcb = iniciar_pcb(contador_pid, prioridad);
    log_info (logger, "## Se crea el proceso PID [%d] - Estado [NEW]", contador_pid);

    enviarProcesoKM(nuevoPcb, path, fd_km);
	contador_pid++;

	return nuevoPcb;
}

void* list_find_with_context(t_list* lista, bool (*condicion)(void*, void*), void* contexto)
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
	
	enviar_op_code(ks_INIT_PROC,info_km.conexion_km);
    	
    enviar_pid(pcb->data.PID,info_km.conexion_km);
    enviar_mensaje(path,info_km.conexion_km);
    
    log_info(logger, "Fue Enviado el Proceso PID [%d] a la Kernel Memory",pcb->data.PID);

}

void terminar_pcb (PCB* pcb){
	free(pcb);
    //para liberar el espacio --> no es de lógica
	
}

void cambiar_estado_pcb(PCB* pcb, estado nuevoEstado){ /*Funcion que cambia el estado de un PCB*/
    
    //Se le podria agregar semaforos pero antes y dsp de llamar a la funcion
    
    pcb -> estado_anterior = (pcb ->estado_pcb);
    pcb ->estado_pcb = nuevoEstado;
    
    log_info (logger, "## PID:[%d] Pasa del Estado [%s] al estado [%s]",pcb->data.PID,nombre_estado(pcb->estado_anterior),nombre_estado(pcb->estado_pcb));
}

char* nombre_estado (estado sto){

    switch (sto)
    {
    case NEW: return "NEW"; break;
    case RDY: return "READY"; break;
    case RNN: return "RUNNING"; break;
    case BCK: return "BLOCK"; break;
    case S_BCK: return "SUSPENDED BLOCK"; break;
    case S_RDY: return "SUSPENDED READY"; break;
    case EXT: return "EXIT"; break;
    case NO_ESTADO: return "NO ESTADO"; break;
    default:
        return "NO SE PUDO IDENTIFICAR";
        break;
    }
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

    terminar_listas_procesos();
    eliminar_listas_suple();

    liberar_conexion (info_km.conexion_km);
	
}

/* ------------ MOCK ------------*/
PCB* crearNuevoProceso_mock(char*, int prioridad, int){
    
    PCB* nuevoPcb = iniciar_pcb(contador_pid, prioridad);
    log_info (logger, "## Se crea el proceso PID [%d] - Estado NEW [MOCK]", contador_pid);

    /*Se evita Enviar el Contexto a la KM*/
    log_info (logger, "Se le envia el Nuevo Proceso PID [%d] a la KM [MOCK]",contador_pid);
	contador_pid++;

	return nuevoPcb;
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