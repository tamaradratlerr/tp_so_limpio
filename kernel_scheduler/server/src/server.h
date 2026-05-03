#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include "utils.h"
#include <pthread.h>   // Para los Mutex y Hilos
#include <semaphore.h> // Para los Semáforos (sem_wait, sem_post)

void iterator(char* value);

listas_procesos* Iniciar_listas_procesos (void);

void terminar_listas_procesos (listas_procesos* listas);

PCB* iniciar_pcb (int PID, int PPID, int UID );

void terminar_pcb (PCB* pcb);




//Funcion que inicializa todas las listas de los Procesos//
listas_procesos* Iniciar_listas_procesos (void);

listas_procesos* listasProcesos;


void terminar_listas_procesos ();


//Funcion que a suma un PCB a su lista correspondiente segun su ESTADO ACTUAL.
void agregar_proceso_lista (PCB* pcb);

//Funcion que inicia un nuevo PCB en estado NEW en utilsKS


//Funcion que termina un pcb liberando su memoria en utils KS

//funcion CAmbia estado pcb
void cambiar_estado_pcb(PCB* pcb, estado nuevoEstado);




//PLANIFICACION

void agregar_lista_ready(PCB* pcb);





sem_t sem_procesos_ready;
sem_t sem_cpus_libres;

pthread_mutex_t mutex_ready;


//de new a ready fifo
void agregar_a_ready_FIFO(PCB* pcb_nuevo);

void manejar_vuelta_proceso_cpu(PCB* pcb);


void atender_cpu();


void agregar_running();




io_stdint(int TAMAÑO);

io_stdout();

io_sleep();



#endif /* SERVER_H_ */
