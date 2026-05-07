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
    FIN_PROCESO,

    //syscalls de la CPU --- Descripcion de cada una esta en el TP.
    MUTEX_CREATE,
    MUTEX_LOCK,
    MUTEX_UNLOK,
    MEM_ALLOC,
    MEM_FREE,
    INIT_PROC,
    EXIT,

	//con la IO
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
    int fd; //hay un socket por cpu q se conecta
    bool enUso; // EN USO = 1 --- LIBRE = 0
    //Ver que mas data nos puede interesar
}CPU;

//Estructura de dato que identifica IOs
typedef struct{
    IO_OPCODE tipo;
    int fd; //hay un socket por io q se conecta
    bool enUso; // EN USO = 1 --- LIBRE = 0    
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

typedef struct 
{
    t_list* STDIN;
    t_list* STDOUT;
    t_list* SLEEP;
}l_IO;

//VARIABLES GLOBALES-------------------------
extern t_log* logger;

listas_procesos* listasProcesos; //Lista de PCBs segun estado
listas_suplementarias* list_suplementarias; //Lista de CPUs y IOs

sem_t sem_procesos_new; //Semaforo para lista de NEWS
sem_t sem_procesos_ready; //Semaforo para lista de READYS
sem_t sem_procesos_running; //Semaforo para lista de RUNNINGS
sem_t sem_procesos_block; //Semaforo para lista de BLOCKS
sem_t sem_procesos_exit; //Semaforo para lista de READYS EXITS

sem_t sem_cpus_libres; //Semaforo para lista de CPUs


pthread_mutex_t mutex_cpus; 
pthread_t hilo_timer;
pthread_mutex_t mutex_ready; 
//FALTA PONER LAS DEMAS MUTEX


/*analizar que tipo de semáforo/mutex*/

PCB* iniciar_pcb (int PID, int PPID, int UID);
void terminar_pcb (PCB* pcb);

#endif