#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include "utils.h"
#include <pthread.h>   
#include <semaphore.h> 




/*Funciones de SERVIDOR*/
void iterator(char* value);

listas_procesos* Iniciar_listas_procesos (void);

void terminar_listas_procesos (listas_procesos* listas);

PCB* iniciar_pcb (int PID, int PPID, int UID );

void terminar_pcb (PCB* pcb);



//INICIALIZAR Y FINALIZAR LISTAS--------------------------------------

listas_procesos* Iniciar_listas_procesos (void); //Funcion que inicializa todas las listas de los Procesos//
void terminar_listas_procesos ();//Funcion que elimina todas las listas de los procesos//

void agregar_proceso_lista (PCB* pcb); //Funcion que a suma un PCB a su lista correspondiente segun su ESTADO ACTUAL.
void eliminar_proceso_Lista (PCB* pcb ); //Funcion que elimina un PCB de su lista correspondiente segun su ESTADO ANTERIOR.

//GESTIÓN DE LISTAS--------------------------------------

void agregar_proceso_lista (PCB* pcb);
void eliminar_proceso_Lista (PCB* pcb );
void agregar_lista_ready(PCB* pcb);
void ready_FIFO(PCB* pcb_nuevo);
void agregar_running();


//GESTIÓN DE PCB-----------------------------------------------------------------
void cambiar_estado_pcb(PCB* pcb, estado nuevoEstado);


//GESTIÓN DE LAS CPUS-----------------------------------------------------------------
void* atender_nuevo_cliente(void* argumento);
void atender_cpu();



//GESTIÓN DE HILOS -------------------------------------------------------------
void* hilo_quantum(void* arg);


//GESTIÓN DE LAS IO-----------------------------------------------------------
io_stdint(int TAMAÑO);
io_stdout();
io_sleep();




#endif /* SERVER_H_ */
