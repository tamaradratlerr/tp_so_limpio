#ifndef UTILS_H_
#define UTILS_H_

#include <commons/string.h>
#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<commons/log.h>
#include <ctype.h>

#include<commons/collections/list.h>
#include "../../utils/src/global_utils.h"

typedef enum {
    NOOP, SET, SUM, SUB, JNZ, COPY_MEM, MOV_IN, MOV_OUT,
    MUTEX_CREATE, MUTEX_LOCK, MUTEX_UNLOCK, 
    MEM_ALLOC, MEM_FREE, SLEEP, 
    STDOUT, STDIN, INIT_PROC, EXIT_PROC
} t_instruccion_code;

typedef struct {
    t_instruccion_code codigo;
    char* params[10]; // en los ejemplos ponían como mucho 3 parámetros
    int cant_params;
} t_instruccion;

typedef struct {

    uint32_t tiempo_instruccion;
    uint32_t tam_max_segmento;

}t_config_cpu;

#endif /* UTILS_H_ */