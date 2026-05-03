#ifndef UTILSKS_H_
#define UTILSKS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <commons/config.h>

t_config* config = NULL;

typedef enum
{
	MENSAJE,
	PAQUETE, 

	//con la CPU
	DISPATCH, 
	INTERRUPT,

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
    int PID;
    int PPID;
    int UID;
    estado estado_pcb;
    estado estado_anterior;
    //Creto que estado anterior puede servir para sacar de esa lista

}PCB;

//Tipo de dato que identifica el estado del Proceso (PCB)
typedef enum 
{
    NEW,
    RNN,
    BCK,
    EXT,
    RDY
    //Faltan agregar los estados del CheckPoint 3
}estado;

//Estructura de dato que identifica todas las listas de los procesos
typedef struct
{
	t_list* niu;
	t_list* rnn;
	t_list* bck;
	t_list* ext;
    //Faltan agregar los estados del CheckPoint 3

}listas_procesos;

//Estructura de dato que identifica CPUs
typedef struct
{
	int idCpu;
    int conexionCPu;
    //se va viendo

}listas_cpu;


typedef enum
{
	FIFO,
    RR,
    NM
    
}algortimoEnUso;

typedef struct{
    int CPU_ID;
}CPU;



typedef struct{
    int IO_ID;
    IO_OPCODE tipo;
    int estado = 0; //valor cero indica que no esta en uso;    
}IO;

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