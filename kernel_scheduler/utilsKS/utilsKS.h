#ifndef UTILSKS_H_
#define UTILSKS_H_

#include "../../utils/src/global_utils.h"

int contador_pid = 0;


t_log* logger; 

t_listas_procesos* listasProcesos; //Lista de PCBs segun estado (GLOBAL)
t_listas_suplementarias* list_suplementarias; //Lista de CPUs y IOs (GLOBAL)
t_list* lista_mutex;
t_info_km info_km;
t_info_config info_config;

typedef struct{
    char *ip_km, *puerto_km;
    int conexion_km;
}t_info_km;

typedef struct{
    char* *planificacion_algoritmo, *listas_algortimo;
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
    bool enUso;                 
    char* nombre;             

} IO;

typedef struct{ //Estreuctura de datos que contiene a las listas de CPU y IOs conectadas  --> CAMBIAR ESTO Y APLICAR CAMBIOS
    
    t_list* cpu;
    t_list* cpu_libre;
    t_list* io;
    t_list* io_ready;

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


/* Semaforos tipo Mutex*/
pthread_mutex_t sem_procesos_new; //Semaforo para lista de NEWS
pthread_mutex_t sem_procesos_ready; //Semaforo para lista de READYS
pthread_mutex_t sem_procesos_running; //Semaforo para lista de RUNNINGS
pthread_mutex_t sem_procesos_block; //Semaforo para lista de BLOCKS
pthread_mutex_t sem_procesos_exit; //Semaforo para lista de READYS EXITS


pthread_mutex_t mutex_cpus;
pthread_mutex_t mutex_ios;
pthread_t hilo_timer;
pthread_mutex_t mutex_ready;
pthread_mutex_t mutex_simulados; 
pthread_mutex_t mutex_cola_exec;
//FALTA PONER LAS DEMAS MUTEX?

/*-----     FUNCIONES      -----*/

PCB* iniciar_pcb (int PID, int PPID, int UID);
void terminar_pcb (PCB* pcb);

IO* buscar_io_por_nombre(char* nombre_buscado);
IO* buscar_io_por_fd(int fd_buscado);


/* --- DECLARACIONES EXTERNAS (Aquí van los extern) --- */

extern int contador_pid;
extern char* ip;
extern char* puerto;
extern char* valor;
extern char* planificacion_algoritmo;
extern char* listas_algortimo;
extern int intervalo_tarea;
extern int tiempo_suspencion;

extern t_listas_procesos* listasProcesos; 
extern t_listas_suplementarias* list_suplementarias; 
extern t_list* lista_mutex;
extern t_info_km info_km;

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