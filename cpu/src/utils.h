#ifndef UTILS_H_
#define UTILS_H_

#include "../../utils/src/global_utils.h"



typedef struct {
    uint32_t pc;
    // Registros de 8 bits (uint8_t)
    uint8_t ax, bx, cx, dx;
    // Registros de 32 bits (uint32_t)
    uint32_t eax, ebx, ecx, edx;
    uint32_t si, di;
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
    SOLICITUD_INSTRUCCION,
    LEER_MEMORIA,
    ESCRIBIR_MEMORIA

}op_code;

#endif /* UTILS_H_ */