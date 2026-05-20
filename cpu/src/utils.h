#ifndef UTILS_H_
#define UTILS_H_

#include "../../utils/src/global_utils.h"



typedef struct {
    int pid;
    uint32_t pc;
    uint32_t registros[4]; // EAX, EBX, ECX, EDX, ... --- poner los que faltan
} t_contexto;

typedef enum {
    NOOP, SET, SUM, SUB, JNZ, COPY_MEM, MOV_IN, MOV_OUT,
    MUTEX_CREATE, MUTEX_LOCK, MUTEX_UNLOCK, 
    MEM_ALLOC, MEM_FREE, SLEEP, 
    STDOUT, STDIN, INIT_PROC, EXIT
} t_instruccion_code;

typedef struct {
    t_instruccion_code codigo;
    char* params[3]; // en los ejemplos ponían como mucho 3 parámetros
    int cant_params;
} t_instruccion;

typedef enum{

    //solicitud para km
    SOLICITUD_INSTRUCCION

}op_code;

#endif /* UTILS_H_ */