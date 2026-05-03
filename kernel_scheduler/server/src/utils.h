#ifndef UTILS_H_
#define UTILS_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<string.h>
#include<assert.h>
#include<utilsKS/utilsKS.h>

#define PUERTO "4444"

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
	t_list* new;
	t_list* rnn;
	t_list* bck;
	t_list* ext;
    t_list* rdy;
    //Faltan agregar los estados del CheckPoint 3

}listas_procesos;


extern t_log* logger;

void* recibir_buffer(int*, int);

int iniciar_servidor(void);

int esperar_cliente(int);

t_list* recibir_paquete(int);

void recibir_mensaje(int);

int recibir_operacion(int);

#endif /* UTILS_H_ */
