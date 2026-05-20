#ifndef UTILSKS_H_
#define UTILSKS_H_

#include "../../utils/src/global_utils.h"

t_config* config = NULL;


int conexion;
char* ip;
char* puerto;
char* valor;
char *ip_km, *puerto_km, *planificacion_algoritmo, *listas_algortimo;	int intervalo_tarea, tiempo_suspencion;	int intervalo_tarea, tiempo_suspencion;
//t_log* logger; ME PARAECE QUE EL LOGGER TIENE QUE SER UNO PARA CLIENTE Y OTRO PARA SERVIDOR

listas_procesos* listasProcesos; //Lista de PCBs segun estado (GLOBAL)
listas_suplementarias* list_suplementarias; //Lista de CPUs y IOs (GLOBAL)


typedef struct {

    PCB* pcb;
    IO_OPCODE tipo_operacion;   
    int tiempo_sleep;     
    uint32_t dir_fisica;  
    uint32_t tamano;
         
} t_pedido_io;


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
    t_queue* cola_bloqueados; // cola de las so-commons para guardar los t_pedido_io*

} IO;

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
    IO_STDIN,
    IO_STDOUT,
    IO_SLEEP

}IO_OPCODE;


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

/*-----     FUNCIONES      -----*/

PCB* iniciar_pcb (int PID, int PPID, int UID);
void terminar_pcb (PCB* pcb);

IO* buscar_io_por_nombre(char* nombre_buscado);
IO* buscar_io_por_fd(int fd_buscado);

#endif