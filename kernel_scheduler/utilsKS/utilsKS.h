#ifndef UTILSKS_H_
#define UTILSKS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <commons/config.h>

t_config* config = NULL;
bool g_server = FALSE;

int conexion;
char* ip;
char* puerto;
char* valor;
char *ip_km, *puerto_km, *planificacion_algoritmo, *listas_algortimo;	int intervalo_tarea, tiempo_suspencion;	int intervalo_tarea, tiempo_suspencion;
//t_log* logger; ME PARAECE QUE EL LOGGER TIENE QUE SER UNO PARA CLIENTE Y OTRO PARA SERVIDOR

typedef enum //Todos los Posibles intercambios de informacion con la CPU, IO y KM.
{
	MENSAJE,
	PAQUETE, 

	//con la CPU
	NUEVA_CPU,
    CPU_LIBRE,
    FIN_PROCESO,
    DESALOJO,
    PCB,

    //syscalls de la CPU --- Descripcion de cada una esta en el TP.
    MUTEX_CREATE,
    MUTEX_LOCK,
    MUTEX_UNLOK,
    MEM_ALLOC,
    MEM_FREE,
    INIT_PROC,
    EXIT,


    //con el KM
    MEM_CORRUPT, //cortar todo con esta

	//con la IO
    NUEVA_IO,
    IO_LIBRE,
	SLEEP, 
	STDIN,
	STDOUT
}op_code;


//Tipo de dato que ingresa desde el kernel memory
typedef struct {
    int PID, PPID, UID;
} t_infoProceso;

//Tipo de dato que va a adoptar el PCB
typedef struct 
{
    t_infoProceso data;
    estado estado_pcb;
    estado estado_anterior;
    int fd_cpu; //socket cpu para que se sepa en q cpu se esta ejecutando

}PCB;

//Tipo de dato que identifica el estado del Proceso (PCB)
typedef enum 
{
    NEW,
    RNN,
    RDY,
    BCK,
    EXT
    
    //Faltan agregar los estados del CheckPoint 3
}estado;

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

//Estructura de dato que identifica CPUs
typedef struct{
    int fd; 
    bool enUso; // EN USO = TRUE --- LIBRE = FALSE
}CPU;

//Estructura de dato que identifica IOs
typedef struct{
    int fd; 
    bool enUso; // EN USO = TRUE --- LIBRE = FALSE   
}IO;


typedef struct{ //Estreuctura de datos que contiene a las listas de CPU y IOs conectadas 
    t_list* cpu;
    t_list* io;
}listas_suplementarias;



typedef enum
{
	FIFO,
    RR,
    NM
    //No lo estamos Usado ahora, deberiamos cambiar cuando leemos el config y darle valor de esta estructura de datos
}algortimoEnUso;







typedef enum
{
    STDIN,
    STDOUT,
    SLEEP

}IO_OPCODE;


//VARIABLES GLOBALES-------------------------
extern t_log* logger;

listas_procesos* listasProcesos; //Lista de PCBs segun estado
listas_suplementarias* list_suplementarias; //Lista de CPUs y IOs


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
//FALTA PONER LAS DEMAS MUTEX?


/*analizar que tipo de semáforo/mutex*/

PCB* iniciar_pcb (int PID, int PPID, int UID);
void terminar_pcb (PCB* pcb);

#endif