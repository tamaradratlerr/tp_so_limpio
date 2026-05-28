#ifndef UTILSKS_H_
#define UTILSKS_H_

#include "../../utils/src/global_utils.h"

int contador_pid = 0;


typedef struct{
    int km, cpu, io;
} t_conexion;

char* ip;
char* puerto;
char* valor;
char *ip_km, *puerto_km, *planificacion_algoritmo, *listas_algortimo;	int intervalo_tarea, tiempo_suspencion;	int intervalo_tarea, tiempo_suspencion;
//t_log* logger; ME PARAECE QUE EL LOGGER TIENE QUE SER UNO PARA CLIENTE Y OTRO PARA SERVIDOR

listas_procesos* listasProcesos; //Lista de PCBs segun estado (GLOBAL)
listas_suplementarias* list_suplementarias; //Lista de CPUs y IOs (GLOBAL)
t_list* lista_mutex;




//Estructura de dato que identifica todas las listas de los procesos
typedef struct
{
	t_list* new;
	t_list* rnn;
    t_list* rdy;
	t_list* bck;
	t_list* ext;

    //Faltan agregar los estados del CheckPoint 3
}listas_procesos;


//Estructura de dato que identifica IOs
typedef struct {

    int fd; 
    bool enUso;                 
    char* nombre;             

} IO;

typedef struct{ //Estreuctura de datos que contiene a las listas de CPU y IOs conectadas 
    
    t_list* cpu;
    t_list* io;
    t_list* io_ready;

}listas_suplementarias;


typedef enum
{
	FIFO,
    RR,
    NM
    //No lo estamos Usado ahora, deberiamos cambiar cuando leemos el config y darle valor de esta estructura de datos
}algortimoEnUso;

typedef struct {
    PCB* pcb;
    int tiempo_ms;
} t_timer_args;

typedef enum
{
    IO_STDIN,
    IO_STDOUT,
    IO_SLEEP

}IO_OPCODE;

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

#endif