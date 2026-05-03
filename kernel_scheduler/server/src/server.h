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

void agregar_proceso_lista (PCB* pcb, listas_procesos* l_procesos);

PCB* iniciar_pcb (int PID, int PPID, int UID );

void terminar_pcb (PCB* pcb);

#endif /* SERVER_H_ */
