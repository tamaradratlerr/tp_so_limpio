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




void* recibir_buffer(int*, int);

int iniciar_servidor(void);

int esperar_cliente(int);

t_list* recibir_paquete(int);

void recibir_mensaje(int);

int recibir_operacion(int);

#endif /* UTILS_H_ */
