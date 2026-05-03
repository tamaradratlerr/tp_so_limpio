#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include "utils.h"
#include <pthread.h>   // Para los Mutex y Hilos
#include <semaphore.h> // Para los Semáforos (sem_wait, sem_post)

/* Variables Globales*/

listas_procesos* listasProcesos; //Lista de PCBs segun estado
l_suplementarias* l_suple; //Lista de CPUs y IOs

sem_t sem_procesos_ready; //Semaforo para lista de READYS
sem_t sem_cpus_libres; //Semaforo para lista de CPUs
pthread_mutex_t mutex_ready; // ????????????

/*Funciones de SERVIDOR*/
void iterator(char* value);

listas_procesos* Iniciar_listas_procesos (void);

void terminar_listas_procesos (listas_procesos* listas);

PCB* iniciar_pcb (int PID, int PPID, int UID );

void terminar_pcb (PCB* pcb);



/*Funciones de Planificador*/

listas_procesos* Iniciar_listas_procesos (void); //Funcion que inicializa todas las listas de los Procesos//
void terminar_listas_procesos ();//Funcion que elimina todas las listas de los procesos//

void agregar_proceso_lista (PCB* pcb); //Funcion que a suma un PCB a su lista correspondiente segun su ESTADO ACTUAL.
void eliminar_proceso_Lista (PCB* pcb ); //Funcion que elimina un PCB de su lista correspondiente segun su ESTADO ANTERIOR.


void cambiar_estado_pcb(PCB* pcb, estado nuevoEstado); //Funcion Cambia el ESTADO ACTUAL y ESTADO ANTERIOR de un PCB


void agregar_lista_ready(PCB* pcb); //Funcion que agrega PCB a lista READY segun el ALGORITMO PLANIFICADOR

void ready_FIFO(PCB* pcb_nuevo); //Funcion que agrega a lista READY para FIFO


void iniciar_listas_suple ();
void eliminar_listas_suple ();







void manejar_vuelta_proceso_cpu(PCB* pcb);


void atender_cpu();


void agregar_running();




io_stdint(int TAMAÑO);

io_stdout();

io_sleep();



#endif /* SERVER_H_ */
