#ifndef UTILS_H_
#define UTILS_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<commons/config.h>
#include<string.h>
#include<assert.h>
#include<pthread.h>
#include "../../utils/src/global_utils.h" 

typedef struct {
    uint32_t memory_size;   //Tamaño del memory stick en bytes
    void* memory;           // Memoria asignada (malloc)
    t_list* cpus_conectadas;// Lista de CPUs conectadas
} t_memory_stick_globals;

extern t_log* logger;
extern t_memory_stick_globals ms_globals;

void init_memory_stick(uint32_t);
void* atender_cliente(void*);
void free_all_globals(void);

void escribir_en_bloque_memoria(uint32_t dir_fisica, void* datos_a_escribir, uint32_t tamanio);
void* leer_de_bloque_memoria(uint32_t dir_fisica, uint32_t tamanio);

#endif /* UTILS_H_ */