#ifndef UTILSKS_H_
#define UTILSKS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <commons/config.h>

t_config* config = NULL;

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
	DISPATCH, 
	INTERRUPT,
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
	t_list* niu;
	t_list* rnn;
    t_list* rdy;
	t_list* bck;
	t_list* ext;

    //Faltan agregar los estados del CheckPoint 3
}listas_procesos;

//Estructura de dato que identifica CPUs
typedef struct{
    int CPU_ID;
    int enUso; // EN USO = 1 --- LIBRE = 0
    //Ver que mas data nos puede interesar
}CPU;

//Estructura de dato que identifica IOs
typedef struct{
    int IO_ID;
    IO_OPCODE tipo;
    int enUso; // EN USO = 1 --- LIBRE = 0    
}IO;


typedef struct{ //Estreuctura de datos que contiene a las listas de CPU y IOs conectadas 
    t_list* cpu;
    t_list* io;
}l_suplementarias;

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


PCB* iniciar_pcb (int PID, int PPID, int UID);
void terminar_pcb (PCB* pcb);

#endif