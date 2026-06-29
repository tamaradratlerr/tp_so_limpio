#ifndef UTILSKS_H_
#define UTILSKS_H_
#include "../../utils/src/global_utils.h"

extern t_config* config;

typedef struct{
    char *ip_km, *puerto_km;
    int conexion_km;
}t_info_km;

typedef struct{
    char *planificacion_algoritmo;
    char *listas_algortimo;
    int intervalo_tarea, tiempo_suspencion;
    bool preemption;
}t_info_config;

//Estructura de dato que identifica todas las listas de los procesos
typedef struct
{
	t_list* new;
	t_list* rnn;
    t_list* rdy;
	t_list* bck;
    t_list* s_bck;
    t_list* s_rdy;
	t_list* ext;

    //Faltan agregar los estados del CheckPoint 3
}t_listas_procesos;


//Estructura de dato que identifica IOs
typedef struct {

    int fd; 
    bool enUso;        //false es que no y true es que si         
    char* nombre;             

} t_IO;

typedef struct{ //Estreuctura de datos que contiene a las listas de CPU y IOs conectadas 
    
    t_list* cpu;
    t_list* io;
    t_list* ms;
    t_list* desalojo;

}t_listas_suplementarias;


typedef struct{

    char* mutex_id;
    int valor;
    PCB* dueño_actual;
    t_list* cola_mutex;

}mutex_cpu;






//algoritmo cola multinivel

typedef enum { FIFO, RR } Algoritmo;
            // 0 = fifo
            // 1 = rr

typedef struct {
    Algoritmo tipo;
    t_list* cola;       
    int quantum;       // solo lo uso si tipo == RR
} ColaPrioridad;

typedef struct {
    ColaPrioridad* niveles;
    int cantidad_niveles;
    bool preemption;    // en el config aparece QUEUE_PREEMPTION que especifica si es con o sin desalojo (únicamente afecta en CMN)
} Planificador_Colas_Multinivel;

typedef struct { // estructura para las cosasa que le mandamos al hilo
    PCB* pcb;
} t_datos_quantum;

/*-----     FUNCIONES      -----*/

void enviarProcesoKM(PCB* pcb, char* path, int fd_km);        
PCB* iniciar_pcb (int PID, int prioridad);
void terminar_pcb (PCB* pcb);
PCB* crearNuevoProceso(char* path, int prioridad, int fd_km);
void enviarProcesoKM(PCB* pcb, char* path, int fd_km);
t_IO* buscar_io_por_nombre(char* nombre_buscado);
t_IO* buscar_io_por_fd(int fd_buscado);
void* list_find_with_context(t_list* lista, bool (*condicion)(void*, void*),void* contexto);
void terminar_programa(t_log* logger, t_config* config, t_info_km info_km);
void iniciar_planificador_CMN(char** algoritmos_array, int total_colas, int quantum_default);
PCB* crearNuevoProceso_mock(char*, int prioridad, int);

/*----- Vars Extern -----*/

extern int contador_pid;
extern t_log* logger;
extern bool mock;
extern int* mem_corrupt_value;
extern int* compactacion_value;
extern int* scheduler_control_loop;

extern Planificador_Colas_Multinivel* planificador;
extern char* ip;
extern char* puerto;
extern char* valor;
extern char* planificacion_algoritmo;
extern char* listas_algortimo;
extern int intervalo_tarea;
extern int tiempo_suspencion;
extern int inicio_todo;


extern t_listas_suplementarias* list_suplementarias;
extern t_listas_procesos* listasProcesos;
extern t_list* lista_bck_io;
extern t_list* lista_mutex;
extern t_info_km info_km;
extern t_info_config info_config;
extern t_datos_quantum* datos_quantum;

/* --- SEMÁFOROS (También son variables globales) --- */

extern pthread_mutex_t sem_procesos_new;
extern pthread_mutex_t sem_procesos_ready;
extern pthread_mutex_t sem_procesos_running;
extern pthread_mutex_t sem_procesos_block;
extern pthread_mutex_t sem_procesos_exit;
extern pthread_mutex_t sem_procesos_s_block;
extern pthread_mutex_t sem_procesos_s_ready;
extern pthread_mutex_t sem_procesos_s_desalojo;

extern pthread_mutex_t mutex_cpus;
extern pthread_mutex_t mutex_ios;
extern pthread_t hilo_timer;
extern pthread_mutex_t mutex_ready;
extern pthread_mutex_t mutex_simulados; 
extern pthread_mutex_t mutex_cola_exec;


#endif