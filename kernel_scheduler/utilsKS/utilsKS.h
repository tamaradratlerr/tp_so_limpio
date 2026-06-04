#ifndef UTILSKS_H_
#define UTILSKS_H_
#include "../../utils/src/global_utils.h"

t_config* config;

typedef struct{
    char *ip_km, *puerto_km;
    int conexion_km;
}t_info_km;

typedef struct{
    char *planificacion_algoritmo;
    char *listas_algortimo;
    int intervalo_tarea, tiempo_suspencion;
}t_info_config;

//Estructura de dato que identifica todas las listas de los procesos
typedef struct
{
	t_list* new;
	t_list* rnn;
    t_list* rdy;
	t_list* bck;
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

}t_listas_suplementarias;


typedef enum
{
	FIFO,
    RR,
    CMN
    //No lo estamos Usado ahora, deberiamos cambiar cuando leemos el config y darle valor de esta estructura de datos
}algortimoEnUso;


typedef struct{

    char* mutex_id;
    int valor;

}mutex_cpu;



/*-----     FUNCIONES      -----*/
void enviarProcesoKM(
        PCB* pcb,
        char* path,
        int fd_km);
        
PCB* iniciar_pcb (int PID);
void terminar_pcb (PCB* pcb);
PCB* crearNuevoProceso(t_log* logger, char* path, int fd_km);
void enviarProcesoKM(PCB* pcb, char* path, int fd_km);
t_IO* buscar_io_por_nombre(char* nombre_buscado);
t_IO* buscar_io_por_fd(int fd_buscado);
void* list_find_with_context(
        t_list* lista,
        bool (*condicion)(void*, void*),
        void* contexto);

extern int contador_pid;
extern t_log* logger;


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
extern t_list* lista_mutex;
extern t_info_km info_km;
extern t_info_config info_config;

/* --- SEMÁFOROS (También son variables globales) --- */

extern pthread_mutex_t sem_procesos_new;
extern pthread_mutex_t sem_procesos_ready;
extern pthread_mutex_t sem_procesos_running;
extern pthread_mutex_t sem_procesos_block;
extern pthread_mutex_t sem_procesos_exit;

extern pthread_mutex_t mutex_cpus;
extern pthread_mutex_t mutex_ios;
extern pthread_t hilo_timer;
extern pthread_mutex_t mutex_ready;
extern pthread_mutex_t mutex_simulados; 
extern pthread_mutex_t mutex_cola_exec;


#endif